#include "PropertyMatchEditor.h"
#include "ResScanner.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "FResScannerModule"

void SPropertyMatchEditor::Construct(const FArguments& InArgs, UPropertyMatchRuleExecutor* InPropertyRuleExecutor)
{
	PropertyRuleExecutor = InPropertyRuleExecutor;
	// MakeShared 直接构造一个 TSharedPtr 对象，生命周期自控
	// 会创建并拥有一个对象，返回一个 TSharedPtr<T>，不需要手动 new
	// 对象的生命周期完全由 TSharedPtr 自动管理
	// TSharedPtr<FMyObject> Ptr = MakeShared<FMyObject>(ConstructorArg1, ConstructorArg2); 完全等价于
	// TSharedPtr<FMyObject> Ptr = TSharedPtr<FMyObject>(new FMyObject(ConstructorArg1, ConstructorArg2));
	// 但是 MakeShared 更高效，因为 MakeShared 内部可以优化内存分配（对象和引用计数在一起分配）
	MatchModeOptions.Add(MakeShared<FString>(TEXT("Equal")));
	MatchModeOptions.Add(MakeShared<FString>(TEXT("NotEqual")));

	if (!PropertyRuleExecutor.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[PropertyMatchEditor::Construct] InPropertyRuleExecutor is null"))
		return;
	}

	// 初始化 RuleItems
	// TODO: 使用成员变量，避免局部变量销毁
	RuleItems.Empty();
	for (FPropertyRule& Rule : PropertyRuleExecutor->RuleData.PropertyRules)
	{
		TSharedPtr<FPropertyRule> NewItem = MakeShared<FPropertyRule>(Rule);
		// 动态生成属性下拉列表
		UpdatePropertyOptions(NewItem->TargetClass.IsValid() ? NewItem->TargetClass.LoadSynchronous() : nullptr,
			NewItem->LocalPropertyOptions);
		if (NewItem->TargetClass.IsValid() && !NewItem->PropertyName.IsNone())
		{
			// 更新类名，属性名
			UClass* TargetClass = NewItem->TargetClass.LoadSynchronous();
			FProperty* Prop = FindFProperty<FProperty>(TargetClass, NewItem->PropertyName);
			// 如果是类似 TEnumsAsByte<enum TextureCompressionSettings> 的属性，存储枚举值
			if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
			{
				UEnum* Enum = ByteProp->Enum;
				if (Enum)
				{
					for (int32 i = 0; i < Enum->NumEnums(); ++i)
					{
						NewItem->EnumValueOptions.Add(MakeShared<FString>(Enum->GetNameStringByIndex(i)));
					}
					UE_LOG(LogResScanner, Log, TEXT("[Construct] Initialized EnumValueOptions %d for %s"),
						Enum->NumEnums(), *NewItem->PropertyName.ToString());
				}
			}
		}
		
		// TODO: MakeShareable 是把通过 new 创建的裸指针转换为一个 TSharedPtr
		// 需要确保这个裸指针没有被其他地方管理，避免 double delete
		// 常用于接口回调中需要提前构造对象、或者外部库返回裸指针的情况
		// 但是这么做是用了临时对象，后面对 RuleItems 的修改，不会修改 PropertyRuleExecutor 中的 数据，不是我们想要的
		// RuleItems.Add(MakeShareable<FPropertyRule>(&Rule));

		// 需要使用 深拷贝
		// TODO: 使用 MakeShared<FPropertyRule>(Rule) 深拷贝 FPropertyRule，避免 MakeShareable(&Rule) 的悬空指针
		RuleItems.Add(MakeShared<FPropertyRule>(Rule));
	}
	ChildSlot
	[
		SNew(SVerticalBox)
		//------------------------------------------------ 反向匹配 -----------------------------------------------//
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]()			// 这里只根据 bReverseCheck 来判断是否选中
			{
				return PropertyRuleExecutor->RuleData.bReverseCheck ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)	// 根据传入的参数修改 bReverseCheck
			{
				if (PropertyRuleExecutor.IsValid())
				{
					PropertyRuleExecutor->RuleData.bReverseCheck = NewState == ECheckBoxState::Checked;
				}
			})
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ReverseCheck", "反向匹配"))
			]
		]
		//------------------------------------------------ 反向匹配 -----------------------------------------------//
		//------------------------------------------------ 属性规则 -----------------------------------------------//
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(5)
		[
			SAssignNew(PropertyRuleListView, SListView<TSharedPtr<FPropertyRule>>)
			.ListItemsSource(&RuleItems)		// 绑定 ListItemsSource 到 RuleItems
			.OnGenerateRow_Lambda([this](TSharedPtr<FPropertyRule> InItem, const TSharedRef<STableViewBase>& OwnerTable)
			{
				return OnGeneratePropertyRuleRow(InItem, OwnerTable);				// 生成属性规则行
			})
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column("Rule")
				.DefaultLabel(LOCTEXT("Rule", "属性规则"))
				.FillWidth(1.0f)
			)
		]
		//------------------------------------------------ 属性规则 -----------------------------------------------//
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SButton)
			.Text(LOCTEXT("AddRule", "添加属性规则"))
			.OnClicked(this, &SPropertyMatchEditor::OnAddPropertyRule)
		]
	];
}


