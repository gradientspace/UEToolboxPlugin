// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "DynamicMesh/MeshSharingUtil.h"
#include "ModelingOperators.h"
#include "ModelGrid/ModelGrid.h"

namespace UE::Geometry { class FDynamicMesh3; }

namespace GS
{

class ModelGrid;
using namespace UE::Geometry;

class GRADIENTSPACEUECORE_API FModelGridMeshingOp : public FDynamicMeshOperator
{
public:

	struct FModelGridData
	{
		ModelGrid SourceGrid;
	};


public:
	virtual ~FModelGridMeshingOp() {}

	TSharedPtr<FModelGridData> SourceData;
	bool bInvertFaces = false;
	bool bRemoveCoincidentFaces = false;
	bool bSelfUnion = false;
	bool bOptimizePlanarAreas = false;

	bool bPreserveColorBorders = true;
	bool bPreserveMaterialBorders = true;

	bool bRecomputeGroups = false;
	double GroupAngleThreshDeg = 2.0;

	enum EUVMode
	{
		None = 0,
		Discard = 1,
		Repack = 2,
		PixelLayoutRepack = 3
	};

	EUVMode UVMode = EUVMode::None;
	int TargetUVResolution = 512;
	int DimensionPixelCount = 4;		// defines number of pixels along standard grid cell dimension for PixelLayoutRepack mode
	int UVIslandPixelBorder = 1;

	int PixelLayoutImageDimensionX_Result = 0;
	int PixelLayoutImageDimensionY_Result = 0;

	virtual void CalculateResult(FProgressCancel* Progress) override;

	virtual bool CalculateResultInPlace(FDynamicMesh3& EditMesh, FProgressCancel* Progress);
};


}
