// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshActor/MultiFrameMeshSceneProxy.h"
#include "MeshActor/RenderBufferUtil.h"
#include "MeshActor/MeshDrawUtil.h"
#include "MeshActor/GSMultiFrameMeshComponent.h"

#include "MaterialDomain.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "Materials/Material.h"
#include "MeshBatch.h"

#include "Misc/EngineVersionComparison.h"

using namespace GS;



FMultiFrameMeshSceneProxy::FMultiFrameMeshSceneProxy(UGSMultiFrameMeshComponent* Component)
	: FPrimitiveSceneProxy(Component),
	MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
	bUseDynamicDrawPath = true; // Component->GetUseDynamicDrawPath();

	CurrentFrameIndex = Component->GetVisibleFrameIndex();
}

FMultiFrameMeshSceneProxy::~FMultiFrameMeshSceneProxy()
{
	check(IsInRenderingThread());

	// enqueue render thread command to free render buffers
	for (FMeshRenderBuffers* RenderBuffers : AllocatedRenderBuffers)
	{
		if (RenderBuffers && RenderBuffers->TriangleCount > 0)
		{
			FMeshRenderBuffers::EnqueueDeleteOnRenderThread(RenderBuffers);
		}
	}
	AllocatedRenderBuffers.Reset();
}


void FMultiFrameMeshSceneProxy::UpdateVisibleFrameIndex(int NewFrameIndex)
{
	FrameIndexLock.Lock();
	CurrentFrameIndex = NewFrameIndex;
	FrameIndexLock.Unlock();
}


void FMultiFrameMeshSceneProxy::Initialize(const TArray<GS::DenseMesh>& MeshFrames, UMaterialInterface* DefaultMaterial)
{
	int N = MeshFrames.Num();
	if (N == 0) return;

	AllocatedRenderBuffers.SetNum(N);
	for (int k = 0; k < N; ++k)
	{
		AllocatedRenderBuffers[k] = new FMeshRenderBuffers(GetScene().GetFeatureLevel());
		AllocatedRenderBuffers[k]->Material = 
			(DefaultMaterial != nullptr) ? DefaultMaterial : UMaterial::GetDefaultMaterial(MD_Surface);
	}

	// could probably build buffers in parallel...
	for (int k = 0; k < N; ++k)
	{
		if (AllocatedRenderBuffers[k] == nullptr) continue;

		const DenseMesh& Mesh = MeshFrames[k];
		int NumTriangles = Mesh.GetTriangleCount();
		if (NumTriangles == 0) continue;

		GS::InitializeRenderBuffersFromMesh_LocalOptimize(Mesh, *AllocatedRenderBuffers[k]);

		ENQUEUE_RENDER_COMMAND(FMultiFrameMeshSceneProxy_UploadFrame)(
			[this, k](FRHICommandListImmediate& RHICmdList)
		{
			AllocatedRenderBuffers[k]->Upload(RHICmdList);
		});
	}

}



void FMultiFrameMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
{
	if (AllocatedRenderBuffers.Num() == 0) return;

	FMultiFrameMeshSceneProxy& NonConstRef = const_cast<FMultiFrameMeshSceneProxy&>(*this);
	NonConstRef.FrameIndexLock.Lock();
	int UseFrameIndex = FMath::Clamp(CurrentFrameIndex, 0, AllocatedRenderBuffers.Num() - 1);
	NonConstRef.FrameIndexLock.Unlock();

	FMeshRenderBuffers* FrameRenderBuffers = AllocatedRenderBuffers[UseFrameIndex];
	if (FrameRenderBuffers == nullptr) return;
	if (FrameRenderBuffers->TriangleCount == 0) return;

	// collect materials

	UMaterialInterface* UseMaterial = (FrameRenderBuffers->Material != nullptr) ?
		FrameRenderBuffers->Material : UMaterial::GetDefaultMaterial(MD_Surface);
	FMaterialRenderProxy* UseMaterialProxy = nullptr;

	const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;
	bool bWireframe = (AllowDebugViewmodes() && EngineShowFlags.Wireframe);
	if (bWireframe)
	{
		if (FMaterialRenderProxy* WireframeMaterialProxy = GS::GetWireframeMaterialProxy(this, EngineShowFlags, GetWireframeColor()))
		{
			Collector.RegisterOneFrameMaterialProxy(WireframeMaterialProxy);
			UseMaterialProxy = WireframeMaterialProxy;
		}
	}
	else
	{
		UseMaterialProxy = (UseMaterial) ? UseMaterial->GetRenderProxy() : nullptr;
	}
	if (!UseMaterialProxy) return;


	// draw mesh batch in each view
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			//const FSceneView* View = Views[ViewIndex];

#if UE_VERSION_OLDER_THAN(5,4,0)
			FRHICommandList* UseCommandList = nullptr;
#else
			FRHICommandList* UseCommandList = &Collector.GetRHICommandList();
#endif

			FMeshBatch& MeshBatch = Collector.AllocateMesh();
			FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
			GS::InitializeDynamicDrawMeshBatch(this, UseMaterialProxy, *FrameRenderBuffers, MeshBatch, DynamicPrimitiveUniformBuffer, bWireframe, UseCommandList);
			Collector.AddMesh(ViewIndex, MeshBatch);
		}
	}
}





FPrimitiveViewRelevance FMultiFrameMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);

	// need to figure out how to hide submitted batches for static draw
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;

	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}



#if RHI_RAYTRACING
bool FMultiFrameMeshSceneProxy::IsRayTracingRelevant() const
{
	return false;
}
bool FMultiFrameMeshSceneProxy::HasRayTracingRepresentation() const
{
	return false;
}
#endif
