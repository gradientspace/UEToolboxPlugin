// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GridActor/ModelGridComponent.h"
#include "ModelGridActor.generated.h"

class UDynamicMeshComponent;
namespace GS { class ModelGrid; }
namespace UE::Geometry { class FDynamicMesh3; }


UENUM(BlueprintType)
enum class EModelGridMeshCollisionMode : uint8
{
	/**  */
	NoCollision = 0,
	/** */
	ComplexAsSimple = 1
};



UCLASS(ConversionRoot, ComponentWrapperClass, ClassGroup = GradientspaceGrid, meta = (ChildCanTick), Experimental)
class GRADIENTSPACEUESCENE_API AGSModelGridActor : public AActor
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(Category = ModelGridActor, VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UGSModelGridComponent> GridComponent;

	// this component is used for visualization of the GridComponent
	// It should not be serialized/etc. *However* this UProperty markup is not
	// respected, as the Component is not a DefaultSubObject, it is created
	// as needed and I guess that means these flags are ignored
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient, NonTransactional, SkipSerialization, Category = ModelGridActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|StaticMesh", AllowPrivateAccess = "true"))
	TObjectPtr<UDynamicMeshComponent> PreviewMeshComponent;

public:
	UFUNCTION(BlueprintCallable, Category = ModelGridActor)
	UGSModelGridComponent* GetGridComponent() const { return GridComponent; }

	UFUNCTION(BlueprintCallable, Category = ModelGridActor)
	UGSModelGrid* GetGrid() const { return GridComponent->GetGrid(); }


public:
	UPROPERTY(Category = ModelGridActor, EditAnywhere, BlueprintReadWrite)
	bool bEnablePreviewMesh = true;

	virtual void UpdateGridPreviewMesh(UE::Geometry::FDynamicMesh3&& PreviewMesh, TArray<UMaterialInterface*>&& Materials);

	virtual UDynamicMeshComponent* GetPreviewMeshComponent() const { return PreviewMeshComponent; }


	UPROPERTY(Category = ModelGridActor, EditAnywhere, BlueprintReadWrite)
	EModelGridMeshCollisionMode PreviewMeshCollisionMode = EModelGridMeshCollisionMode::ComplexAsSimple;

	UFUNCTION(BlueprintCallable, Category=ModelGridActor)
	void SetPreviewMeshCollisionMode(EModelGridMeshCollisionMode NewMode);



protected:
	void OnModelGridChanged(UGSModelGridComponent* Component, UGSModelGrid* ModelGrid);
	FDelegateHandle OnModelGridChanged_Handle;

protected:
	virtual void PostLoad() override;
	virtual void PostInitializeComponents() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;

	bool bPreviewMeshWasNullInLastPreEditUndo = false;		// used for hacks to workaround undo/redo problems
#endif

	virtual void UpdatePreviewMeshState();
	virtual void UpdatePreviewMeshCollisionState();
};
