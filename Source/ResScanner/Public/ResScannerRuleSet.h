#pragma once

#include "CoreMinimal.h"
#include "ResScannerRuleBase.h"
#include "ResScannerRuleSet.generated.h"

/**
 * 规则容器类，后续用于组合规则
 * 比如多个 NameRule 和 PathRule 和 PropertyRule 可以组合在一个规则容器中
 */
UCLASS(Blueprintable)
class UResScannerRuleSet : public UObject
{
	GENERATED_BODY()
public:
	// 存放规则的数组
	UPROPERTY(EditAnywhere, Instanced, Category = "ResScannerRule")
	TArray<UResScannerRuleBase*> Rules;
};
