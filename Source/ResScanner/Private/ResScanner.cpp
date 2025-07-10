// Copyright Epic Games, Inc. All Rights Reserved.

#include "ResScanner.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "ResScannerStyle.h"
#include "ResScannerCommands.h"
#include "NameMatchRuleExecutor.h"
#include "ResScannerRuleBase.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IDetailsView.h"
#include "JsonObjectConverter.h"
#include "PropertyEditorModule.h"
#include "PropertyMatchEditor.h"
#include "PropertyMatchRuleExecutor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"


// 定义插件的标签名称
static const FName ResScannerTabName("ResScanner");

// 本地化命名空间定义，后续使用 LOCTEXT 宏时可用
#define LOCTEXT_NAMESPACE "FResScannerModule"

DEFINE_LOG_CATEGORY(LogResScanner)

void FResScannerModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// 手动初始化共享指针系统
	//SharedThis = TSharedPtr<FResScannerModule>(this);

	// 初始化插件样式（例如按钮图标，Slate 样式等）
	FResScannerStyle::Initialize();
	FResScannerStyle::ReloadTextures();

	// 注册插件命令（快捷键或按钮绑定）
	FResScannerCommands::Register();

	// 创建命令列表对象
	PluginCommands = MakeShareable(new FUICommandList);

	// 将 “打开插件窗口” 命令绑定到自定义函数 PluginButtonClicked
	PluginCommands->MapAction(
		FResScannerCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FResScannerModule::PluginButtonClicked),
		FCanExecuteAction());

	// 注册菜单扩展回调函数，在 ToolMenus 初始化完成后调用 RegisterMenus
	// RegisterMenus 用来注册插件工具栏和工具栏按钮
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FResScannerModule::RegisterMenus));

	// 向全局 Tab 管理器注册一个 Nomad (独立)窗口，用于打开插件 UI 面板
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ResScannerTabName, FOnSpawnTab::CreateRaw(this, &FResScannerModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FResScannerTabTitle", "ResScanner"))		// Tab 显示标题
		.SetMenuType(ETabSpawnerMenuType::Hidden);		// 不在菜单中显示，手动调用打开

	// 初始化规则集
	RuleSet = NewObject<UResScannerRuleSet>();
}

// 插件模块关闭函数
void FResScannerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	// 取消菜单注册回调（防止插件卸载后仍被调用）
	UToolMenus::UnRegisterStartupCallback(this);

	// 取消该插件作为菜单项所有者（清理菜单）
	UToolMenus::UnregisterOwner(this);

	// 清理样式资源
	FResScannerStyle::Shutdown();

	// 取消命令注册
	FResScannerCommands::Unregister();

	// 取消插件窗口的 Tab 注册
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ResScannerTabName);

	RuleItems.Empty();
	RuleSet = nullptr;
}

// 生成主界面对应的 Dockable Tab 面板
TSharedRef<SDockTab> FResScannerModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	// 插件窗口内显示的文字内容
	ScanResults.Empty();

	// 创建插件窗口 Tab，并填充一个简单的文本控件
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)			// 表示是独立窗口
		[
			// 左侧用于显示规则配置， 右侧用于显示扫描结果
			SNew(SSplitter)				// 使用 SSplitter 分割左右区域
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(0.4f)	// 左侧占 40%
			[
				// ----------左侧用于显示规则配置------------
				CreateRuleConfigPanel()
			]
			+ SSplitter::Slot()
			 .Value(0.6f)
			 [
				 // ----------右侧用于显示扫描结果------------
				 CreateScanResultsPanel()
			 ]
		];
}

