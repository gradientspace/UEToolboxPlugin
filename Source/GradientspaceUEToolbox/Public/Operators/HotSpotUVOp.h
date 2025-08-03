// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/MeshSharingUtil.h"
#include "ModelingOperators.h"
#include "Polygon2.h"

namespace UE::Geometry
{


class GRADIENTSPACEUETOOLBOX_API FHotSpotSet
{
public:
	struct FHotSpotPatch
	{
		FAxisAlignedBox2d ShapeBounds;
		FPolygon2d ShapeBoundary;

		FAxisAlignedBox2d UVBounds;
	};

	TArray<FHotSpotPatch> Patches;
};


class GRADIENTSPACEUETOOLBOX_API FHotSpotUVOp : public FDynamicMeshOperator
{
public:
	virtual ~FHotSpotUVOp() {}


	/** Handle to the source DynamicMesh */
	TSharedPtr<FSharedConstDynamicMesh3> OriginalMeshShared;

	FTransformSRT3d WorldTransform;

	FHotSpotSet HotSpots;

	double ScaleFactor = 1.0;

	/** Triangles of the OriginalMesh that should be extruded */
	TOptional<TArray<int32>> TriangleSelection;

	// UV layer
	int32 UVLayer = 0;

	int32 Debug_ShiftSelection = 0;

	//
	// FDynamicMeshOperator implementation
	// 

	virtual void CalculateResult(FProgressCancel* Progress) override;

	virtual bool CalculateResultInPlace(FDynamicMesh3& EditMesh, FProgressCancel* Progress);
};


} // end namespace UE::Geometry


