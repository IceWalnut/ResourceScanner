#include "PropertyMatchRuleExecutor.h"

bool UPropertyMatchRuleExecutor::Match_Implementation(const FAssetData& AssetData) const
{
	return EvaluatePropertyMatch(AssetData, RuleData);
}

/**
 * 
 * @param InAssetData 资产
 * @param PropertyMatchRule 属性匹配规则
 * @return 
 */
bool UPropertyMatchRuleExecutor::EvaluatePropertyMatch(const FAssetData& InAssetData, const FPropertyMatchRule& PropertyMatchRule) const
{
	UObject* AssetObj = InAssetData.GetAsset();
	if (!AssetObj)
	{
		UE_LOG(LogTemp, Error, TEXT("[EvaluatePropertyMatch] AssetObj is nullptr"));
		return false;
	}
	FString AssetName = InAssetData.AssetName.ToString();
	if (AssetName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[EvaluatePropertyMatch] AssetName is empty"));
		return false;
	}

	if (PropertyMatchRule.PropertyRules.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[EvaluatePropertyMatch] PropertyRules is empty"));
		return false;
	}

	for (const FPropertyRule PropertyRule : PropertyMatchRule.PropertyRules)
	{
		if (!PropertyRule.TargetClass.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[EvaluatePropertyMatch] TargetClass is invalid"));
			return false;
		}
		// TODO: 如果资产不是这条规则中的类
		// 这里这样写 OK 吗？原来写的是 AssetObj->IsA(PropertyRule.TargetClass) 编译时报错
		UClass* TargetClass =  PropertyRule.TargetClass.LoadSynchronous();
		// 这里不能按照下面这行来写，因为 PropertyRule.TargetClass->StaticClass() 返回 nullptr
		// if (!AssetObj->IsA(PropertyRule.TargetClass->StaticClass()))
		// 因为 SoftClassPtr 并没有加载，PropertyRule.TargetClass->StaticClass 是 nullptr
		if (!AssetObj->IsA(TargetClass))
		{
			// TODO: 这里直接返回 false 就行了，千万不要打印 log，否则会所有资产都打印一行
			// UE_LOG(LogTemp, Error, TEXT("[EvaluatePropertyMatch] AssetObj is not a %s"), *PropertyRule.TargetClass->GetName());
			return false;
		}

		// TODO: 重点！！！
		// FindFProperty 传入两个参数，第一个参数是 UObject 的类，第二个参数是属性名
		// FindFProperty 泛型
		FProperty* Prop = FindFProperty<FProperty>(AssetObj->GetClass(), PropertyRule.PropertyName);
		if (Prop && PropertyRule.MatchMode == EPropertyMatchMode::Equal)
		{
			FString CurrentPropValue;
			// 获取属性值为文本，参数如下
			// FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope = nullptr
			// 要导出的字符串地址，属性值，默认值，父对象，端口标志，导出根作用域
			Prop->ExportTextItem_Direct(CurrentPropValue, Prop->ContainerPtrToValuePtr<void>(AssetObj),
				nullptr, nullptr, PPF_None);
			return CurrentPropValue == PropertyRule.PropertyValue;
		}
	}
	
	return false;
}

FString UPropertyMatchRuleExecutor::GetErrorReason_Implementation() const
{
	return FString::Printf(TEXT("属性不匹配值"));
}
