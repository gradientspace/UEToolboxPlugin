// Copyright Gradientspace Corp. All Rights Reserved.
#include "ModelGridMeshingFunctions.h"
#include "GradientspaceScriptUtil.h"

#include "Operators/ModelGridMeshingOp.h"

using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UGSScriptLibrary_ModelGridMeshing"


UGSModelGrid*  UGSScriptLibrary_ModelGridMeshing::GenerateMeshForModelGridV1(
	UGSModelGrid* TargetGrid,
	UDynamicMesh* TargetMesh,
	const FModelGridMeshingOptionsV1& Options)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGrid, TEXT("GenerateMeshForModelGridV1"));
	CHECK_OBJ_VALID_OR_RETURN_OTHER(TargetMesh, TargetGrid, TEXT("TargetMesh"), TEXT("GenerateMeshForModelGridV1"));

	TSharedPtr<FModelGridMeshingOp::FModelGridData> SourceData = MakeShared<FModelGridMeshingOp::FModelGridData>();
	TargetGrid->ProcessGrid([&](const GS::ModelGrid& Grid) {
		SourceData->SourceGrid = Grid;
	});

	FModelGridMeshingOp MeshingOp;
	MeshingOp.SourceData = SourceData;
	MeshingOp.bRemoveCoincidentFaces = ( (int)Options.HiddenRemoval >= (int)EModelGridHiddenRemovalStrategy::RemoveIdenticalFacePairs );
	MeshingOp.bSelfUnion = (Options.HiddenRemoval == EModelGridHiddenRemovalStrategy::RemoveAllHiddenAreas);
	MeshingOp.bPreserveColorBorders = Options.bPreserveColorBorders;
	MeshingOp.bPreserveMaterialBorders = Options.bPreserveMaterialBorders;
	MeshingOp.bOptimizePlanarAreas = Options.bSimplifyMesh;
	MeshingOp.bRecomputeGroups = Options.bSimplifyMesh;
	MeshingOp.GroupAngleThreshDeg = 2.0;

	// these can be done w/ geometry script and don't need to be done here...
	MeshingOp.bInvertFaces = false;
	MeshingOp.UVMode = FModelGridMeshingOp::EUVMode::None;
	MeshingOp.TargetUVResolution = 512;

	FDynamicMesh3 ResultMesh;
	MeshingOp.CalculateResultInPlace(ResultMesh, nullptr);
	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) {
		EditMesh = MoveTemp(ResultMesh);
	});

	return TargetGrid;
}



#undef LOCTEXT_NAMESPACE