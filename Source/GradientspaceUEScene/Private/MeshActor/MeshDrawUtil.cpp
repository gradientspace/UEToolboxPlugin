// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshActor/MeshDrawUtil.h"
#include "MeshActor/DefaultMeshRenderBuffers.h"

#include "SceneInterface.h"
#include "SceneManagement.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/MaterialRenderProxy.h"
#include "Materials/Material.h"
#include "ShowFlags.h"
#include "Engine/Engine.h" // for GEngine
#include "Misc/EngineVersionComparison.h"

using namespace GS;

FMaterialRenderProxy* GS::GetWireframeMaterialProxy(
	const FPrimitiveSceneProxy* SceneProxy,
	const FEngineShowFlags& EngineShowFlags,
	const FLinearColor& BaseWireframeColor)
{
	FColoredMaterialRenderProxy* WireframeMaterialInstance = nullptr;
	if (GEngine->WireframeMaterial != nullptr)
	{
		FLinearColor UseColor = GetSelectionColor(BaseWireframeColor,
			!(GIsEditor && EngineShowFlags.Selection) || SceneProxy->IsSelected(), 
			SceneProxy->IsHovered(), false);


		WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial->GetRenderProxy(), UseColor);

		return WireframeMaterialInstance;
	}
	return nullptr;
}


void GS::InitializeDynamicDrawMeshBatch(
	const FPrimitiveSceneProxy* SceneProxy,
	const FMaterialRenderProxy* MaterialProxy,
	const FMeshRenderBuffers& RenderBuffers,
	FMeshBatch& MeshBatch,
	FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer,
	bool bWireframe,
	FRHICommandList* UseRHICommandList)
{
	FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
	BatchElement.IndexBuffer = &RenderBuffers.IndexBuffer;
	MeshBatch.bWireframe = bWireframe;
	MeshBatch.VertexFactory = &RenderBuffers.VertexFactory;
	MeshBatch.MaterialRenderProxy = MaterialProxy;

	bool bHasPrecomputedVolumetricLightmap;
	FMatrix PreviousLocalToWorld;
	int32 SingleCaptureIndex;
	bool bOutputVelocity;
	SceneProxy->GetScene().GetPrimitiveUniformShaderParameters_RenderThread(
		SceneProxy->GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
	bOutputVelocity |= SceneProxy->AlwaysHasVelocity();

	const FMatrix& LocalToWorld = SceneProxy->GetLocalToWorld();
	const FBoxSphereBounds& WorldBounds = SceneProxy->GetBounds();
	const FBoxSphereBounds& LocalBounds = SceneProxy->GetLocalBounds();
	const FBoxSphereBounds& PreSkinnedLocalBounds = SceneProxy->GetLocalBounds();
	const FCustomPrimitiveData* CustomPrimitiveData = SceneProxy->GetCustomPrimitiveData();
#if UE_VERSION_OLDER_THAN(5,4,0)
	DynamicPrimitiveUniformBuffer.Set(
		LocalToWorld, PreviousLocalToWorld,
		WorldBounds, LocalBounds, PreSkinnedLocalBounds,
		SceneProxy->ReceivesDecals(), bHasPrecomputedVolumetricLightmap, bOutputVelocity, CustomPrimitiveData);
#else
	check(UseRHICommandList != nullptr);
	DynamicPrimitiveUniformBuffer.Set(*UseRHICommandList,
		LocalToWorld, PreviousLocalToWorld,
		WorldBounds, LocalBounds, PreSkinnedLocalBounds,
		SceneProxy->ReceivesDecals(), bHasPrecomputedVolumetricLightmap, bOutputVelocity, CustomPrimitiveData);
#endif
	BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

	BatchElement.FirstIndex = 0;
	BatchElement.NumPrimitives = RenderBuffers.IndexBuffer.Indices.Num() / 3;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = RenderBuffers.PositionVertexBuffer.GetNumVertices() - 1;
	MeshBatch.ReverseCulling = SceneProxy->IsLocalToWorldDeterminantNegative();
	MeshBatch.Type = PT_TriangleList;
	MeshBatch.DepthPriorityGroup = SDPG_World;

	MeshBatch.bCanApplyViewModeOverrides = false;
}
