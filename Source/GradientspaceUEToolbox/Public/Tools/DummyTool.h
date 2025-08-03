// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveToolBuilder.h"

#include "DummyTool.generated.h"

UCLASS()
class GRADIENTSPACEUETOOLBOX_API UDummyToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return false; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override { return nullptr; }
};
