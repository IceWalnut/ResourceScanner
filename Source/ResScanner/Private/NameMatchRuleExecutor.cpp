#include "NameMatchRuleExecutor.h"
#include "RegexUtil.h"

// 这里实际上不合适用 INLINE ，因为实际的执行中有 for 循环
// 而 inline 是在编译期间进行的代码替换，优化了函数执行的参数入栈出栈问题
// 有了循环，编译时会有代码膨胀的问题
bool UNameMatchRuleExecutor::Match_Implementation(const FAssetData& AssetData) const
{
	FString Name = AssetData.AssetName.ToString();
	return EvaluateNameMatch(Name, RuleData);
}

FORCEINLINE FString UNameMatchRuleExecutor::GetErrorReason_Implementation() const
{
	return Super::GetErrorReason_Implementation();
}

FORCEINLINE EScanRuleType UNameMatchRuleExecutor::GetRuleType() const
{
	return EScanRuleType::NameMatch;
}

/**
 * 根据规则评估资产评是否匹配
 * @param InAssetName 资产名
 * @param RuleData 规则
 * @return 
 */
bool UNameMatchRuleExecutor::EvaluateNameMatch(const FString& InAssetName, const FNameMatchRule& RuleData)
{
	if (InAssetName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[EvaluatePropertyMatch] InAssetName is empty"));
		return false;
	}

	if (RuleData.NameRules.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[EvaluatePropertyMatch] PropertyRules is empty"));
		return false;
	}
	
	for (const FNameRule& Rule : RuleData.NameRules)
	{
		if (!EvaluateSingleRule(InAssetName, Rule))
		{
			// 如果不匹配，返回 是否反转检查
			return RuleData.bReverseCheck;
		}
	}

	// 如果匹配，返回反转检查的逆值
	return !RuleData.bReverseCheck;
}

bool UNameMatchRuleExecutor::EvaluateSingleRule(const FString& AssetName, const FNameRule& NameRule)
{
	int32 MatchCount = 0;

	for (const FString& Pattern : NameRule.RuleList)
	{
		bool bMatched = false;
		switch (NameRule.MatchMode)
		{
		case ENameMatchMode::StartWith:
			bMatched = AssetName.StartsWith(Pattern);
			break;
		case ENameMatchMode::EndWith:
			bMatched = AssetName.EndsWith(Pattern);
			break;
		case ENameMatchMode::Contain:
			bMatched = AssetName.Contains(Pattern);
			break;
		case ENameMatchMode::Regex:
			bMatched = RegexUtil::TryMatch(Pattern, AssetName);
			break;
		}

		if (bMatched)
		{
			MatchCount++;
			if (NameRule.MatchLogic == EMatchLogic::Optional && MatchCount >= NameRule.OptionalRuleMatchNum)
				return true;
		}
		else
		{
			if (NameRule.MatchLogic == EMatchLogic::Necessary)
				return false;			// Necessary 情况下只要有一个不匹配就失败
		}
	}

	// 最终判断，如果是 必须，则直接返回 true，因为上面已经判断过 必须 且 为 false 的情况了，这里只会是 true
	if (NameRule.MatchLogic == EMatchLogic::Necessary)
		return true;
	// 走到这里一定是 Optional ，直接判断 MatchCount 是否达到  OptionalRuleMatchNum 即可
	return MatchCount >= NameRule.OptionalRuleMatchNum;
}
