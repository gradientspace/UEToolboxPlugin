// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Tools/ModelGridEditorTool.h"
#include "ModelGridEditorTool_Editor.generated.h"

UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridEditorToolEditorBuilder : public UModelGridEditorToolBuilder
{
	GENERATED_BODY()
public:
	virtual UModelGridEditorTool* CreateNewToolOfType(const FToolBuilderState& SceneState) const override;
};



UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridEditorToolEditor
	: public UModelGridEditorTool
{
	GENERATED_BODY()
public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
};