// 规则配置面板
// 之前这里一直崩溃，后面把 OnRuleSelectionChanged 抽取出来，就正常了
TSharedRef<SWidget> FResScannerModule::CreateRuleConfigPanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("RuleConfigTitle", "规则配置"))
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()		// 添加规则类型选择
			.AutoWidth()
			.Padding(2)
			[
				SNew(SComboBox<TSharedPtr<FString>>)	// ComboBox 是下拉选择框
				.OptionsSource(&RuleTypeOptions)		// 规则类型
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
				{
					return SNew(STextBlock).Text(FText::FromString(*InItem));
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
				{
					OnRuleSelectionChanged(NewSelection);
				})
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return SelectedRuleType.IsValid() ? FText::FromString(*SelectedRuleType) : LOCTEXT("SelectRuleType", "选择规则类型");
					})
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(RuleListView, SListView<TWeakObjectPtr<UResScannerRuleBase>>)
				.ListItemsSource(&RuleItems)
				.OnGenerateRow_Lambda([this](TWeakObjectPtr<UResScannerRuleBase> InItem, const TSharedRef<STableViewBase>& OwnerTable)
				{
					return OnGenerateRuleRow(InItem.Get(), OwnerTable);		// 显示规则列表
				})
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column("RuleName")
					.DefaultLabel(LOCTEXT("RuleName", "规则名称"))
				)
			]
		];
}

// 当选择了下拉选择框中某一条规则
void FResScannerModule::OnRuleSelectionChanged(TSharedPtr<FString> NewSelection)
{
	// 选择之后添加到规则集中
	if (NewSelection.IsValid() && RuleSet)
	{
		UResScannerRuleBase* NewRule = nullptr;
		if (NewSelection->Equals(TEXT("NameMatch")))
		{
			NewRule = NewObject<UNameMatchRuleExecutor>(RuleSet);
		}
		else if (NewSelection->Equals(TEXT("PropertyMatch")))
		{
			NewRule = NewObject<UPropertyMatchRuleExecutor>(RuleSet);
		}
		
		// TODO: PathMatch, PropertyMatch
		if (NewRule)
		{
			// 添加进 RuleSet 和 RuleItems
			RuleSet->Rules.Add(NewRule);
			// 注意：绝对不能写 RuleItems.Add(MakeShareable(NewRule)); !!!
			// 原来 RuleItems 的声明是 TArray<TSharedPtr<UResScannerRuleBase>>，这是一种错误的写法！！！
			// 原因：1. MakeShareable() 创建了独立的强引用计数控制块
			// 2. UE 的垃圾回收系统 GC 完全不知道此引用计数
			// 3. 当 GC 运行时，会检测对象是否该被回收，但不会检查 TSharedPtr 的引用
			// 4. 将导致 对象被意外销毁 Double Delete 崩溃
			RuleItems.Add(NewRule);
			SelectedRuleType = NewSelection;
			if (RuleListView.IsValid()) RuleListView->RequestListRefresh();
		}
	}
}

