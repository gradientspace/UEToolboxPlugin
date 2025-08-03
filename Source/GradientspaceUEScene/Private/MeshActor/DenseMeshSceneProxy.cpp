// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshActor/DenseMeshSceneProxy.h"
#include "MeshActor/RenderBufferUtil.h"
#include "MeshActor/MeshDrawUtil.h"
#include "MeshActor/GSMeshComponent.h"

#include "MaterialDomain.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "Materials/Material.h"
#include "MeshBatch.h"

#include "Misc/EngineVersionComparison.h"

using namespace GS;



FDenseMeshSceneProxy::FDenseMeshSceneProxy(UGSMeshComponent* Component)
	: FPrimitiveSceneProxy(Component),
	MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
	bUseDynamicDrawPath = Component->GetUseDynamicDrawPath();
}

FDenseMeshSceneProxy::~FDenseMeshSceneProxy()
{
	check(IsInRenderingThread());

	// enqueue render thread command to free render buffers
	if (AllocatedRenderBuffers && AllocatedRenderBuffers->TriangleCount > 0)
	{
		FMeshRenderBuffers::EnqueueDeleteOnRenderThread(AllocatedRenderBuffers);
		AllocatedRenderBuffers = nullptr;
	}
}


void FDenseMeshSceneProxy::InitializeRenderBuffers()
{
	AllocatedRenderBuffers = new FMeshRenderBuffers(GetScene().GetFeatureLevel());
	AllocatedRenderBuffers->Material = UMaterial::GetDefaultMaterial(MD_Surface);
}


void FDenseMeshSceneProxy::InitializeFromMesh_Fastest(const DenseMesh& Mesh)
{
	int NumTriangles = Mesh.GetTriangleCount();
	if (NumTriangles == 0) return;

	InitializeRenderBuffers();
	if (AllocatedRenderBuffers != nullptr)
	{
		GS::InitializeRenderBuffersFromMesh_Fastest(Mesh, *AllocatedRenderBuffers);

		ENQUEUE_RENDER_COMMAND(FDenseMeshSceneProxy_Upload)(
			[this](FRHICommandListImmediate& RHICmdList) {
			AllocatedRenderBuffers->Upload(RHICmdList);
		});
	}
}



void FDenseMeshSceneProxy::InitializeFromMesh_LocalOptimize(const DenseMesh& Mesh)
{
	int NumTriangles = Mesh.GetTriangleCount();
	if (NumTriangles == 0) return;

	InitializeRenderBuffers();
	if (AllocatedRenderBuffers != nullptr)
	{
		GS::InitializeRenderBuffersFromMesh_LocalOptimize(Mesh, *AllocatedRenderBuffers);

		ENQUEUE_RENDER_COMMAND(FDenseMeshSceneProxy_Upload)(
			[this](FRHICommandListImmediate& RHICmdList) {
			AllocatedRenderBuffers->Upload(RHICmdList);
		});
	}
}


void FDenseMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
{
	if (AllocatedRenderBuffers == nullptr) return;

	// collect materials

	UMaterialInterface* UseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	FMaterialRenderProxy* UseMaterialProxy = nullptr;

	const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;
	bool bWireframe = (AllowDebugViewmodes() && EngineShowFlags.Wireframe);
	if ( bWireframe )
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
#if UE_VERSION_OLDER_THAN(5,4,0)
			FRHICommandList* UseCommandList = nullptr;
#else
			FRHICommandList* UseCommandList = &Collector.GetRHICommandList();
#endif
			//const FSceneView* View = Views[ViewIndex];  // unused?
			FMeshBatch& MeshBatch = Collector.AllocateMesh();
			FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
			GS::InitializeDynamicDrawMeshBatch(this, UseMaterialProxy, *AllocatedRenderBuffers, MeshBatch, DynamicPrimitiveUniformBuffer, bWireframe, UseCommandList);
			Collector.AddMesh(ViewIndex, MeshBatch);
		}
	}
}




void FDenseMeshSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	checkSlow(IsInParallelRenderingThread());

	UMaterialInterface* UseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	FMaterialRenderProxy* MaterialProxy = UseMaterial->GetRenderProxy(); //bWireframe ? WireframeMaterialInstance : UseMaterial->GetRenderProxy();

	const int32 NumBatches = 1;
	PDI->ReserveMemoryForMeshes(NumBatches);

	FMeshBatch MeshBatch;
	MeshBatch.VertexFactory = &AllocatedRenderBuffers->VertexFactory;
	MeshBatch.MaterialRenderProxy = MaterialProxy;

	MeshBatch.bWireframe = false;
	MeshBatch.CastShadow = true;
	MeshBatch.bUseForDepthPass = true;
	MeshBatch.bUseAsOccluder = ShouldUseAsOccluder() && GetScene().GetShadingPath() == EShadingPath::Deferred && !IsMovable();
	MeshBatch.bUseForMaterial = true;

	MeshBatch.Type = PT_TriangleList;

	MeshBatch.DepthPriorityGroup = SDPG_World;
	MeshBatch.LODIndex = 0;
	MeshBatch.bDitheredLODTransition = false;
	MeshBatch.bCanApplyViewModeOverrides = false;

	MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
	MeshBatch.bDisableBackfaceCulling = false;

	// static mesh does this
	//MeshBatch.SegmentIndex = SectionIndex;
	//MeshBatch.MeshIdInPrimitive = SectionIndex;

	FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
	BatchElement.IndexBuffer = &AllocatedRenderBuffers->IndexBuffer;
	BatchElement.FirstIndex = 0;
	BatchElement.NumPrimitives = AllocatedRenderBuffers->IndexBuffer.Indices.Num() / 3;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = AllocatedRenderBuffers->PositionVertexBuffer.GetNumVertices() - 1;

	BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
	// what is this for? static mesh proxy does it
	//BatchElement.VertexFactoryUserData = MeshBatch.VertexFactory.GetUniformBuffer();

	// see FInstancedStaticMeshSceneProxy::SetupInstancedMeshBatch...
	//BatchElement.NumInstances = 2;

	// seems like somehow we can put custom data in here? does it go to the material somehow?
	BatchElement.UserData = nullptr;

	//bool bHasPrecomputedVolumetricLightmap;
	//FMatrix PreviousLocalToWorld;
	//int32 SingleCaptureIndex;
	//bool bOutputVelocity;
	//GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
	//bOutputVelocity |= AlwaysHasVelocity();

	//FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
	//DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), ReceivesDecals(), bHasPrecomputedVolumetricLightmap, bOutputVelocity, GetCustomPrimitiveData());
	//BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

	// related to LOD support, see FStaticMeshSceneProxy::SetMeshElementScreenSize
	//MeshBatch.bDitheredLODTransition = false;
	//BatchElement.MaxScreenSize = 0.0f;
	//BatchElement.MinScreenSize = -1.0f;

#if RHI_RAYTRACING
	MeshBatch.CastRayTracedShadow = (MeshBatch.CastShadow && bCastDynamicShadow)?1:0;
#endif

	PDI->DrawMesh(MeshBatch, FLT_MAX);


	//if (OverlayMaterial != nullptr)
	//{
	//	FMeshBatch OverlayMeshBatch(MeshBatch);
	//	OverlayMeshBatch.bOverlayMaterial = true;
	//	OverlayMeshBatch.CastShadow = false;
	//	OverlayMeshBatch.bSelectable = false;
	//	OverlayMeshBatch.MaterialRenderProxy = OverlayMaterial->GetRenderProxy();
	//     // FIGURE OUT WHAT THIS MEANS!!
	//	// make sure overlay is always rendered on top of base mesh
	//	OverlayMeshBatch.MeshIdInPrimitive += LODModel.Sections.Num();
	//	PDI->DrawMesh(OverlayMeshBatch, FLT_MAX);
	//}

}





FPrimitiveViewRelevance FDenseMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);

	bool bRequiresDynamicDrawPath = false;
#if !(UE_BUILD_SHIPPING) || WITH_EDITOR
	bRequiresDynamicDrawPath = IsRichView(*View->Family);
	// todo: look at FStaticMeshSceneProxy::GetViewRelevance to see other force-dynamic-draw options
#endif

	bool bActuallyUseDynamicDrawPath = bUseDynamicDrawPath || bRequiresDynamicDrawPath;
	Result.bDynamicRelevance = bActuallyUseDynamicDrawPath;
	Result.bStaticRelevance = !bActuallyUseDynamicDrawPath;

	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}



#if RHI_RAYTRACING
bool FDenseMeshSceneProxy::IsRayTracingRelevant() const
{ 
	return false; 
}
bool FDenseMeshSceneProxy::HasRayTracingRepresentation() const 
{ 
	return false; 
}
#endif
