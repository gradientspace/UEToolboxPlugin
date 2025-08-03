// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Core/GSError.h"

class UInteractiveTool;

class GRADIENTSPACEUETOOLCORE_API IGSToolSubsystemAPI
{
public:
	virtual ~IGSToolSubsystemAPI() {}

	virtual void RegisterToolHotkeyBindings(UInteractiveTool* Tool) = 0;
	virtual void UnRegisterToolHotkeyBindings(UInteractiveTool* Tool) = 0;

	virtual void EnableToolFeedbackWidget(UInteractiveTool* Tool) = 0;
	virtual void SetToolFeedbackStrings(const GS::GSErrorSet& ErrorSet) = 0;
	virtual void DisableToolFeedbackWidget(UInteractiveTool* Tool) = 0;
};
