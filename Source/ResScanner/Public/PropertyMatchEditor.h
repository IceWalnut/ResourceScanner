#pragma once
#include "CoreMinimal.h"
#include "PropertyMatchRuleExecutor.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 自定义 Slate 控件，动态 UI 用于 UClass 选择、属性名下拉框、值编辑器
 * SPropertyMatchEditor 包含：
 *		SObjectPropertyEntryBox ：选择 UClass
 *		SComboBox : 选择属性名，基于反射获取
 *		STextComboBox : 选择匹配模式
 *		ValueEditor : 根据属性类型生成编辑器（如复选框、下拉框、资源选择器）
 *		UpdatePropertyOptions 函数： 使用 TFieldIterator 获取 UClass 的可编辑属性
 *		CreateValueEditor 函数: 格局 FProperty 类生成编辑器（bool，enum，object，默认文本）
 *	模块依赖：
 *		AssetTools
 */
class SPropertyMatchEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPropertyMatchEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UPropertyMatchRuleExecutor* InPropertyRuleExecutor);

	// 同步 RuleItems 中的规则到 PropertyMatchRuleExecutor 中
	void SyncRuleItemsToPropertyRules();
	

private:
	// 属性规则
	TWeakObjectPtr<UPropertyMatchRuleExecutor> PropertyRuleExecutor;
	// 属性选项
	TArray<TSharedPtr<FName>> PropertyOptions;
	// 匹配模式选项
	TArray<TSharedPtr<FString>> MatchModeOptions;

	// TODO: 属性规则显示列表
	// 为什么 SListView 要求 TArray<TSharedPtr<T>> ?
	// 因为 SListView<T> 要求 T 必须是 TSharedPtr<T>, UObject*, TFieldPath, TSharedRef, FName
	// 而 FPropertyRule 是一个普通的 USTRUCT，所以这里不能直接用 FPropertyRule
	// 而 PropertyMatchRuleExecutor->RuleData 中的 PropertyRules 是一个 TArray<FPropertyRule>
	// 所以这里需要一个中间层，将 FPropertyRule 转换为 TSharedPtr<FPropertyRule>
	// 也就是下面的 RuleItems，同时需要一个同步机制，当 UI 中数据修改时，需要同步到 PropertyMatchRuleExecutor 中
	TSharedPtr<SListView<TSharedPtr<FPropertyRule>>> PropertyRuleListView;

	// 新增一个成员变量，用于 SListView 显示的规则列表，最后需要将这个数组中的数据同步到 PropertyMatchRuleExecutor 中
	TArray<TSharedPtr<FPropertyRule>> RuleItems;
	
	// 用于 SListView 显示的规则列表
	TArray<TWeakObjectPtr<UPropertyMatchRuleExecutor>> PropertyRuleItems;


private:
	// 生成 SListView 的行
	TSharedRef<ITableRow> OnGeneratePropertyRuleRow(TSharedPtr<FPropertyRule> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	FString OnObjectPropertyEntryBoxCreated(TSharedPtr<FPropertyRule> InItem);
	void OnObjectPropertyEntryBoxChanged(TSharedPtr<FPropertyRule> InItem, const FAssetData& AssetData);
	void OnPropSelectionChanged(TSharedPtr<FPropertyRule> InPropertyRule, TSharedPtr<FName> NewSelection);
	
	// 更新属性名选项
	void UpdatePropertyOptions(UClass* TargetClass, TArray<TSharedPtr<FName>>& OutOptions);

	TSharedPtr<FString> GetInitialEnumOption(FPropertyRule& PropertyRule);
	TSharedPtr<FString> GetInitialMatchOption(const FPropertyRule& PropertyRule);
	TSharedRef<SWidget> CreateEnumPropertyEditor(UEnum* Enum, FPropertyRule& PropertyRule);
	// 创建属性值编辑器
	TSharedRef<SWidget> CreateValueEditor(FPropertyRule& PropertyRule);

	// 添加新规则
	FReply OnAddPropertyRule();

	// 删除规则
	FReply OnRemovePropertyRule(FPropertyRule* PropertyRule);

	// 刷新 UI
	void RefreshList();

	
};