// 结果显示面板
TSharedRef<SWidget> FResScannerModule::CreateScanResultsPanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			[
				SNew(SButton)
				.Text(LOCTEXT("ScanButton", "扫描资源"))
				// 这种按钮绑定方式，函数返回值必须为 FReply
				// 要求类必须支持共享指针引用计数机制，根本原因在于 UE 的委托系统需要确保回调对象生命周期安全
				// UE 的 .OnClicked() 委托实际上调用的是 CreateSP (共享指针绑定)而不是 CreateUObject 或 CreateRaw
				// 使用 CreateSP 绑定时，UE 要求目标对象必须继承自 TSharedFromThis<T>，对象必须已经被 MakaShared 或 MakeShareable 创建
				// 所以如果这里用 OnClicked(this, &FResScannerModule::OnStartScanClicked) 绑定，就要求 FResScannerModule 必须继承 TSharedFromThis<FResScannerModule>
				// 但是这里 FResScannerModule 如果继承自 TSharedFromThis，就需要手动管理对象生命周期，而插件的生命周期由 UE 管理
				// 所以这里还是建议 OnClicked_Lambda 绑定
				// 因为 Lambda 捕获 this 指针不需要共享指针系统支持，Lambda 的生命周期由 Slate 系统管理，与 Tab 共存
				// 避免与 引擎模块管理机制冲突，Tab 关闭时会自动销毁关联的 Lambda
				.OnClicked_Lambda([this]()
				{
					return OnStartScanClicked();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			[
				SNew(SButton)
				.Text(LOCTEXT("ImportConfig", "导入配置"))
				.OnClicked_Lambda([this]()
				{
					return OnImportConfigClicked();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			[
				SNew(SButton)
				.Text(LOCTEXT("ExportConfig", "导出配置"))
				.OnClicked_Lambda([this]()
				{
					return OnExportConfigClicked();
				})
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(5)
		[
			SAssignNew(ScanResultsListView, SListView<TSharedPtr<FScanResultItem>>)
			.ListItemsSource(&ScanResults)			// 注意这里给进来的是一个指针
			.OnGenerateRow_Lambda([this](TSharedPtr<FScanResultItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
			{
				return OnGenerateResultRow(InItem, OwnerTable);				// 显示结果列表
			})
			.HeaderRow(
				SNew(SHeaderRow)
				+ SHeaderRow::Column("AssetName")
				.DefaultLabel(LOCTEXT("AssetName", "资源名称"))
				.FillWidth(0.6f)
				+ SHeaderRow::Column("AssetPath")
				.DefaultLabel(LOCTEXT("AssetPath", "资源路径"))
				.FillWidth(0.2f)
				+ SHeaderRow::Column("ErrorReason")
				.DefaultLabel(LOCTEXT("ErrorReason", "错误原因"))
				.FillWidth(0.2f)
			)
		];
}

// 生成规则行
TSharedRef<ITableRow> FResScannerModule::OnGenerateRuleRow(UResScannerRuleBase* InItem,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	if (!InItem) return SNew(STableRow<TSharedPtr<UResScannerRuleBase>>, OwnerTable);
	return SNew(STableRow<TSharedPtr<UResScannerRuleBase>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InItem->GetClass()->GetName()))		// 显示规则类型
			]
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("EditRule", "编辑"))
				.OnClicked_Lambda([this, InItem]()
				{
					return OnEditRuleClicked(InItem);
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RemoveRule", "删除"))
				.OnClicked_Lambda([this, InItem]()
				{
					RuleSet->Rules.Remove(InItem);
					RuleItems.Remove(InItem);
					if (RuleListView.IsValid()) RuleListView->RequestListRefresh();
					return FReply::Handled();
				})
			]
		];
}

// 扫描按钮点击事件处理函数
FReply FResScannerModule::OnStartScanClicked()
{
	RunAssetScan();

	return FReply::Handled();
}

// 导入配置按钮点击事件处理函数
FReply FResScannerModule::OnImportConfigClicked()
{
	// 获取 DesktopPlatform 模块
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TArray<FString> OutFiles;
		// 利用 DesktopPlatform 模块打开文件对话框
		bool Opened = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("导入配置"),
			FPaths::ProjectDir(),
			TEXT(""),
			TEXT("JSON Files|*.json"),
			EFileDialogFlags::None,
			OutFiles
		);
		if (!Opened || OutFiles.Num() <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("[OnImportConfigClicked] 未选择文件"));
			return FReply::Handled();
		}
		
		FString JsonString;
		// 将 Json 字符串反序列化为规则集
		// 从文件中读取 json 字符串
		if (!FFileHelper::LoadFileToString(JsonString, *OutFiles[0]))
		{
			UE_LOG(LogTemp, Error, TEXT("[OnImportConfigClicked] 文件读取失败"));
			return FReply::Handled();
		}
		
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		// 将 json 字符串反序列化为 json 对象
		if (!FJsonSerializer::Deserialize(Reader, JsonObject))
		{
			UE_LOG(LogTemp, Error, TEXT("[OnImportConfigClicked] 文件反序列化失败"));
			return FReply::Handled();
		}
	
		RuleSet->Rules.Empty();
		RuleItems.Empty();
		const TArray<TSharedPtr<FJsonValue>>* RulesArray;
		// 根据 "Rules" 字段获取 json 对象中的规则数组
		if (!JsonObject->TryGetArrayField(TEXT("Rules"), RulesArray))
		{
			UE_LOG(LogTemp, Error, TEXT("[OnImportConfigClicked] 规则数组获取失败"));
			return FReply::Handled();
		}
		
		for (const TSharedPtr<FJsonValue>& RuleValue : *RulesArray)
		{
			// TODO: 注意这里展示了 TSharedPtr 怎么转换为 TSharedRef!!!
			const TSharedRef<FJsonObject>& RuleJsonObject = RuleValue->AsObject().ToSharedRef();
			FString RuleType;
			if (RuleJsonObject->TryGetStringField(TEXT("Type"), RuleType))
			{
				UE_LOG(LogTemp, Log, TEXT("[OnImportConfigClicked] RuleType: %s"), *RuleType)
				// TODO: 这里只能写死字符串吗
				if (RuleType.Equals(TEXT("NameMatch")))
				{
					// RuleSet 作为这个 Object 的拥有者
					UNameMatchRuleExecutor* NewRule = NewObject<UNameMatchRuleExecutor>(
						RuleSet, UNameMatchRuleExecutor::StaticClass(), TEXT("NewNameRule"));
					// JsonObject, StructDefinition, OutStruct, Flag
					FJsonObjectConverter::JsonObjectToUStruct(RuleJsonObject, FNameMatchRule::StaticStruct(), &NewRule->RuleData, 0, 0);
					RuleSet->Rules.Add(NewRule);
					RuleItems.Add(NewRule);
				}
			}
		}
		if (RuleListView.IsValid())
		{
			RuleListView->RequestListRefresh();
		}
	}
	return FReply::Handled();
}

// 使用 IFileDialog 或 IDesktopPlatform 打开保存文件对话框
FReply FResScannerModule::OnExportConfigClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TArray<FString> OutFiles;
		bool Opened = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			TEXT("导出配置"),
			FPaths::ProjectDir(),
			TEXT("Config.json"),
			TEXT("JSON Files|*.json"),
			EFileDialogFlags::None,
			OutFiles
		);
		if (Opened && OutFiles.Num() > 0)
		{
			FString JsonString;
			// 调用下面的工具函数 SerializeRuleSetToJson 将规则集序列化为 Json 字符串
			if (SerializeRuleSetToJson(RuleSet, JsonString))
			{
				// 保存到文件
				FFileHelper::SaveStringToFile(JsonString, *OutFiles[0]);
			}
		}
	}
	
	return FReply::Handled();
}

