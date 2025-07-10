#pragma once

#include "CoreMinimal.h"
#include "ResScannerRuleBase.h"
#include "RuleDataType.h"
#include "NameMatchRuleExecutor.generated.h"

/**
 * 名字匹配规则执行器
 */
UCLASS(Blueprintable)
class RESSCANNER_API UNameMatchRuleExecutor : public UResScannerRuleBase
{
	GENERATED_BODY()

public:
	
	virtual bool Match_Implementation(const FAssetData& AssetData) const override;

	virtual FString GetErrorReason_Implementation() const override;

	virtual EScanRuleType GetRuleType() const override;

public:
	// 规则数据
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ResScanner")
	FNameMatchRule RuleData;

private:
	// 根据规则数据评估是否合规
	static bool EvaluateNameMatch(const FString& InAssetName, const FNameMatchRule& RuleData);
	// 根据单条规则评估是否合规
	static bool EvaluateSingleRule(const FString& AssetName, const FNameRule& NameRule);
};