/**
 * 生成属性规则行
 * @param InItem 属性规则，注意这儿传进来的 InItem 是 RuleItems 列表中的，不是 PropertyMatchRuleExecutor 中的
 * @param OwnerTable 
 * @return 
 */
TSharedRef<ITableRow> SPropertyMatchEditor::OnGeneratePropertyRuleRow(TSharedPtr<FPropertyRule> InItem, 
                                                                      const TSharedRef<STableViewBase>& OwnerTable)
{
    if (!InItem)
    {
        UE_LOG(LogTemp, Error, TEXT("[SPropertyMatchEditor::OnGeneratePropertyRuleRow] InItem is null"));
        return SNew(STableRow<TSharedPtr<FPropertyRule>>, OwnerTable);
    }
	
	// 用来存放 InItem 这条属性规则中的类 TSoftClassPtr<UObject> 的属性列表的数组
    // TArray<TSharedPtr<FName>> LocalPropertyOptions;
    // TODO: ！！！ 重要 InItem->TargetClass 是一个 SoftClassPtr
    UClass* TargetClass = InItem->TargetClass.IsValid() ? InItem->TargetClass.LoadSynchronous() : nullptr;
    // 通过 TargetClass 来获取这个类的可编辑的属性名
    // UpdatePropertyOptions(TargetClass, LocalPropertyOptions);

    return SNew(STableRow<TSharedPtr<FPropertyRule>>, OwnerTable)
        [
			// 选择垂直布局
            SNew(SBorder)
            .Padding(FMargin(5))
            .BorderImage(FCoreStyle::Get().GetBrush("Border"))
            [
                SNew(SVerticalBox)
				//---------------------------------------------- 类选择框 ---------------------------------------------//	
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(2)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0, 0, 10, 0)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("ClassLabel", "目标类"))
                        .MinDesiredWidth(100)
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
						// TODO: SObjectPropertyEntryBox 是一个 Slate 控件，用于让用户从内容浏览器中选择一个资产
	                    // 这里是 UClass 类型的资产，如 UTexture2D
						// 比如一个蓝图类、材质、贴图、声音等
	                    // 这里发现用 UObject::StaticClass 不行，改用 UClass 试试
                        SNew(SObjectPropertyEntryBox)
                        .AllowedClass(UClass::StaticClass())
	                    .AllowCreate(false)
	                    // TODO: 非常重要！！！ 这里用 WeakPtr
	                    // ObjectPath_Lambda 是一个回调函数，告诉 SObjectPropertyEntryBox 当前显示的资产路径（字符串形式，如 `/Script/Engine.Texture2D`）
	                    // 调用时机：当 SObjectPropertyEntryBox 需要初始化或刷新 UI 时（例如创建小部件、渲染、重绘）
	                    //			每次 Slate 需要显示当前选择的资产路径时（例如在下拉的文本框中显示 `Texture2D`）
	                    // 返回值：	返回一个 FString, 表示资产的路径（FSoftObjectPath::GetAssetPathString()）
	                    //			如果路径为空 (FString()) ，下拉框显示为空或占位文本
	                    // 为什么会被频繁调用？
                    	//			Slate 的渲染机制会导致 ObjectPath_Lambda 被频繁调用（如每帧重绘、窗口调整、鼠标悬停）
						.ObjectPath_Lambda([this, WeakItem = TWeakPtr<FPropertyRule>(InItem)]()		// 生成时要做的
                        {
							// WeakPtr 转换为 TSharedPtr
							if (TSharedPtr<FPropertyRule> PinnedItem = WeakItem.Pin())
							{
								return OnObjectPropertyEntryBoxCreated(PinnedItem);
							}
							UE_LOG(LogTemp, Warning, TEXT("[ObjectPath_Lambda] WeakItem is invalid"));
							return FString();
                        })
	                    // AssetData 是 SObjectPropertyEntryBox 中有的属性
						.OnObjectChanged_Lambda([this, WeakItem = TWeakPtr<FPropertyRule>(InItem)](const FAssetData& AssetData) // 改变时要做的
                        {
							if (TSharedPtr<FPropertyRule> PinnedItem = WeakItem.Pin())
							{
								OnObjectPropertyEntryBoxChanged(PinnedItem, AssetData);
							}
							else
							{
								UE_LOG(LogTemp, Warning, TEXT("[ObjectChanged_Lambda] WeakItem is invalid"));
							}
                        })
                    ]
                ]
				//------------------------------------------------ 类选择框 -----------------------------------------------//
				//------------------------------------------------ 属性选择 -----------------------------------------------//
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(2)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0, 0, 10, 0)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("PropertyLabel", "属性"))
                        .MinDesiredWidth(100)
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
                        SNew(SComboBox<TSharedPtr<FName>>)
                        .OptionsSource(&InItem->LocalPropertyOptions)
                        .OnGenerateWidget_Lambda([](TSharedPtr<FName> Option)
                        {
                        	// TODO: 这里有问题，因为之前 的 LocalPropertyOptions 一直为空所以崩溃
                        	if (Option.Get()->IsNone())
                        	{
                        		UE_LOG(LogTemp, Warning, TEXT("[OnGenerateWidget_Lambda] Option is None"))
                        		return SNew(STextBlock).Text(LOCTEXT("None", "无"));
                        	}
                            return SNew(STextBlock).Text(FText::FromName(*Option));
                        })
                        .OnSelectionChanged_Lambda([this, WeakItem = TWeakPtr<FPropertyRule>(InItem)](TSharedPtr<FName> NewSelection, ESelectInfo::Type)
                        {
                        	if (WeakItem.Pin())
                        	{
                        		OnPropSelectionChanged(WeakItem.Pin(), NewSelection);	
                        	}                            
                        })
                        [
                            SNew(STextBlock)
                            .Text(FText::FromName(InItem->PropertyName))
                        ]
                    ]
                ]
				//------------------------------------------------ 属性选择 -----------------------------------------------//
				//------------------------------------------------ 匹配模式 -----------------------------------------------//
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(2)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0, 0, 10, 0)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("MatchModeLabel", "匹配模式"))
                        .MinDesiredWidth(100)
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
                        SNew(STextComboBox)
                        .OptionsSource(&MatchModeOptions)
                        .OnSelectionChanged_Lambda([this, WeakItem = TWeakPtr<FPropertyRule>(InItem)](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
                        {
                        	if (NewSelection.IsValid() && WeakItem.IsValid())
                        	{
                        		TSharedPtr<FPropertyRule> PinnedItem = WeakItem.Pin();
                        		PinnedItem->MatchMode = NewSelection->Equals(TEXT("Equal")) ? EPropertyMatchMode::Equal : EPropertyMatchMode::NotEqual;
								SyncRuleItemsToPropertyRules();		// 无需刷新
                        	}
                        	else
                        	{
                        		UE_LOG(LogTemp, Warning, TEXT("[OnSelectionChanged_Lambda] NewSelection or WeakItem is invalid"));
                        	}
                        })
	                    .InitiallySelectedItem(GetInitialMatchOption(*InItem)) // 绑定初始值
                    ]
                ]
				//------------------------------------------------ 匹配模式 -----------------------------------------------//
				//------------------------------------------------ 创建属性值 -----------------------------------------------//
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(2)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0, 0, 10, 0)
                    [
                        SNew(STextBlock)
						.Text(LOCTEXT("ValueLabel", "值"))
                        .MinDesiredWidth(100)
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
                        CreateValueEditor(*InItem)
                    ]
                ]
				//------------------------------------------------ 创建属性值 -----------------------------------------------//
				//------------------------------------------------ 删除规则 -----------------------------------------------//
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(2)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("Remove", "删除"))
                        .OnClicked_Lambda([this, WeakItem = TWeakPtr<FPropertyRule>(InItem)]()
                        {
                        	if (TSharedPtr<FPropertyRule> PinnedItem = WeakItem.Pin())
                        	{
                        		return OnRemovePropertyRule(PinnedItem.Get());	
                        	}
                            UE_LOG(LogResScanner, Warning, TEXT("[DeleteRule][OnClicked_Lambda] PinnedItem is invalid"));
                        	return FReply::Handled();
                        })
                    ]
                ]
				//------------------------------------------------ 删除规则 -----------------------------------------------//
            ]
        ];
}

