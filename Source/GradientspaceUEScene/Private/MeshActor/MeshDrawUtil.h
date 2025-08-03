// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Math/Color.h"

class FPrimitiveSceneProxy;
class FMaterialRenderProxy;
struct FEngineShowFlags;
struct FMeshBatch;
class FDynamicPrimitiveUniformBuffer;
class FRHICommandList;
namespace GS { class FMeshRenderBuffers; }

namespace GS
{

	GRADIENTSPACEUESCENE_API FMaterialRenderProxy* GetWireframeMaterialProxy(
		const FPrimitiveSceneProxy* SceneProxy,
		const FEngineShowFlags& ShowFlags,
		const FLinearColor& BaseWireframeColor );


	GRADIENTSPACEUESCENE_API void InitializeDynamicDrawMeshBatch(
		const FPrimitiveSceneProxy* SceneProxy,
		const FMaterialRenderProxy* MaterialProxy,
		const FMeshRenderBuffers& RenderBuffers,
		FMeshBatch& MeshBatch,
		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer,
		bool bWireframe,
		FRHICommandList* UseRHICommandList = nullptr);


} // end namespace GS
