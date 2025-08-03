// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "EditorSubsystem.h"
#include "TickableEditorObject.h"

#include "Core/GSError.h"
#include "Settings/IGSToolSubsystemAPI.h"

#include "GSModelingModeSubsystem.generated.h"

class UInteractiveTool;
class FModeToolkit;
class SGSToolFeedbackWidget;
class FEditorModeTools;
typedef FName FEditorModeID;		// avoid having to include for this
class UEdMode;

UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGSModelingModeSubsystem : public UEditorSubsystem, public FTickableEditorObject, public IGSToolSubsystemAPI
{
	GENERATED_BODY()
public:
	static UGSModelingModeSubsystem* Get();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void BeginListeningForModeChanges();

	bool IsFlightCameraActive();
	static bool GetIsFlightCameraActive();

	void TrySwapToStartupEditorMode();

	void AddModeButtonsToMainToolbar();


	// IGSToolSubsystemAPI
	virtual void RegisterToolHotkeyBindings(UInteractiveTool* Tool) override;
	virtual void UnRegisterToolHotkeyBindings(UInteractiveTool* Tool) override;

	virtual void EnableToolFeedbackWidget(UInteractiveTool* Tool) override;
	virtual void SetToolFeedbackStrings(const GS::GSErrorSet& ErrorSet) override;
	virtual void DisableToolFeedbackWidget(UInteractiveTool* Tool) override;


	// FTickableEditorObject interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

protected:

	TSharedPtr<FModeToolkit> FindModelingModeToolkit(UInteractiveTool* Tool);

	TSharedPtr<SGSToolFeedbackWidget> ToolFeedbackWidget;

	FDelegateHandle ModeChangeListenerHandle;
	TWeakObjectPtr<UEdMode> LastModelingMode;
	FEditorModeTools* ActiveModeToolsPtr = nullptr;
	void OnEditorModeChanged(const FEditorModeID& ModeID, bool IsEnteringMode);
	void OnEditorModeChanged(FEditorModeID ModeID);

	void OnExecuteModelingModeCommand(int CommandEnum);

	bool bModelingModeActive = false;


	FDelegateHandle EditorInitializedHandle;
	FName PendingEditorMode;
	bool bSwapToEditorModePending = false;

	bool bInitialEditorSetupCompleted = false;
};