// 选择了某个属性值的回调
void SPropertyMatchEditor::OnPropSelectionChanged(TSharedPtr<FPropertyRule> InPropertyRule, TSharedPtr<FName> NewSelection)
{
	if (NewSelection.IsValid() && !NewSelection->IsNone())
	{
		InPropertyRule->PropertyName = *NewSelection;
		UE_LOG(LogTemp, Log, TEXT("[OnPropSelectionChanged] NewSelection is %s"), *NewSelection->ToString());
	}
	else
	{
		InPropertyRule->PropertyName = NAME_None;
		UE_LOG(LogTemp, Warning, TEXT("[OnPropSelectionChanged] NewSelection is None"));
	}
	InPropertyRule->PropertyValue = FString();
	SyncRuleItemsToPropertyRules();
	RefreshList();
}

// 创建 ObjectPropertyEntryBox 回调
FString SPropertyMatchEditor::OnObjectPropertyEntryBoxCreated(TSharedPtr<FPropertyRule> InItem)
{
	static bool bLoggedWarning = false; // 避免重复日志
	static bool bLoggedWarning2 = false; // 避免重复日志
	static bool bLoggedWarning3 = false; // 避免重复日志
	if (!InItem.IsValid())
	{
		if (!bLoggedWarning)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OnObjectPropertyEntryBoxCreated] InItem is null"));
			bLoggedWarning = true;
		}
		return FString();
	}
	
	if (InItem->TargetClass.IsValid())
	{
		FString Path = InItem->TargetClass.ToSoftObjectPath().GetAssetPathString();
		if (!bLoggedWarning2)
		{
			UE_LOG(LogTemp, Log, TEXT("[OnObjectPropertyEntryBoxCreated] Returning path: %s"), *Path);
			bLoggedWarning2 = true;
		}
		return Path;
	}
	if (!bLoggedWarning3)
	{
		UE_LOG(LogTemp, Log, TEXT("[OnObjectPropertyEntryBoxCreated] TargetClass is invalid for Item"));
		bLoggedWarning3 = true;
	}
	return FString();
}

