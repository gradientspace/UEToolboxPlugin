// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "PrimitiveSceneProxy.h"
#include "MeshActor/DefaultMeshRenderBuffers.h"
#include "HAL/CriticalSection.h"

#include "Mesh/DenseMesh.h"

class UGSMultiFrameMeshComponent;

namespace GS
{

class FMultiFrameMeshSceneProxy : public FPrimitiveSceneProxy
{
protected:
	FMaterialRelevance MaterialRelevance;

	TArray<FMeshRenderBuffers*> AllocatedRenderBuffers;

	bool bUseDynamicDrawPath = false;

	int CurrentFrameIndex = 0;
	FCriticalSection FrameIndexLock;

public:
	FMultiFrameMeshSceneProxy(UGSMultiFrameMeshComponent* Component);
	virtual ~FMultiFrameMeshSceneProxy();

	// API for SceneComponent
	void Initialize(
		const TArray<GS::DenseMesh>& MeshFrames,
		UMaterialInterface* DefaultMaterial	);

	// thread-safe
	void UpdateVisibleFrameIndex(int NewFrameIndex);

public:
	// SceneProxy API implementation

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;

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
