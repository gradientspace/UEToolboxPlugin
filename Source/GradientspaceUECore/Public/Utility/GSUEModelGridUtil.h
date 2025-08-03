// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"
#include "Util/ProgressCancel.h"

#include "Core/SharedPointer.h"
#include "ModelGrid/MaterialReferenceSet.h"

namespace GS { class ModelGrid; }
namespace UE::Geometry { class FDynamicMesh3; }

namespace GS
{

GRADIENTSPACEUECORE_API
void ExtractGridFullMesh(
	const GS::ModelGrid& Grid,
	UE::Geometry::FDynamicMesh3& ResultMesh,
	bool bEnableMaterials,
	bool bEnableUVs,
	FProgressCancel* Progress = nullptr);

GRADIENTSPACEUECORE_API
void ExtractGridFullMesh(
	const GS::ModelGrid& Grid,
	UE::Geometry::FDynamicMesh3& ResultMesh,
	bool bEnableMaterials,
	bool bEnableUVs,
	GS::SharedPtr<ICellMaterialToIndexMap> GridMaterialMap,
	FProgressCancel* Progress = nullptr);
	
}
