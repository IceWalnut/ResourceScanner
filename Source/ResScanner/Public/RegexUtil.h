#pragma once

#include "CoreMinimal.h"

namespace RegexUtil
{
	static bool TryMatch(const FString& Pattern, const FString& InputText)
	{
		if (Pattern.IsEmpty() || InputText.IsEmpty())
		{
			return false;
		}

		// 构造 Pattern 和 Matcher
		FRegexPattern CompiledPattern(Pattern);
		FRegexMatcher Matcher(CompiledPattern, InputText);

		// 执行正则匹配
		return Matcher.FindNext();
	}
}