// 当选择了某个 UObject 之后，更新这条属性规则的成员变量
void SPropertyMatchEditor::OnObjectPropertyEntryBoxChanged(TSharedPtr<FPropertyRule> InItem, const FAssetData& AssetData)
{
	if (!InItem.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[OnObjectPropertyEntryBoxChanged] InItem is null"));
		return;
	}
	if (!PropertyRuleExecutor.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[OnObjectPropertyEntryBoxChanged] PropertyRuleExecutor is null"));
		return;
	}
	if (!AssetData.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[OnObjectPropertyEntryBoxChanged] AssetData is invalid"));
		return;
	}

	// GetAsset() 返回的是一个 UObject*
	UClass* SelectedClass = Cast<UClass>(AssetData.GetAsset());
	if (!SelectedClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OnObjectPropertyEntryBoxChanged] Invalid class from AssetData: %s"), *AssetData.GetObjectPathString());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[OnObjectPropertyEntryBoxChanged] Selected class: %s"), *SelectedClass->GetName());
	InItem->TargetClass = TSoftClassPtr<UObject>(SelectedClass);
	InItem->PropertyName = NAME_None;
	InItem->PropertyValue = FString();
	InItem->EnumValueOptions.Empty();		// 清空枚举选项
	// 注意这里就需要更新属性列表
	UpdatePropertyOptions(SelectedClass, InItem->LocalPropertyOptions);
	SyncRuleItemsToPropertyRules();
	RefreshList();		// 类变化需要刷新列表
	UE_LOG(LogTemp, Log, TEXT("[OnObjectPropertyEntryBoxChanged] Updated TargetClass: %s"), *InItem->TargetClass.ToSoftObjectPath().GetAssetPathString());
}

