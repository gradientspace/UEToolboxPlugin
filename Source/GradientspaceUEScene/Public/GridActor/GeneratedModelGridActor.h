// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GridActor/ModelGridActor.h"
#include "GeneratedModelGridActor.generated.h"

/**
 * GSGeneratedModelGridActor is a variant of GSModelGridActor that supports procedural
 * generation of the ModelGrid stored inside the ModelGridComponent. This is similar to
 * GeneratedDynamicMeshActor in many ways (with improvements based on the observed usage
 * of that design).
 * 
 * This class will function at Editor and at Runtime. However at Runtime, currently
 * invalidation/mark-for-rebuild must be done explicitly.
 */
UCLASS()
class GRADIENTSPACEUESCENE_API AGSGeneratedModelGridActor : public AGSModelGridActor
{
	GENERATED_BODY()

public:
	virtual ~AGSGeneratedModelGridActor();

public:
	/** If this flag is true, then the grid will not be regenerated until it is set to false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ModelGrid")
	bool bPauseRegeneration = false;

	/** 
	 * If this flag is true, then the grid will be cleared (ie all cells empty) before running the Regenerate function 
	 * Setting to false allows edits to be accumulated
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ModelGrid")
	bool bClearGridBeforeRegenerate = true;

	/**
	 * A ModelGridComponent can be backed by a Component-level ModelGrid (ie stored inside the Component) or a
	 * ModelGridAsset. However in the Asset case, multiple Components can use the same Asset, and regenerating
	 * into that Asset will affect all of them. So (eg) placing multiple instances with different parameters will not work,
	 * they will overwrite eachothers changes to the Asset.
	 * 
	 * So by default we do not allow this situation. However if you are certain you want it - eg if you know only
	 * one instance of the generated grid will exist, or each will be set to a unique Asset - set this to true
	 * to disable the validation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ModelGrid")
	bool bAllowAssetGridModification = false;

public:

	/**
	 * This event is executed to regenerate the Actor. The input TargetGrid will be taken from the Actor's ModelGridComponent.
	 * The function ExecuteRegenerateIfPending() should be used to run OnRegenerateModelGrid, 
	 * rather than calling it yourself.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "ModelGrid")
	void OnRegenerateModelGrid(UGSModelGrid* TargetGrid);
	virtual void OnRegenerateModelGrid_Implementation(UGSModelGrid* TargetGrid) {}

	/** Mark this Actor for a grid rebuild (next frame at earliest) */
	UFUNCTION(BlueprintCallable, Category = "ModelGrid")
	void MarkForRebuild();

	/** Immediately force this Actor to rebuild, optionally even if bPauseRegeneration=true  */
	UFUNCTION(BlueprintCallable, Category = "ModelGrid")
	void RebuildImmediately(bool bEvenIfPaused);

	/**
	 * Execute OnRegenerateModelGrid() if necessary (ie if we have been marked for regeneration).
	 * Change notifications will be deferred until after the regeneration.
	 */
	virtual void ExecuteRegenerateIfPending(bool bOverridePause = false);


protected:
	bool bRegenerationPending = false;

	bool bIsRegisteredWithGenerationManager = false;
	void RegisterWithGenerationManager();
	void UnregisterWithGenerationManager();


	//
	// actor/object functions that are overridden to implement functionality for this object
	//

public:
	// UObject functions
	virtual void PostLoad() override;
	virtual void Destroyed() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	// Actor functions
	virtual void PostActorCreated() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;

};

