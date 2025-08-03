// Copyright Gradientspace Corp. All Rights Reserved.
#include "Operators/RasterizeToGridOp.h"

#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Spatial/FastWinding.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMeshEditor.h"

#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridEditMachine.h"
#include "ModelGrid/ModelGridMeshCache.h"
#include "GenericGrid/RasterizeToGrid.h"
#include "Utility/GSUEModelGridUtil.h"



using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "RasterizeToGridOp"


void FRasterizeToGridOp::CalculateResult(FProgressCancel* Progress)
{
	*ResultMesh = FDynamicMesh3();
	// todo

	CalculateResultInPlace(*ResultMesh, Progress);
}


template<typename TransformType>
static FVector3d GetAverageTranslation(TArrayView<const TransformType> Transforms)
{
	if (Transforms.IsEmpty())
	{
		return FVector3d::ZeroVector;
	}

	FVector3d Avg(0, 0, 0);
	for (const FTransformSRT3d& Transform : Transforms)
	{
		Avg += Transform.GetTranslation();
	}
	return Avg / double(Transforms.Num());
}



bool FRasterizeToGridOp::CalculateResultInPlace(FDynamicMesh3& EditMesh, FProgressCancel* Progress)
{
	if (Progress && Progress->Cancelled()) { return false; }

	FDynamicMesh3 CombinedMesh;

	FDynamicMeshColorOverlay* PrimaryColors = nullptr;
	if (bSampleVertexColors)
	{
		CombinedMesh.EnableAttributes();
		CombinedMesh.Attributes()->EnablePrimaryColors();
		PrimaryColors = CombinedMesh.Attributes()->PrimaryColors();
	}

	int NumMeshes = OriginalMeshesShared.Num();
	if (NumMeshes != WorldTransforms.Num()) { return false; }

	// accumulate meshes  (this should be done as a preprocess and cached....)
	FDynamicMeshEditor Editor(&CombinedMesh);
	FMeshIndexMappings IndexMaps;
	for (int k = 0; k < NumMeshes; ++k)
	{
		FTransformSRT3d Transform = WorldTransforms[k];
		IndexMaps.Reset();

		OriginalMeshesShared[k]->AccessSharedObject([&](const FDynamicMesh3& Mesh)
		{
			Editor.AppendMesh(&Mesh, IndexMaps, [&](int vid, const FVector3d& Pos) {
				return Transform.TransformPosition(Pos); 
			});

			if (Transform.GetDeterminant() < 0) {
				for (int tid : Mesh.TriangleIndicesItr()) {
					CombinedMesh.ReverseTriOrientation(IndexMaps.GetNewTriangle(tid));
				}
			}
		});

		if (Progress && Progress->Cancelled()) { return false; }
	}

	if (CombinedMesh.TriangleCount() == 0) {
		return false;
	}
	if (Progress && Progress->Cancelled()) { return false; }

	FAxisAlignedBox3d MeshBounds = CombinedMesh.GetBounds();
	FVector3d WorldMeshOrigin = MeshBounds.Center() - MeshBounds.Extents().Z * FVector3d::UnitZ();
	MeshTransforms::Translate(CombinedMesh, -WorldMeshOrigin);

	ResultTransform = FTransform3d(WorldMeshOrigin);

	FDynamicMeshAABBTree3 Spatial(&CombinedMesh);
	if (Progress && Progress->Cancelled()) { return false; }
	TFastWindingTree<FDynamicMesh3> FastWinding(&Spatial);
	if (Progress && Progress->Cancelled()) { return false; }

	FVector3d CellDims = CellDimensions;
	//double DiagThresh = CellDims.Length() * 2;
	double DiagThresh = 9999999.0;

	ModelGrid NewGrid;
	NewGrid.Initialize(CellDims);
	ModelGridEditMachine GridEditor;
	GridEditor.SetCurrentGrid(NewGrid);
	if (Progress && Progress->Cancelled()) { return false; }

	MeshBounds = Spatial.GetBoundingBox();
	FVector3d GridOrigin = MeshBounds.Center() - MeshBounds.Extents().Z * FVector3d::UnitZ();
	FFrame3d MeshFrame;
	MeshFrame.Origin = GridOrigin;
	FAxisAlignedBox3d MeshBoundsInFrame = FAxisAlignedBox3d::Empty();
	for (FVector3d Pos : CombinedMesh.VerticesItr())
	{
		MeshBoundsInFrame.Contain(Pos);
	}
	if (Progress && Progress->Cancelled()) { return false; }

	UniformGridAdapter Adapter;
	GridEditor.InitializeUniformGridAdapter(Adapter);
	GridEditor.BeginExternalEdit();

	RasterizeToGrid Rasterizer;
	Rasterizer.SetGrid(&Adapter);
	Rasterizer.SetBounds((Frame3d)MeshFrame, (AxisBox3d)MeshBoundsInFrame);

	Rasterizer.DefaultColor = GS::Color3b::White();
	if (bSampleVertexColors)
	{
		Rasterizer.ColorSampleFunc = [&](const FVector3d& Pos, Color3b& ResultColor) -> bool
		{
			double NearDistSqr;
			int NearTID = Spatial.FindNearestTriangle(Pos, NearDistSqr);
			FDistPoint3Triangle3d Dist = TMeshQueries<FDynamicMesh3>::TriangleDistance(CombinedMesh, NearTID, Pos);
			if (Dist.GetSquared() < DiagThresh * DiagThresh)
			{
				FVector4d InterpColor(1, 1, 1, 1);

				if (PrimaryColors->IsSetTriangle(NearTID))
					PrimaryColors->GetTriBaryInterpolate<double>(NearTID, &Dist.TriangleBaryCoords.X, &InterpColor.X);
				FLinearColor LinearColor = ToLinearColor(InterpColor);
				ResultColor = (Color3b)LinearColor.ToFColor(bOutputSRGBColors);
				return true;
			}
			return false;
		};
	}

	Rasterizer.BinaryRasterize([&](const Vector3d& Pos)
	{
		return FastWinding.IsInside(Pos, WindingIso);
	}, true);
	if (Progress && Progress->Cancelled()) { return false; }

	GridEditor.EndCurrentInteraction();

	EditMesh = FDynamicMesh3();
	// no material or UV for now
	GS::ExtractGridFullMesh(NewGrid, EditMesh, false, false, Progress);

	ResultGrid = MakeShared<ModelGrid>();
	*ResultGrid = MoveTemp(NewGrid);

	return true;
}







#undef LOCTEXT_NAMESPACE