/**
 * 动态生成属性下拉列表
 * @param TargetClass 传入类
 * @param OutOptions 输出属性列表
 */
void SPropertyMatchEditor::UpdatePropertyOptions(UClass* TargetClass, TArray<TSharedPtr<FName>>& OutOptions)
{
	OutOptions.Empty();
	if (!TargetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[UpdatePropertyOptions] TargetClass is null"));
		return;
	}

	// 获取一个 UClass 的所有可编辑属性名
	// 遍历 TargetClass 的所有属性
	// TODO: TFieldIterator<FProperty>
	// TFieldIterator<FProperty> 是用于 反射地遍历一个类的  UProperty (成员变量)列表 的迭代器
	// 是一个模板类，`TFieldIterator<FProperty>` 会遍历所有类型为 FProperty 或其子类 如 FIntProperty, FStrProperty 的成员变量
	// 它内部会从 `TargetClass` 开始，沿着继承链向上查找，直到 UObject
	// 你可以选择是否只查找当前类的字段（通过 `EFieldIteratorFlags::ExcludeSuper`），但这里是默认包含父类字段
	for (TFieldIterator<FProperty> It(TargetClass); It; ++It)
	{
		// 如果属性有 CPF_Edit 标志
		if (It->HasAnyPropertyFlags(CPF_Edit))
		{
			OutOptions.Add(MakeShared<FName>(It->GetName()));
		}
	}
	
}



/**
 * 创建属性编辑器
 * @param PropertyRule 
 * @return 
 */
TSharedRef<SWidget> SPropertyMatchEditor::CreateValueEditor(FPropertyRule& PropertyRule)
{
	UE_LOG(LogTemp, Log, TEXT("[CreateValueEditor] PropertyName: %s"), *PropertyRule.PropertyName.ToString());
	
	if (PropertyRule.PropertyName == NAME_None || !PropertyRule.TargetClass.IsValid())
	{
		return SNew(STextBlock).Text(LOCTEXT("NoPorpertySelected", "未选择属性"));
	}

	UClass* TargetClass = PropertyRule.TargetClass.LoadSynchronous();
	FProperty* Prop = FindFProperty<FProperty>(TargetClass, PropertyRule.PropertyName);
	if (!Prop)
	{
		UE_LOG(LogTemp, Error, TEXT("Property %s not found in class %s"), *PropertyRule.PropertyName.ToString(), *TargetClass->GetName());
		return SNew(STextBlock).Text(LOCTEXT("InvalidProperty", "无效属性"));
	}
	UE_LOG(LogTemp, Log, TEXT("[CreateValueEditor] PropertyType: %s"), *Prop->GetClass()->GetName());

	// TODO: 重点： 根据 FProperty 的类型进行不同的处理
	// FBoolProperty, FEnumProperty, FObjectProperty,
	// 没处理的：FNumericProperty, FStructProperty, FOptionalProperty, FMulticastDelegateProperty
	// 1. 处理 FBoolProperty
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		UE_LOG(LogTemp, Log, TEXT("[CreateValueEditor] Property is a bool"));
		return SNew(SCheckBox)
			.IsChecked_Lambda([&PropertyRule]()
			{
				return PropertyRule.PropertyValue.Equals(TEXT("true")) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this, &PropertyRule](ECheckBoxState NewState)
			{
				PropertyRule.PropertyValue = NewState == ECheckBoxState::Checked ? TEXT("true") : TEXT("false");
				SyncRuleItemsToPropertyRules();
			});
	}
	// 2. 处理 FEnumProperty
	else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		UE_LOG(LogTemp, Log, TEXT("[CreateValueEditor] Property is an enum"));		
		UEnum* Enum = EnumProp->GetEnum();
		return CreateEnumPropertyEditor(Enum, PropertyRule);
	}
	// 3. 处理 FByteProperty (TEnumAsByte)
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
	{
		UE_LOG(LogTemp, Log, TEXT("[CreateValueEditor] Property type is FByteProperty"));
		UEnum* Enum = ByteProp->Enum;
		return CreateEnumPropertyEditor(Enum, PropertyRule);
	}
	// 4. 处理 FNumericProperty (如 int32, float)
	else if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Prop))
	{
		UE_LOG(LogTemp, Log, TEXT("[CreateValueEditor] Property is a numeric"));
		return SNew(SNumericEntryBox<float>)
			.Value_Lambda([&PropertyRule]()
			{
				float Value = 0.0f;
				LexFromString(Value, *PropertyRule.PropertyValue);
				return Value;
			})
			.OnValueChanged_Lambda([this, &PropertyRule](float NewValue)
			{
				PropertyRule.PropertyValue = FString::Printf(TEXT("%f"), NewValue);
				SyncRuleItemsToPropertyRules();
			});
	}
	// 5. 处理 FObjectProperty
	else if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Prop))
	{
		UE_LOG(LogTemp, Log, TEXT("[CreateValueEditor] Property is an object"))
		// SObjectPropertyEntryBox 用来可视化、选择和编辑基于 UObject 的资产引用（如纹理、材质、静态网格等）
		return SNew(SObjectPropertyEntryBox)
			.AllowedClass(ObjectProp->PropertyClass)
			.ObjectPath_Lambda([PropertyRule]()
			{
				return PropertyRule.PropertyValue;
			})
			.OnObjectChanged_Lambda([this, &PropertyRule](const FAssetData& AssetData)
			{
				// 实际上是把 UObject 的 path 赋给 PropertyRule.PropertyValue
				PropertyRule.PropertyValue = AssetData.GetObjectPathString();
				SyncRuleItemsToPropertyRules();
			});
	}
	// 非 bool, Enum, Object 的属性
	// TODO: 将来这里可能需要处理更多类型的属性
	UE_LOG(LogTemp, Log, TEXT("[CreateValueEditor] Property is other types"));
	return SNew(SEditableTextBox)
		.Text(FText::FromString(PropertyRule.PropertyValue))
		.OnTextChanged_Lambda([this, &PropertyRule](const FText& NewText)
		{
			PropertyRule.PropertyValue = NewText.ToString();
			SyncRuleItemsToPropertyRules();
		});
}


