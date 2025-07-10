// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "ResScannerStyle.h"

class FResScannerCommands : public TCommands<FResScannerCommands>
{
public:

	FResScannerCommands()
		: TCommands<FResScannerCommands>(TEXT("ResScanner"), NSLOCTEXT("Contexts", "ResScanner", "ResScanner Plugin"), NAME_None, FResScannerStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};