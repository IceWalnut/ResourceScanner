// Copyright Epic Games, Inc. All Rights Reserved.

#include "ResScannerCommands.h"

#define LOCTEXT_NAMESPACE "FResScannerModule"

void FResScannerCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "ResScanner", "Bring up ResScanner window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