// 获取已经选取的属性值（如果没有选择，就给第一个）
TSharedPtr<FString> SPropertyMatchEditor::GetInitialEnumOption(FPropertyRule& PropertyRule)
{
	// 如果已经设置了值，就返回对应的选项
	if (!PropertyRule.PropertyValue.IsEmpty() && !PropertyRule.EnumValueOptions.IsEmpty())
	{
		for (TSharedPtr<FString> Option : PropertyRule.EnumValueOptions)
		{
			if (Option->Equals(PropertyRule.PropertyValue))
			{
				// 注意这里也要 Sync
				SyncRuleItemsToPropertyRules();
				return Option;
			}
		}
	}
	// 如果没有设置值，就返回第一个选项，注意这里也要 Sync，因为 PropertyRule 里面还没值，给了默认的用户可能不选择直接使用
	PropertyRule.PropertyValue = *PropertyRule.EnumValueOptions[0];
	SyncRuleItemsToPropertyRules();
	return PropertyRule.EnumValueOptions.Num() > 0 ? PropertyRule.EnumValueOptions[0] : nullptr;
}

TSharedPtr<FString> SPropertyMatchEditor::GetInitialMatchOption(const FPropertyRule& PropertyRule)
{
	return PropertyRule.MatchMode == EPropertyMatchMode::Equal ? MakeShared<FString>(TEXT("Equal"))
		: MakeShared<FString>(TEXT("NotEqual"));
}

