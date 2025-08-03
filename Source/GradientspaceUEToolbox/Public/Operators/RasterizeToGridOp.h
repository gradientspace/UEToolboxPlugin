// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/MeshSharingUtil.h"
#include "ModelingOperators.h"

namespace GS
{

class ModelGrid;
using namespace UE::Geometry;


class GRADIENTSPACEUETOOLBOX_API FRasterizeToGridOp : public FDynamicMeshOperator
{
public:
	virtual ~FRasterizeToGridOp() {}

	/** Handle to the source DynamicMeshes. Note: assuming vertex colors on these meshes are in Linear color space. */
	TArray<TSharedPtr<FSharedConstDynamicMesh3>> OriginalMeshesShared;
	TArray<FTransformSRT3d> WorldTransforms;

	double WindingIso = 0.5;
	FVector3d CellDimensions = FVector3d(10,10,10);

	//! if true, sample vertex colors from input mesh
	bool bSampleVertexColors = true;

	//! if true, colors set in resulting grid will be SRGB-encoded 
	bool bOutputSRGBColors = true;

	TSharedPtr<ModelGrid> ResultGrid;

	//
	// FDynamicMeshOperator implementation
	// 

	virtual void CalculateResult(FProgressCancel* Progress) override;

	virtual bool CalculateResultInPlace(FDynamicMesh3& EditMesh, FProgressCancel* Progress);
};



} // end namespace GS