// 添加新规则
FReply FResScannerModule::OnAddNewRule()
{
	UNameMatchRuleExecutor* NewRule = NewObject<UNameMatchRuleExecutor>();
	RuleSet->Rules.Add(NewRule);
	
	RuleItems.Add(NewRule);
	// 将规则列表的 SListView 赋给 RuleListView
	if (RuleListView.IsValid()) RuleListView->RequestListRefresh();
	return FReply::Handled();
}

// 新开一个窗口编辑规则
FReply FResScannerModule::OnEditRuleClicked(UResScannerRuleBase* InItem)
{
	if (!InItem) return FReply::Handled();
	
	TSharedRef<SWindow> RuleEditorWindow = SNew(SWindow)
		.Title(LOCTEXT("RuleEditor", "规则编辑器"))
		.ClientSize(FVector2D(600, 400))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	// TODO: 原来写的是 TSharedRef<SWidget> Content;
	// 编译报错：SWidget 是一个抽象类，不能直接实例化
	// 改成 TSharedPtr<SWidget>
	TSharedPtr<SWidget> Content;
	TSharedPtr<SPropertyMatchEditor> PropertyMatchEditor;		// 保存 SPropertyMatchEditor 实例
	if (UPropertyMatchRuleExecutor* PropertyRuleExecutor = Cast<UPropertyMatchRuleExecutor>(InItem))
	{
		PropertyMatchEditor = SNew(SPropertyMatchEditor, PropertyRuleExecutor);
		Content = PropertyMatchEditor;
	}
	else
	{
		// 加载 PropertyEditor 模块
		// PropertyEditor 可以显示任何 UObject 或 AActor 的可编辑属性（带有 UPROPERTY 宏的属性）
		// 根据元数据描述 (EditAnywhere, Category 等) 自动生成编辑界面
		// 例如，修改光源强度、材质属性、物理属性等
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		
		// 配置 DetailsView 参数
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bAllowMultipleTopLevelObjects = false; // 仅支持单个对象
		DetailsViewArgs.bShowObjectLabel = true; // 显示对象标签
		DetailsViewArgs.bShowScrollBar = true; // 显示滚动条
		// 创建 DetailsView（注意只能这么创建）
		TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		// 把传进来的 ResScannerRule 对象绑定到 DetailsView
		DetailsView->SetObject(InItem);		// 设置要编辑的对象
		Content = DetailsView;
	}
	
	RuleEditorWindow->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5)
			[
				SNew(SButton)
				.Text(LOCTEXT("Save", "保存"))
				// 这里 InItem 通常用裸指针就可以
				.OnClicked_Lambda([this, RuleEditorWindow, WeakItem = TWeakObjectPtr<UResScannerRuleBase>(InItem), PropertyMatchEditor]()
				{
					if (WeakItem.IsValid())
					{
						// 保存属性规则
						if (PropertyMatchEditor.IsValid())
						{
							PropertyMatchEditor.Get()->SyncRuleItemsToPropertyRules();
							UE_LOG(LogResScanner, Log, TEXT("[OnEditRuleClicked][Save] 保存属性规则"));
						}
						
						// TODO: Modify 是 UObject 中的函数，用于标记这个 UObject 将被修改
						// 调用 Modify 函数实际上做了以下几件事情：
						// 1. 判断引擎是否正在记录事务（即是否处于一个 Undoable 操作中）
						// 2. 如果是，就将这个对象当前的状态序列化进事务系统
						// 3. 后续当用户撤销或重做时，这个对象将恢复到 Modify() 调用时的状态
						WeakItem.Get()->Modify();
						if (RuleSet)
						{
							RuleSet->Modify();
						}
						FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SaveSuccess", "规则保存成功"));
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("[OnEditRuleClicked][Save] InItem is nullptr"))
					}
					RuleEditorWindow->RequestDestroyWindow();
					return FReply::Handled();
				})	
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5)
			[
				SNew(SButton)
				.Text(LOCTEXT("Cancel", "取消"))
				.OnClicked_Lambda([RuleEditorWindow]()
				{
					RuleEditorWindow->RequestDestroyWindow();
					return FReply::Handled();
				})
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(5)
		[
			Content.ToSharedRef()		// TODO：需要转换为 TSharedRef
		]
	);

	RuleEditorWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda([this](const TSharedRef<SWindow>&)
	{
		if (RuleListView.IsValid())
			RuleListView->RequestListRefresh();
	}));

	FSlateApplication::Get().AddWindow(RuleEditorWindow);	
	return FReply::Handled();
}


