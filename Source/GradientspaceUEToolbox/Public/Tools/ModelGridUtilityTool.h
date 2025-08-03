// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"


#include "ModelGridUtilityTool.generated.h"

class AGSModelGridActor;
class UGSModelGridAsset;
class UWorld;


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridUtilityToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};



UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridUtilityTool_TargetInfo : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Dimensions of the Component's internal ModelGrid */
	UPROPERTY(VisibleAnywhere, Category="Component Info", meta=(NoResetToDefault))
	FString ComponentGrid;

	/** Dimensions of the ModelGrid Asset assigned to the Component */
	UPROPERTY(VisibleAnywhere, Category = "Asset Info", meta = (NoResetToDefault))
	FString AssetGrid;

	/** The ModelGrid Asset assigned to the Component */
	UPROPERTY(VisibleAnywhere, Category = "Asset Info", meta=(NoResetToDefault, EditCondition=bTargetHasAssetAssigned, EditConditionHides, HideEditConditionToggle))
	TObjectPtr<UGSModelGridAsset> AssignedAsset;

	/** Is the ModelGrid Asset being overridden (eg by the Ignore Asset flag on the Component) */
	UPROPERTY(VisibleAnywhere, Category = "Asset Info", meta=(NoResetToDefault, EditCondition=bTargetHasAssetAssigned, EditConditionHides, HideEditConditionToggle))
	FString IsAssetOverridden;

	UPROPERTY()
	bool bTargetHasAssetAssigned = false;
};


enum class EModelGridUtilityToolActions
{
	NoAction = 0,
	CopyToNewAsset = 1000,
	SetToNewAsset = 1001,
	CopyAssetToComponent = 1010,
	RemoveAsset = 1020,
	ClearComponent = 1030,
	CopyFromSelected = 1040
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridUtilityTool_AssetFunctions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** 
	 * Create a new ModelGrid Asset from the target ModelGrid Actor. 
	 * The new Asset is not assigned to this Actor.
	 * The new Asset's data will be taken from the currently-in-use ModelGrid (ie Asset or Component data)
	 */
	UFUNCTION(CallInEditor, Category = AssetUtilities, meta = (DisplayPriority = 1))
	void CopyToNewAsset()
	{
		PostAction(EModelGridUtilityToolActions::CopyToNewAsset);
	}

	/** 
	 * Create a new ModelGrid Asset from the target ModelGrid Actor, and assign it to the Actor. 
	 * The new Asset's data will be taken from the currently-in-use ModelGrid (ie Asset or Component data)
	 */
	UFUNCTION(CallInEditor, Category = AssetUtilities, meta = (DisplayPriority = 2))
	void SetToNewAsset()
	{
		PostAction(EModelGridUtilityToolActions::SetToNewAsset);
	}

	/** Copy the data in the currently-assigned ModelGrid Asset to the Component's internal ModelGrid */
	UFUNCTION(CallInEditor, Category = AssetUtilities, meta = (DisplayPriority = 3))
	void CopyAssetToComponent()
	{
		PostAction(EModelGridUtilityToolActions::CopyAssetToComponent);
	}

	/** Un-assign the ModelGrid Asset for the current Actor, if one is assigned */
	UFUNCTION(CallInEditor, Category = AssetUtilities, meta = (DisplayPriority = 4))
	void RemoveAsset()
	{
		PostAction(EModelGridUtilityToolActions::RemoveAsset);
	}

	/** Set the Component ModeLGrid to an empty grid */
	UFUNCTION(CallInEditor, Category = AssetUtilities, meta = (DisplayPriority = 5))
	void ClearComponentGrid()
	{
		PostAction(EModelGridUtilityToolActions::ClearComponent);
	}

	/** 
	 * Copy the data in the currently-selected ModelGrid Actor to the target ModelGrid Actor
	 * The currently-in-use ModelGrid (ie Asset or Component data) will be updated.
	 */
	UFUNCTION(CallInEditor, Category = AssetUtilities, meta = (DisplayPriority = 6))
	void CopyFromSelected()
	{
		PostAction(EModelGridUtilityToolActions::CopyFromSelected);
	}

	TWeakObjectPtr<UModelGridUtilityTool> Tool;
	void PostAction(EModelGridUtilityToolActions Action);
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridUtilityTool : public UInteractiveTool
{
	GENERATED_BODY()
public:

	virtual void Setup() override;
	virtual void OnTick(float DeltaTime) override;


	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override { return false; }
	virtual bool CanAccept() const override { return false; }


public:

	void SetTargetWorld(UWorld* World);
	UWorld* GetTargetWorld();

	void SetExistingActor(AGSModelGridActor* Actor);

	void RequestAction(EModelGridUtilityToolActions Action);


protected:
	UPROPERTY()
	TObjectPtr<UModelGridUtilityTool_TargetInfo> TargetInfo;

	UPROPERTY()
	TObjectPtr<UModelGridUtilityTool_AssetFunctions> AssetFunctionsProps;

protected:

	UPROPERTY()
	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	UPROPERTY()
	TWeakObjectPtr<AGSModelGridActor> ExistingActor = nullptr;

	void UpdateTargetInfo();

	bool bHavePendingAction = false;
	EModelGridUtilityToolActions PendingAction;
	virtual void ApplyAction(EModelGridUtilityToolActions ActionType);


	virtual void SaveAsAsset(bool bUpdateTargetActor);
	virtual void CopyAssetToComponent();
	virtual void RemoveAsset();
	virtual void ClearComponent();
	virtual void CopyFromSelected();
};


