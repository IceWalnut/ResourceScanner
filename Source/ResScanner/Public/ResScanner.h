// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ResScannerRuleSet.h"

class FToolBarBuilder;
class FMenuBuilder;

struct FScanResultItem
{
	FString AssetPath;
	FString RuleName;
	FString ErrorReason;
};

DECLARE_LOG_CATEGORY_EXTERN(LogResScanner, Log, All)

class FResScannerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();
	

public:
	// 模块共享指针引用
	//TSharedPtr<FResScannerModule> SharedThis;
	
private:

	void RegisterMenus();

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);
	
	// 扫描按钮点击事件
	FReply OnStartScanClicked();
	// 导入配置按钮点击事件
	FReply OnImportConfigClicked();
	// 导出配置按钮点击事件
	FReply OnExportConfigClicked();

	// 添加规则
	FReply OnAddNewRule();
	FReply OnEditRuleClicked(UResScannerRuleBase* InItem);

	// 生成列表每一行
	// 这个函数用来告诉 Slate 每一行怎么渲染
	TSharedRef<ITableRow> OnGenerateResultRow(TSharedPtr<FScanResultItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateRuleRow(
		UResScannerRuleBase* InItem, const TSharedRef<STableViewBase>& OwnerTable);

	// TODO: 开始扫描资源
	void RunAssetScan();

	bool SerializeRuleSetToJson(const UResScannerRuleSet* InRuleSet, FString& OutJson);

	TSharedRef<SWidget> CreateRuleConfigPanel();
	TSharedRef<SWidget> CreateScanResultsPanel();
	void OnRuleSelectionChanged(TSharedPtr<FString> NewSelection);


private:
	TSharedPtr<class FUICommandList> PluginCommands;

	// 扫描结果
	TArray<TSharedPtr<FScanResultItem>> ScanResults;
	// 扫描结果列表视图
	TSharedPtr<SListView<TSharedPtr<FScanResultItem>>> ScanResultsListView;

	// 规则集
	UResScannerRuleSet* RuleSet;
	// 规则列表
	// 原来写的是 TArray<TSharedPtr<UResScannerRuleBase>> RuleItems; 这种也不推荐
	// 因为会和 UObject 的 GC 冲突
	TArray<TWeakObjectPtr<UResScannerRuleBase>> RuleItems;		// 用于 SListView 显示
	// 规则列表视图
	// RuleListView 用于存储左侧规则列表的 SListView，与 ScanResultsListView 类似
	TSharedPtr<SListView<TWeakObjectPtr<UResScannerRuleBase>>> RuleListView;

	// 规则类型选项
	TArray<TSharedPtr<FString>> RuleTypeOptions = {
		MakeShared<FString>(TEXT("NameMatch")),
		MakeShared<FString>(TEXT("PropertyMatch"))
		// MakeShared<FString>(TEXT("PathMatch"))
	};
	// 选中的类型
	TSharedPtr<FString> SelectedRuleType;
};