// 生成列表每一行
TSharedRef<ITableRow> FResScannerModule::OnGenerateResultRow(TSharedPtr<FScanResultItem> InItem,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FScanResultItem>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.6f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InItem->AssetPath))
				.OnDoubleClicked_Lambda([InItem](const FGeometry& Geometry, const FPointerEvent& PointerEvent)
				{
					// 双击跳转到资源
					FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
					FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(InItem->AssetPath);
					if (AssetData.IsValid())
					{
						GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetData.GetAsset());
					}
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.2f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InItem->RuleName))		// 显示规则名称
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.2f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InItem->ErrorReason))	// 显示错误原因
			]
		];
}

// 开始扫描资源
void FResScannerModule::RunAssetScan()
{
	// 清空旧结果
	ScanResults.Empty();

	// 获取 AssetRegistry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	// 获取注册表中的所有资源，注意这里包含了引擎中的资源
	// AssetRegistry.GetAllAssets(AssetDataList, true);

	// FName AssetPath = TEXT("/Game/FirstPersonArms/Character/Textures/Manny");
	// GetAssetsByPath 是获取某个路径下的资源
	// 用 "/Game" + GetAssetsByPath 就能获取到项目中的资源而排除引擎资源了
	FName RootPath = TEXT("/Game");
	if (!AssetRegistry.GetAssetsByPath(RootPath, AssetDataList, true, true))
	{
		UE_LOG(LogResScanner, Warning, TEXT("[RunAssetScan] Cannot get assets from path: %s"), *RootPath.ToString());
		return ;
	}

	for (const FAssetData& AssetData : AssetDataList)
	{
		for (UResScannerRuleBase* Rule : RuleSet->Rules)
		{
			if (!Rule) continue;
			bool bMatch = Rule->Match(AssetData);
			if (Rule->bReverseCheck) bMatch = !bMatch;

			if (bMatch)
			{
				TSharedPtr<FScanResultItem> Result = MakeShared<FScanResultItem>();
				// Result->AssetPath = AssetData.ObjectPath.ToString();
				Result->AssetPath = AssetData.GetObjectPathString();
				Result->RuleName = Rule->GetClass()->GetName();
				Result->ErrorReason = Rule->GetErrorReason();
				ScanResults.Add(Result);
			}
		}
	}

	if (ScanResultsListView.IsValid())
	{
		ScanResultsListView->RequestListRefresh();
	}
}

