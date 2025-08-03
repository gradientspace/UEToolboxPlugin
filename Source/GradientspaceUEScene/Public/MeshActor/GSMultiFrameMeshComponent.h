// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Components/MeshComponent.h"
#include "Mesh/DenseMesh.h"

#include "GSMultiFrameMeshComponent.generated.h"

/**
 * Gradientspace Multi-Frame Mesh Component
 */
UCLASS(ClassGroup = Rendering, Experimental)
class GRADIENTSPACEUESCENE_API UGSMultiFrameMeshComponent : public UMeshComponent
{
	GENERATED_BODY()
public:
	UGSMultiFrameMeshComponent();

	void SetMeshFrames(TArray<GS::DenseMesh>&& MeshFrames);

	UFUNCTION(BlueprintCallable, Category = "MultiFrame Mesh Component")
	int GetNumFrames() const;

	UFUNCTION(BlueprintCallable, Category="MultiFrame Mesh Component")
	void SetCurrentFrameIndex(int NewIndex);

	UFUNCTION(BlueprintCallable, Category = "MultiFrame Mesh Component")
	int GetCurrentFrameIndex() const { return CurrentFrameIndex; }

	//! overrides the CurrentFrameIndex if >= 0. Value is interpreted modulo frame count, ie it loops
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MultiFrame Mesh Component", meta=(UIMin=-1, UIMax=100))
	int OverrideFrameIndex = -1;

	UFUNCTION(BlueprintCallable, Category = "MultiFrame Mesh Component")
	void SetOverrideFrameIndex(int NewIndex);

	UFUNCTION(BlueprintCallable, Category = "MultiFrame Mesh Component")
	void ClearOverrideFrameIndex();

	//! takes CurrentFrameIndex and OverrideFrameIndex into account
	UFUNCTION(BlueprintCallable, Category = "MultiFrame Mesh Component")
	int GetVisibleFrameIndex() const;



public:
	// UMeshComponent Material Interface
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual FMaterialRelevance GetMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;

	UFUNCTION(BlueprintCallable, Category = "MultiFrame Mesh Component")
	void SetBaseMaterial(UMaterialInterface* Material);

protected:
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;


protected:
	TArray<GS::DenseMesh> MeshFrames;
	FBox LocalBounds;

	int CurrentFrameIndex = 0;

	virtual void UpdateFrameInSceneProxy();

protected:
	// UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	// USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};
