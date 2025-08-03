// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Math/Vector.h"
#include "BoxTypes.h"


namespace GS
{



template<typename MeshType>
UE::Geometry::FAxisAlignedBox3d ComputeMeshVerticesBounds(const MeshType& Mesh, TFunctionRef<FVector3d(const FVector3d&)> TransformFunc)
{
	UE::Geometry::FAxisAlignedBox3d Bounds = UE::Geometry::FAxisAlignedBox3d::Empty();
	int32 NV = Mesh.VertexCount();
	for (int32 k = 0; k < NV; ++k) {
		if (Mesh.IsVertex(k))
			Bounds.Contain(TransformFunc(Mesh.GetVertex(k)));
	}
	return Bounds;
}



}
