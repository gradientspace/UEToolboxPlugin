// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Math/Vector4.h"

namespace UE::Geometry { 
	class FDynamicMesh3; 
	template<class MeshType> class TMeshAABBTree3;
	typedef TMeshAABBTree3<FDynamicMesh3> FDynamicMeshAABBTree3;
	template<class ImageType> class TImageBuilder;
}



namespace GS
{
using namespace UE::Geometry;

GRADIENTSPACEUECORE_API
void BakeMeshVertexColorsToImage(
	const FDynamicMesh3& Mesh,
	int ImageResolution,
	double ProjectionDistance,
	int SamplesPerPixel,
	bool bLinearFiltering,
	TImageBuilder<FVector4f>& ResultImageOut,
	FDynamicMeshAABBTree3* MeshSpatial = nullptr,
	int UseUVLayer = 0);





}