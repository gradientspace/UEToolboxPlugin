// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Tools/CreateModelGridTool.h"
#include "CreateModelGridTool_Editor.generated.h"

UCLASS()
class GRADIENTSPACEUETOOLBOX_API UCreateModelGridToolEditorBuilder : public UCreateModelGridToolBuilder
{
	GENERATED_BODY()
public:
	virtual UCreateModelGridTool* CreateNewToolOfType(const FToolBuilderState& SceneState) const override;
};



UCLASS()
class GRADIENTSPACEUETOOLBOX_API UCreateModelGridToolEditor
	: public UCreateModelGridTool
{
	GENERATED_BODY()
public:
	virtual void OnCreateNewModelGrid(GS::ModelGrid&& SourceGrid) override;
};

