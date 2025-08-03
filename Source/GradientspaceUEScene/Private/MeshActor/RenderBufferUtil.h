// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"

namespace GS { class DenseMesh; }
namespace GS { class FMeshRenderBuffers; }

namespace GS
{
	GRADIENTSPACEUESCENE_API void InitializeRenderBuffersFromMesh(
		const DenseMesh& Mesh,
		FMeshRenderBuffers& RenderBuffers);

	// 3 new vertices per triangle, no optimization at all
	GRADIENTSPACEUESCENE_API void InitializeRenderBuffersFromMesh_Fastest(
		const DenseMesh& Mesh,
		FMeshRenderBuffers& RenderBuffers);

	// LRU cache used to try to re-use recently-seen unique vertices
	GRADIENTSPACEUESCENE_API void InitializeRenderBuffersFromMesh_LocalOptimize(
		const DenseMesh& Mesh,
		FMeshRenderBuffers& RenderBuffers);


} // end namespace GS