// 用户点击菜单按钮或命令时，会打开这个插件的窗口
void FResScannerModule::PluginButtonClicked()
{
	// 请求打开窗口
	FGlobalTabmanager::Get()->TryInvokeTab(ResScannerTabName);
}

// 注册插件菜单项和工具栏按钮
void FResScannerModule::RegisterMenus()
{
	// 设置当前模块为菜单项的所有者，方便后续卸载清理
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	// 在主菜单 Window -> Layout 下添加插件菜单按钮
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FResScannerCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	// 在工具栏的 Settings 区域添加按钮
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				// 初始化工具栏按钮并绑定命令
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FResScannerCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}


//-------------------------------- Util ---------------------------------
/**
 * 使用 FJsonObjectConverter 将 UResScannerRuleSet 和 FNameMatchRule 转换为 Json
 * @param InRuleSet 规则集
 * @param OutJsonStr 输出的 Json 字符串
 * @return 
 */
bool FResScannerModule::SerializeRuleSetToJson(const UResScannerRuleSet* InRuleSet, FString& OutJsonStr)
{
	// 最外层的 Json 对象
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	// 用于存放规则的 JsonValue 数组
	TArray<TSharedPtr<FJsonValue>> RulesArray;

	// 遍历规则集
	for (UResScannerRuleBase* Rule : InRuleSet->Rules)
	{
		// 创建规则的 Json 对象
		TSharedRef<FJsonObject> RuleJson = MakeShareable(new FJsonObject);
		// 根据规则类型设置 Json 中 Type 的值
		RuleJson->SetStringField("Type", Rule->ScanRuleTypeToString(Rule->GetRuleType()));
		
		if (auto NameRule = Cast<UNameMatchRuleExecutor>(Rule))
		{
			// 参数分别是 StructDefinition, Struct, OutJsonObj, CheckFlags, SkipFlags
			FJsonObjectConverter::UStructToJsonObject(FNameMatchRule::StaticStruct(), &NameRule->RuleData, RuleJson, 0, 0);
			// 添加到数组中 (FJsonValueObject 是 FJsonValue 的派生类)
			RulesArray.Add(MakeShareable(new FJsonValueObject(RuleJson)));
		}
		else if (auto PropertyRule = Cast<UPropertyMatchRuleExecutor>(Rule))
		{
			FJsonObjectConverter::UStructToJsonObject(FPropertyMatchRule::StaticStruct(), &PropertyRule->RuleData, RuleJson, 0, 0);
			RulesArray.Add(MakeShareable(new FJsonValueObject(RuleJson)));
		}
		
		// TODO: 其它类型的规则处理
	}
	JsonObject->SetArrayField("Rules", RulesArray);

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonStr);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return true;
}
//-------------------------------- Util ---------------------------------

// 取消本地命名空间定义
#undef LOCTEXT_NAMESPACE

// 声明插件模块实现，这一行告诉 Unreal 用 FResScannerModule 来实现 ResScanner 模块
IMPLEMENT_MODULE(FResScannerModule, ResScanner)