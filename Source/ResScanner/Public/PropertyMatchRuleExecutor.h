#pragma once

#include "CoreMinimal.h"
#include "ResScannerRuleBase.h"
#include "RuleDataType.h"
#include "PropertyMatchRuleExecutor.generated.h"

UCLASS(Blueprintable)
class RESSCANNER_API UPropertyMatchRuleExecutor : public UResScannerRuleBase
{
	GENERATED_BODY()

public:
	
	// 是否匹配
	// 注意这里只需要实现基类中的 虚函数  Match_Implementation
	virtual bool Match_Implementation(const FAssetData& AssetData) const override;
	virtual FString GetErrorReason_Implementation() const override;
	
public:
	// 属性规则数据
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ResScanner")
	FPropertyMatchRule RuleData;

private:
	bool EvaluatePropertyMatch(const FAssetData& InAssetData, const FPropertyMatchRule& PropertyMatchRule) const;
};


