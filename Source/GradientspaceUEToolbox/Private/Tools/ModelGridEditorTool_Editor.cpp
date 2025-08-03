// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/ModelGridEditorTool_Editor.h"
#include "InteractiveToolManager.h"
#include "EditorIntegration/GSModelingModeSubsystem.h"



UModelGridEditorTool* UModelGridEditorToolEditorBuilder::CreateNewToolOfType(const FToolBuilderState& SceneState) const
{
	return NewObject<UModelGridEditorToolEditor>(SceneState.ToolManager);
}


void UModelGridEditorToolEditor::Setup()
{
	UModelGridEditorTool::Setup();

	if (UGSModelingModeSubsystem* Subsystem = UGSModelingModeSubsystem::Get())
		Subsystem->RegisterToolHotkeyBindings(this);
}

void UModelGridEditorToolEditor::Shutdown(EToolShutdownType ShutdownType)
{
	if (UGSModelingModeSubsystem* Subsystem = UGSModelingModeSubsystem::Get())
		Subsystem->UnRegisterToolHotkeyBindings(this);

	UModelGridEditorTool::Shutdown(ShutdownType);
}