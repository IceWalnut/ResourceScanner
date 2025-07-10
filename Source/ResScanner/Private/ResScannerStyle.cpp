// Copyright Epic Games, Inc. All Rights Reserved.

#include "ResScannerStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FResScannerStyle::StyleInstance = nullptr;

void FResScannerStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FResScannerStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FResScannerStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("ResScannerStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FResScannerStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("ResScannerStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("ResScanner")->GetBaseDir() / TEXT("Resources"));

	Style->Set("ResScanner.OpenPluginWindow", new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), Icon20x20));

	// 设置支持中文字体
	FSlateFontInfo FontInfo(RootToContentDir(TEXT("Font/msyh.ttf"), TEXT("")), 12);
	Style->Set("ResScanner.DefaultFont", FontInfo);
	return Style;
}

void FResScannerStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FResScannerStyle::Get()
{
	return *StyleInstance;
}
