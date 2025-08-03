// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Components/MeshComponent.h"
#include "Mesh/DenseMesh.h"

#include "GSMeshComponent.generated.h"

/**
 * Gradientspace Mesh Component
 */
UCLASS(ClassGroup = Rendering, Experimental)
class GRADIENTSPACEUESCENE_API UGSMeshComponent : public UMeshComponent
{
	GENERATED_BODY()
public:

	UGSMeshComponent();

	void ProcessMesh(TFunctionRef<void(const GS::DenseMesh& Mesh)> ProcessFunc) const;

	void EditMesh(TFunctionRef<void(GS::DenseMesh& Mesh)> UpdateFunc);



public:
	UFUNCTION(BlueprintCallable, Category = "Gradientspace Mesh")
	void SetUseDynamicDrawPath(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Gradientspace Mesh")
	bool GetUseDynamicDrawPath() const { return bUseDynamicDrawPath; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetUseDynamicDrawPath, BlueprintGetter = GetUseDynamicDrawPath, Category = "Gradientspace Mesh")
	bool bUseDynamicDrawPath = false;


protected:
	TUniquePtr<GS::DenseMesh> Mesh;
	FBox LocalBounds;

protected:
	// UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	// USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
};
