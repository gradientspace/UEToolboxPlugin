// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceUEToolboxHotkeys.h"
#include "Styling/AppStyle.h"

#include "Tools/ModelGridEditorTool_Editor.h"
#include "Tools/MoverTool.h"
#include "Tools/PixelPaintTool.h"
#include "Tools/GSTexturePaintTool.h"

#define LOCTEXT_NAMESPACE "GradientspaceUEToolboxHotkeys"




FGradientspaceModelingModeHotkeys::FGradientspaceModelingModeHotkeys() :
	TCommands<FGradientspaceModelingModeHotkeys>(
		"GradientspaceModelingModeCommands", // Context name for fast lookup
		NSLOCTEXT("Contexts", "GradientspaceModelingModeCommands", "Gradientspace Modeling Mode Shortcuts"), // Localized context name for displaying
		NAME_None, // Parent
		FAppStyle::GetAppStyleSetName() // Icon Style Set
	)
{
}

void FGradientspaceModelingModeHotkeys::RegisterCommands()
{
	UI_COMMAND(QuickMoveCommand, "Quick Move Tool", "Launch the Quick Move Tool", EUserInterfaceActionType::Button, FInputChord(EKeys::Tab));
}

void FGradientspaceModelingModeHotkeys::RegisterCommandBindings(TSharedPtr<FUICommandList> UICommandList, TFunction<void(EGradientspaceModelingModeCommands)> OnCommandExecuted)
{
	const FGradientspaceModelingModeHotkeys& Commands = FGradientspaceModelingModeHotkeys::Get();

	UICommandList->MapAction(
		Commands.QuickMoveCommand,
		FExecuteAction::CreateLambda([OnCommandExecuted]() { OnCommandExecuted(EGradientspaceModelingModeCommands::BeginQuickMoveTool); }));
}

void FGradientspaceModelingModeHotkeys::UnRegisterCommandBindings(TSharedPtr<FUICommandList> UICommandList)
{
	const FGradientspaceModelingModeHotkeys& Commands = FGradientspaceModelingModeHotkeys::Get();
	UICommandList->UnmapAction(Commands.QuickMoveCommand);
}







FGradientspaceModelingToolHotkeys::FGradientspaceModelingToolHotkeys() :
	TInteractiveToolCommands<FGradientspaceModelingToolHotkeys>(
		"GradientspaceModelingToolsHotkeys", // Context name for fast lookup
		NSLOCTEXT("Contexts", "GradientspaceModelingToolsHotkeys", "Gradientspace Tools - Shared Shortcuts"), // Localized context name for displaying
		NAME_None, // Parent
		FAppStyle::GetAppStyleSetName() // Icon Style Set
	)
{
}


void FGradientspaceModelingToolHotkeys::GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs)
{
}


void FGradientspaceModelingToolHotkeys::RegisterAllToolActions()
{
	FGradientspaceModelingToolHotkeys::Register();
	FModelGridEditorToolHotkeys::Register();
	FQuickMoveToolHotkeys::Register();
	FPixelPaintToolHotkeys::Register();
	FTexturePaintToolHotkeys::Register();
}

void FGradientspaceModelingToolHotkeys::UnregisterAllToolActions()
{
	FGradientspaceModelingToolHotkeys::Unregister();
	FModelGridEditorToolHotkeys::Unregister();
	FQuickMoveToolHotkeys::Unregister();
	FPixelPaintToolHotkeys::Unregister();
	FTexturePaintToolHotkeys::Unregister();
}


void FGradientspaceModelingToolHotkeys::UpdateToolCommandBinding(UInteractiveTool* Tool, TSharedPtr<FUICommandList> UICommandList, bool bUnbind)
{
#define UPDATE_BINDING(CommandsType)  if (!bUnbind) CommandsType::Get().BindCommandsForCurrentTool(UICommandList, Tool); else CommandsType::Get().UnbindActiveCommands(UICommandList);

	if (ExactCast<UModelGridEditorToolEditor>(Tool) != nullptr)
	{
		UPDATE_BINDING(FModelGridEditorToolHotkeys);
	}
	else if (ExactCast<UGSMoverTool>(Tool) != nullptr)
	{
		UPDATE_BINDING(FQuickMoveToolHotkeys);
	}
	else if (ExactCast<UGSMeshPixelPaintTool>(Tool) != nullptr)
	{
		UPDATE_BINDING(FPixelPaintToolHotkeys);
	}
	else if (ExactCast<UGSMeshTexturePaintTool>(Tool) != nullptr)
	{
		UPDATE_BINDING(FTexturePaintToolHotkeys);
	}
	else
	{
		UPDATE_BINDING(FGradientspaceModelingToolHotkeys);
	}
}



#define DEFINE_TOOL_HOTKEYS_IMPL(CommandsClassName, ContextNameString, SettingsDialogString, ToolClassName ) \
CommandsClassName::CommandsClassName() : TInteractiveToolCommands<CommandsClassName>( \
ContextNameString, NSLOCTEXT("Contexts", ContextNameString, SettingsDialogString), NAME_None, FAppStyle::GetAppStyleSetName()) {} \
void CommandsClassName::GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs) \
{\
	ToolCDOs.Add(GetMutableDefault<ToolClassName>()); \
}

DEFINE_TOOL_HOTKEYS_IMPL(FModelGridEditorToolHotkeys, "ModelGridEditorTool", "Gradientspace Grid Builder Tool", UModelGridEditorToolEditor);
DEFINE_TOOL_HOTKEYS_IMPL(FQuickMoveToolHotkeys, "QuickMoveTool", "Gradientspace QuickMove Tool", UGSMoverTool);
DEFINE_TOOL_HOTKEYS_IMPL(FPixelPaintToolHotkeys, "PixelPaintTool", "Gradientspace PixelPaint Tool", UGSMeshPixelPaintTool);
DEFINE_TOOL_HOTKEYS_IMPL(FTexturePaintToolHotkeys, "TexturePaintTool", "Gradientspace TexturePaint Tool", UGSMeshTexturePaintTool);



#undef LOCTEXT_NAMESPACE 

