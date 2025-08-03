// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Tools/InteractiveToolsCommands.h"


enum class EGradientspaceModelingModeCommands
{
	BeginQuickMoveTool
};

class FGradientspaceModelingModeHotkeys : public TCommands<FGradientspaceModelingModeHotkeys>
{
public:
	FGradientspaceModelingModeHotkeys();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> QuickMoveCommand;

	static void RegisterCommandBindings(TSharedPtr<FUICommandList> UICommandList, TFunction<void(EGradientspaceModelingModeCommands)> OnCommandExecuted);
	static void UnRegisterCommandBindings(TSharedPtr<FUICommandList> UICommandList);
};





/**
 * 
 */
class FGradientspaceModelingToolHotkeys : public TInteractiveToolCommands<FGradientspaceModelingToolHotkeys>
{
public:
	FGradientspaceModelingToolHotkeys();

	virtual void GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs) override;

	static void RegisterAllToolActions();
	static void UnregisterAllToolActions();
	static void UpdateToolCommandBinding(UInteractiveTool* Tool, TSharedPtr<FUICommandList> UICommandList, bool bUnbind = false);
};





#define DECLARE_TOOL_HOTKEYS(CommandsClassName) \
class CommandsClassName : public TInteractiveToolCommands<CommandsClassName> \
{\
public:\
	CommandsClassName();\
	virtual void GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs) override;\
};\


DECLARE_TOOL_HOTKEYS(FModelGridEditorToolHotkeys);
DECLARE_TOOL_HOTKEYS(FQuickMoveToolHotkeys);
DECLARE_TOOL_HOTKEYS(FPixelPaintToolHotkeys);
DECLARE_TOOL_HOTKEYS(FTexturePaintToolHotkeys);
