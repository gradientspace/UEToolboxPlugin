// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Rendering/StaticMeshVertexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/ColorVertexBuffer.h"
#include "DynamicMeshBuilder.h"		// FDynamicMeshIndexBuffer32
#include "LocalVertexFactory.h"

class UMaterialInterface;


namespace GS
{

class FMeshRenderBuffers
{
public:
	int TriangleCount = 0;

	FStaticMeshVertexBuffer StaticMeshVertexBuffer;
	FPositionVertexBuffer PositionVertexBuffer;
	FColorVertexBuffer ColorVertexBuffer;

	FDynamicMeshIndexBuffer32 IndexBuffer;

	FLocalVertexFactory VertexFactory;

	UMaterialInterface* Material = nullptr;


	FMeshRenderBuffers(ERHIFeatureLevel::Type FeatureLevelType);

	virtual ~FMeshRenderBuffers();

	void Upload(FRHICommandListImmediate& RHICmdList);

	void InitOrUpdateResource(FRHICommandListImmediate& RHICmdList, FRenderResource* Resource);

	//! delete render buffers on rendering thread
	static void EnqueueDeleteOnRenderThread(FMeshRenderBuffers* RenderBuffers);

};


} // end namespace GS
