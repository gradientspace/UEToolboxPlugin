// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "PrimitiveSceneProxy.h"
#include "MeshActor/DefaultMeshRenderBuffers.h"

#include "Mesh/DenseMesh.h"

class UGSMeshComponent;

namespace GS
{

class FDenseMeshSceneProxy : public FPrimitiveSceneProxy
{
protected:
	FMaterialRelevance MaterialRelevance;

	FMeshRenderBuffers* AllocatedRenderBuffers;

	bool bUseDynamicDrawPath = false;

public:
	FDenseMeshSceneProxy(UGSMeshComponent* Component);
	virtual ~FDenseMeshSceneProxy();

	// API for SceneComponent
	void InitializeRenderBuffers();
	void InitializeFromMesh_Fastest(const DenseMesh& Mesh);
	void InitializeFromMesh_LocalOptimize(const DenseMesh& Mesh);


public:
	// SceneProxy API implementation

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual bool CanBeOccluded() const override	{
		return !MaterialRelevance.bDisableDepthTest;
	}

#if RHI_RAYTRACING
	virtual bool IsRayTracingRelevant() const override;
	virtual bool HasRayTracingRepresentation() const override;
#endif


	SIZE_T GetTypeHash() const override {
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}
	// todo improve this?
	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }
	uint32 GetAllocatedSize(void) const { return FPrimitiveSceneProxy::GetAllocatedSize(); }
};





} // end namespace GS
