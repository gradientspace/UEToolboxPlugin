// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Math/Vector.h"
#include "IndexTypes.h"

#include "Math/GSVector2.h"
#include "Math/GSVector3.h"
#include "Math/GSIndex3.h"
#include "Core/GSError.h"


namespace UE::Geometry { class FDynamicMesh3; }
namespace GS { class DenseMesh; }
namespace GS { class PolyMesh; }

namespace GS
{
	using namespace UE::Geometry;

	GRADIENTSPACEUECORE_API void ConvertDynamicMeshToDenseMesh(const FDynamicMesh3& DynamicMesh, DenseMesh& DenseMesh);
	
	GRADIENTSPACEUECORE_API void ConvertDynamicMeshToPolyMesh(const FDynamicMesh3& DynamicMesh, PolyMesh& PolyMesh);

	GRADIENTSPACEUECORE_API bool ConvertDynamicMeshGroupsToPolyMesh(
		const FDynamicMesh3& DynamicMesh, 
		int GroupLayer, 
		PolyMesh& PolyMesh,
		GSErrorSet* Errors = nullptr );

	//! warning: handling of polygons not currently supported!
	GRADIENTSPACEUECORE_API void ConvertPolyMeshToDynamicMesh(const PolyMesh& PolyMesh, FDynamicMesh3& DynamicMesh);



	GRADIENTSPACEUECORE_API void AddUV1BarycentricsChannel(
		FDynamicMesh3& DynamicMesh);

}