// 创建 Enum 属性编辑器，返回一个 SWidget
TSharedRef<SWidget> SPropertyMatchEditor::CreateEnumPropertyEditor(UEnum* Enum, FPropertyRule& PropertyRule)
{
	if (Enum)
	{ 
		// 根据 EnumProperty 获取 枚举属性 的字符串值，放到数组中
        // 这里把枚举值放在临时数组中，而是放在 PropertyRule 对象中
		PropertyRule.EnumValueOptions.Empty();
			 
		for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
		{
			PropertyRule.EnumValueOptions.Add(MakeShared<FString>(Enum->GetNameStringByIndex(i)));
		}

		if (PropertyRule.EnumValueOptions.Num() > 0)
		{
			return SNew(STextComboBox)
			.OptionsSource(&PropertyRule.EnumValueOptions)
			.OnSelectionChanged_Lambda([this, &PropertyRule](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
			{
				if (NewSelection.IsValid())
				{
					PropertyRule.PropertyValue = *NewSelection;
					UE_LOG(LogResScanner, Log, TEXT("[CreateEnumPropertyEditor] Selected enum value: %s"), *PropertyRule.PropertyValue);
					SyncRuleItemsToPropertyRules();
				}
			})
			// 根据已经存储的 PropertyValue 从 EnumValueOptions 列表中获取初始值
			.InitiallySelectedItem(GetInitialEnumOption(PropertyRule));		// 设置初始值，注意这里不能硬编码写死，因为可能是别的值（并非只有新建规则会走到这里）
		}
		UE_LOG(LogResScanner, Log, TEXT("[CreateEnumPropertyEditor] PropertyRule.EnumValueOptions 为空: %s"), *PropertyRule.PropertyValue);
		return SNew(STextBlock).Text(LOCTEXT("NoEnumOptions", "无可用枚举项"));
	}
	UE_LOG(LogResScanner, Log, TEXT("[CreateEnumPropertyEditor] Enum does not exist! :  %s"), *PropertyRule.PropertyValue);
	return SNew(STextBlock).Text(LOCTEXT("NoEnumValues", "无枚举值"));
}

/**
 * 添加默认属性规则
 * @return 
 */
FReply SPropertyMatchEditor::OnAddPropertyRule()
{
	if (PropertyRuleExecutor.IsValid())
	{
		FPropertyRule NewRule;
		NewRule.MatchMode = EPropertyMatchMode::Equal;		// 默认值
		PropertyRuleExecutor->RuleData.PropertyRules.Add(NewRule);
		RuleItems.Add(MakeShared<FPropertyRule>(NewRule));
		UE_LOG(LogTemp, Log, TEXT("Added Property Rule, Total: %d"), PropertyRuleExecutor->RuleData.PropertyRules.Num());
		RefreshList();
	}
	return FReply::Handled();
}

/**
 * 删除规则
 * @param PropertyRule 
 * @return 
 */
FReply SPropertyMatchEditor::OnRemovePropertyRule(FPropertyRule* PropertyRule)
{
	if (PropertyRuleExecutor.IsValid() && PropertyRule)
	{
		// 遍历 PropertyRuleExecutor 规则列表中的规则，找到这条 PropertyRule 并删除
		int32 index = 0;
		for (index = 0; index < RuleItems.Num(); ++index)
		{
			// // TODO: 这里写的时候第一时间没想到，只是写 PropertyRuleExecutor->RuleData.PropertyRules[i] 和 PropertyRule 作比较
			// // 实际上是 FPropertyRule 和 FPropertyRule 作比较
			// // 对前者取地址不就得到指针了吗，所以直接 &PropertyRules[i] 得到的就是 FPropertyRule*
			// if (&PropertyRuleExecutor->RuleData.PropertyRules[index] == PropertyRule)
			// {
			// 	PropertyRuleExecutor->RuleData.PropertyRules.RemoveAt(index);
			// 	break;
			// }

			if (RuleItems[index].IsValid() && RuleItems[index].Get() == PropertyRule)
			{
				RuleItems.RemoveAt(index);
				PropertyRuleExecutor->RuleData.PropertyRules.RemoveAt(index);
				UE_LOG(LogTemp, Log, TEXT("Removed Property Rule, Total: %d"), PropertyRuleExecutor->RuleData.PropertyRules.Num());
				RefreshList();
				return FReply::Handled();
			}
		}
		// 如果遍历完了还没找到，打印失败 log
		if (index == PropertyRuleExecutor->RuleData.PropertyRules.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to remove property rule"));
			return FReply::Handled();
		}
		RefreshList();
	}
	return FReply::Handled();
}

/**
 * 刷新列表，非必要不调用
 */
void SPropertyMatchEditor::RefreshList()
{
	if (PropertyRuleListView.IsValid())
	{
		PropertyRuleListView->RebuildList();		// 强制重建列表
	}
}

void SPropertyMatchEditor::SyncRuleItemsToPropertyRules()
{
	if (!PropertyRuleExecutor.IsValid()) return;
	
	PropertyRuleExecutor->RuleData.PropertyRules.Empty();
	for (const TSharedPtr<FPropertyRule>& RuleItem : RuleItems)
	{
		if (RuleItem.IsValid())
		{
			PropertyRuleExecutor->RuleData.PropertyRules.Add(*RuleItem);
		}
	}
}

#undef LOCTEXT_NAMESPACE
