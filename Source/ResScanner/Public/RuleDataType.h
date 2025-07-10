#pragma once

#include "CoreMinimal.h"
#include "RuleDataType.generated.h"

// 名字匹配规则
UENUM()
enum class ENameMatchMode : uint8
{
    StartWith,          // 以...开头
    EndWith,            // 以...结尾
    Contain,            // 包含...
    Regex               // 正则匹配
};

// 匹配逻辑
UENUM()
enum class EMatchLogic : uint8
{
    Necessary,
    Optional
};

// ---------------------------------------------- Name Rule --------------------------------------------- //
// 文件名规则
USTRUCT(BlueprintType)
struct FNameRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ENameMatchMode MatchMode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMatchLogic MatchLogic;

    // 作用：正则匹配的时候，如果选了 Optinal，匹配的数量，只有大于这个数量，才算匹配成功
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 OptionalRuleMatchNum = 1;

    // 规则数组，比如说
    // 规则1： 以 T_ 开头 ，这个是 必须的，也就是 Necessary
    // 规则2： 以 _d 或 _x 结尾，这个是可选的，也就是 Optional
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RuleList;
};

USTRUCT(BlueprintType)
struct FNameMatchRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FNameRule> NameRules;

    // 是否反向匹配
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bReverseCheck;
};

// ---------------------------------------------- Name Rule --------------------------------------------- //

// -------------------------------------------- Property Rule --------------------------------------------- //
// 属性匹配模式
UENUM()
enum class EPropertyMatchMode : uint8
{
    // TODO: 将来需要扩展其他模式
    Equal,
    NotEqual
};

// 属性规则
USTRUCT(BlueprintType)
struct FPropertyRule
{
    GENERATED_BODY()

    // TODO: 目标 Class (如 UTexture2D)
    // TSoftClassPtr 是一个软引用 SoftReference 到某个 UClass 类型的指针。
    // 它的底层本质是一个 FSoftObjectPath，指向一个 Blueprint 类或原生类的路径，可以跨模块保存而不导致资源强引用或强依赖。
    // 这个字段允许你在编辑器中选择一个类（包括 Blueprint 类）
    // 它不会被加载到内存中，直到调用 LoadSynchronous() 或 TryLoad()
    // 非常适合保存 用户指定的类名而不强依赖该类的场景
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ResScanner")
    TSoftClassPtr<UObject> TargetClass;

    // 属性名 (如 CompressionSettings)
    // TODO: 说一下这里为什么用 FName
    // 所有 FName 实例存储在全局唯一表 中，相同字符串共享一份数据，避免重复存储。
    // 不区分大小写
    // 内部通过索引值引用，操作高效
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ResScanner", meta = (EditCondition = "TargetClass != nullptr"))
    FName PropertyName;
        
    // 匹配模式 (如 Equal)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ResScanner", meta = (EditCondition = "PropertyName != NAME_NONE"))
    EPropertyMatchMode MatchMode;

    // TODO: 属性值 (序列化为字符串，动态解析)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ResScanner", meta = (EditCondition = "PropertyName != NAME_NONE"))
    FString PropertyValue;

    // 属性数组，这个是运行时数据，没有持久化的需求，因此不需要 UPROPERTY 标记
    TArray<TSharedPtr<FName>> LocalPropertyOptions;
    // 枚举属性数组，运行时数据，没有持久化需求
    // 如果属性是枚举值，存在内存中，不需要 UPROPERTY
    TArray<TSharedPtr<FString>> EnumValueOptions;
};

// 属性规则列表
USTRUCT(BlueprintType)
struct FPropertyMatchRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FPropertyRule> PropertyRules;

    // 是否反向匹配
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bReverseCheck;
};
// -------------------------------------------- Property Rule --------------------------------------------- //