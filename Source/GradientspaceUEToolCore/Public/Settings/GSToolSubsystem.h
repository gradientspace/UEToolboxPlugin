// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "Settings/IGSToolSubsystemAPI.h"

#include "GSToolSubsystem.generated.h"


class UInteractiveTool;


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UGSToolSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:
	static UGSToolSubsystem* Get();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void SetActiveAPI(IGSToolSubsystemAPI* NewImplementation);

	void RegisterToolHotkeyBindings(UInteractiveTool* Tool);
	void UnRegisterToolHotkeyBindings(UInteractiveTool* Tool);

	void EnableToolFeedbackWidget(UInteractiveTool* Tool);
	void SetToolFeedbackStrings(const GS::GSErrorSet& ErrorSet);
	void DisableToolFeedbackWidget(UInteractiveTool* Tool);

protected:
	IGSToolSubsystemAPI* Impl = nullptr;

	TSharedPtr<IGSToolSubsystemAPI> DefaultImpl;
};




class GSDefaultToolSubsystemAPI : public IGSToolSubsystemAPI
{
public:
	virtual void RegisterToolHotkeyBindings(UInteractiveTool* Tool) override;
	virtual void UnRegisterToolHotkeyBindings(UInteractiveTool* Tool) override;

	virtual void EnableToolFeedbackWidget(UInteractiveTool* Tool) override;
	virtual void SetToolFeedbackStrings(const GS::GSErrorSet& ErrorSet) override;
	virtual void DisableToolFeedbackWidget(UInteractiveTool* Tool) override;
};
