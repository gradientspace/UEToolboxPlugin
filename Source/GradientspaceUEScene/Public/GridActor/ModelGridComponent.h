// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Components/PrimitiveComponent.h"
#include "GridActor/UGSModelGrid.h"
#include "GridActor/UGSGridMaterialSet.h"
#include "GridActor/UGSModelGridAsset.h"
#include "Templates/PimplPtr.h"

namespace GS { class ModelGridCollider; }

#include "ModelGridComponent.generated.h"


UENUM(BlueprintType)
enum class EModelGridComponentCollisionMode : uint8
{
	/**  */
	NoCollision = 0,
	/** */
	GridCollider = 1
};


/**
 * UGSModelGridComponent stores a ModelGrid object. This Component does not actually render
 * or display the grid, it just provides the Grid data structures and associated
 * Material set. Generally a UGSModelGridComponent will be used with an AGSModelGridActor
 * which will create a MeshComponent for rendering, collision, etc
 * 
 * Both an Internal ModelGrid (ie stored inside the Component/Actor/Level) and an external
 * ModelGridAsset are supported. The Asset will by default override the Internal grid,
 * this can be toggled with the bIgnoreAsset property without clearing the Asset reference.
 * 
 */
UCLASS(hidecategories = (LOD), ClassGroup = Rendering)
class GRADIENTSPACEUESCENE_API UGSModelGridComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()
public:

	/** Returns the active ModelGrid, whether internal or from the referenced Asset */
	UFUNCTION(BlueprintCallable, Category = ModelGridComponent) 
	virtual UGSModelGrid* GetGrid() const;

	/** Returns the ModelGrid stored inside the Component */
	UFUNCTION(BlueprintCallable, Category = ModelGridComponent)
	virtual UGSModelGrid* GetComponentGrid() const;

	/** Returns the active MaterialSet, whether internal or from the referenced Asset */
	UFUNCTION(BlueprintCallable, Category = ModelGridComponent)
	virtual UGSGridMaterialSet* GetGridMaterials() const;

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnModelGridChanged, UGSModelGridComponent*, UGSModelGrid*);
	FOnModelGridChanged& OnModelGridChanged() { return ModelGridChangedEvent; }
protected:
	FOnModelGridChanged ModelGridChangedEvent;
	virtual void NotifyModelGridModification(UGSModelGrid* Grid);

protected:
	/** Internal ModelGrid stored in the Component (ie as part of the Actor/Level) */
	UPROPERTY(Instanced, NoClear, meta = (NoResetToDefault))
	TObjectPtr<UGSModelGrid> LocalModelGrid;

	/**
	 * Grid Material Set associated with the Local ModelGrid  (both Grid and MaterialSet are stored in the Component)
	 * This MaterialSet is ignored if a ModelGrid Asset is assigned to the Component
	 */
	UPROPERTY(VisibleAnywhere, Instanced, NoClear, Category=ModelGridComponent, meta=(DisplayName = "Local Materials", NoResetToDefault))
	TObjectPtr<UGSGridMaterialSet> LocalModelGridMaterials;


public:
	// TODO: should this be in a separate component type, maybe w/ shared base subclass?
	// TODO: may need to look at ReplicatedUsing=OnRep_StaticMesh in StaticMeshCOmpnent...
	/**
	 * ModelGrid Asset assigned to this Component. The ModelGrid from the Asset will
	 * be used instead of the local ModelGrid that is stored inside the Component.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ModelGridComponent, meta=(DisplayName="ModelGrid Asset"))
	TObjectPtr<UGSModelGridAsset> GridObjectAsset;

	/**
	 * If enabled, the assigned ModelGrid Asset will be ignored and the 
	 * Component-local ModelGrid will be used instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ModelGridComponent)
	bool bIgnoreAsset = false;

	virtual void SetGridAsset(UGSModelGridAsset* NewGridAsset);
	virtual bool HasGridAsset() const { return GridObjectAsset != nullptr; }
	virtual UGSModelGridAsset* GetGridAsset() const { return GridObjectAsset; }
	virtual UGSModelGridAsset* GetGridAssetIfEnabled() const { return (bIgnoreAsset) ? nullptr : GridObjectAsset; }





protected:
	// todo: some kind of async support...
	TPimplPtr<GS::ModelGridCollider> GridCollider;
	virtual void UpdateGridCollider();
	virtual void InvalidateGridCollider();
public:
	UPROPERTY(Category = ModelGridComponent, EditAnywhere, BlueprintReadWrite)
	EModelGridComponentCollisionMode GridCollisionMode = EModelGridComponentCollisionMode::NoCollision;

	UFUNCTION(BlueprintCallable, Category = ModelGridComponent)
	void SetGridCollisionMode(EModelGridComponentCollisionMode NewMode);

	virtual void ProcessGridCollider(TFunctionRef<void(const GS::ModelGridCollider&)> ProcessFunc) const;

protected:
	void OnLocalModelGridUpdated(UGSModelGrid* ModelGrid);
	FDelegateHandle OnLocalModelGridUpdated_Handle;

	void OnModelGridAssetUpdated(UGSModelGrid* ModelGrid);
	FDelegateHandle OnModelGridAssetUpdated_Handle;

	TWeakObjectPtr<UGSModelGridAsset> LastTargetAsset;
	//! update the Asset/OnModelGridAssetUpdated connection. Returns true if connection changed (disconnect or connect)
	bool UpdateGridAssetListener();
	void NotifyCurrentActiveGridModified();

protected:
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
	//! PostEditImport is called after editor T3D-based copy-paste/duplicate/import - PostDuplicate is *not* called on Components, but this is
	virtual void PostEditImport() override;

public:
	// TODO WORKAROUND - this is a function that external code (eg Actor) can use to 
	// force this Component to start listening to an assigned GridObjectAsset after a duplication,
	// because otherwise it doesn't know about it. TBD to figure out how to avoid this.
	virtual void ForceUpdateConnections();

};

