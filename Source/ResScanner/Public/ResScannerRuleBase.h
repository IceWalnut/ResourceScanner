#pragma once

#include "CoreMinimal.h"
#include "ResScannerRuleBase.generated.h"

UENUM()
enum class EScanRuleType : uint8
{
	Base,
	NameMatch,
	PathMatch,
	PropertyMatch
};

/**
 * 资源扫描规则基类
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class RESSCANNER_API UResScannerRuleBase : public UObject
{
	GENERATED_BODY()
public:
	// 判断是否匹配
	// BlueprintNativeEvent 表示既能在 C++ 中提供默认实现，又能在蓝图中被覆盖的函数
	// 当从 C++ 中调用此函数时，若有蓝图覆盖，则执行蓝图版本；否则执行 C++ 版本
	UFUNCTION(BlueprintNativeEvent)
	bool Match(const FAssetData& AssetData) const;
	// virtual 允许子类重写此函数
	// _Implementation 是 UE 反射系统要求 BlueprintNativeEvent 必须有对应的 _Implementation 后缀函数作为默认实现
	// const 表示不会修改对象状态
	// 默认返回 true 是安全设计：当子类忘记实现时，规则不会错误拦截资源
	virtual bool Match_Implementation(const FAssetData& AssetData) const { return true; }

	// 返回失败的原因
	UFUNCTION(BlueprintNativeEvent)
	FString GetErrorReason() const;
	virtual FString GetErrorReason_Implementation() const { return TEXT("Error Reason"); }

	// 是否启用反向检测（如：找出不符合命名规范的资源）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ResScannerRule")
	bool bReverseCheck = false;

	
	
	virtual EScanRuleType GetRuleType() const
	{
		return EScanRuleType::Base;
	};

	FString ScanRuleTypeToString(EScanRuleType Type)
	{
		static const TMap<EScanRuleType, FString> TypeMap =
		{
			{ EScanRuleType::Base, TEXT("Base") }, 
			{ EScanRuleType::NameMatch, TEXT("NameMatch") },
			{ EScanRuleType::PathMatch, TEXT("PathMatch") },
			{ EScanRuleType::PropertyMatch, TEXT("PropertyMatch") }
		};
		return TypeMap.FindChecked(Type);
	}
	
};
