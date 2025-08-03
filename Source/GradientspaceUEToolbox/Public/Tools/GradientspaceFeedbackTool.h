// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "PropertyTypes/ActionButtonGroup.h"

#include "GradientspaceFeedbackTool.generated.h"

UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGradientspaceFeedbackToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGradientspaceFeedbackToolSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:


	UPROPERTY(EditAnywhere, Category="Feedback", meta = (DisplayName="Feedback", MultiLine = true))
	FString FeedbackText;

	/** Type your feedback here. Shift+Enter to go to a new line.*/
	UPROPERTY(EditAnywhere, Category = "Feedback")
	FGSActionButtonGroup FeedbackButtons;


	UPROPERTY(EditAnywhere, Category = "Quick Links")
	FGSActionButtonGroup LinkButtons;

	/** The current version of the UE Toolbox plugin installed on your computer */
	UPROPERTY(VisibleAnywhere, Category = "Plugin Info", meta=(NoResetToDefault))
	FString InstalledVersion;

	/** The latest version of the UE Toolbox plugin available from the Gradientspace website */
	UPROPERTY(VisibleAnywhere, Category = "Plugin Info", meta = (NoResetToDefault, EditCondition=bUpdateCheckHasRun, EditConditionHides, HideEditConditionToggle))
	FString AvailableVerson;

	UPROPERTY(EditAnywhere, Category = "Plugin Info")
	FGSActionButtonGroup UpdateButtons;

	UPROPERTY()
	bool bUpdateCheckHasRun = false;

	UPROPERTY()
	bool bNewerVersionAvaiable = false;
};



UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGradientspaceFeedbackTool
	: public UInteractiveTool
{
	GENERATED_BODY()
public:
	UGradientspaceFeedbackTool();
	virtual void Setup() override;



	UPROPERTY()
	TObjectPtr<UGradientspaceFeedbackToolSettings> Settings;


protected:
	TSharedPtr<FGSActionButtonTarget> ActionButtonsTarget;
	virtual void OnActionButtonClicked(FString CommandName);
	virtual bool IsActionButtonEnabled(FString CommandName);
	virtual bool IsActionButtonVisible(FString CommandName);
};
