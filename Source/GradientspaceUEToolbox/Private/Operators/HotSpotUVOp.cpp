// Copyright Gradientspace Corp. All Rights Reserved.
#include "Operators/HotSpotUVOp.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Selections/MeshConnectedComponents.h"
#include "Parameterization/DynamicMeshUVEditor.h"
#include "DynamicSubmesh3.h"
#include "MeshRegionBoundaryLoops.h"
#include "CompGeom/FitOrientedBox2.h"
#include "DynamicSubmesh3.h"
#include "Operations/MeshRegionOperator.h"
#include "MeshBoundaryLoops.h"
#include "Containers/ArrayView.h"
#include "MeshQueries.h"


using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "HotSpotUVOp"


void FHotSpotUVOp::CalculateResult(FProgressCancel* Progress)
{
	bool bMeshValid = OriginalMeshShared->AccessSharedObject([&](const FDynamicMesh3& Mesh)
	{
		*ResultMesh = Mesh;
	});

	if (bMeshValid)
	{
		CalculateResultInPlace(*ResultMesh, Progress);
	}


	//NewResultInfo = FGeometryResult(EGeometryResultType::InProgress);
	//bool bOK = false;

	//NewResultInfo.SetSuccess(bOK, Progress);
	//SetResultInfo(NewResultInfo);

}

UE_DISABLE_OPTIMIZATION
bool FHotSpotUVOp::CalculateResultInPlace(FDynamicMesh3& EditMesh, FProgressCancel* Progress)
{
	FDynamicMeshUVEditor UVEditor(&EditMesh, UVLayer, true);
	FDynamicMeshUVOverlay* UseOverlay = UVEditor.GetOverlay();
	if (ensure(UseOverlay != nullptr) == false)
	{
		return false;
	}

	if (Progress && Progress->Cancelled()) { return false; }



	FMeshConnectedComponents UVComponents(&EditMesh);
	UVComponents.FindConnectedTriangles([this](int32 CurTri, int32 NbrTri) {
		return ResultMesh->GetTriangleGroup(CurTri) == ResultMesh->GetTriangleGroup(NbrTri);
	});

	//UVComponents.FindConnectedTriangles([&](int32 Triangle0, int32 Triangle1) {
	//	return UVOverlay.AreTrianglesConnected(Triangle0, Triangle1);
	//});

	if (Progress && Progress->Cancelled()) { return false; }

	int32 NumPatches = HotSpots.Patches.Num();

	for (FMeshConnectedComponents::FComponent& UVRegion : UVComponents)
	{
		// compute initial projection frame for this patch based on average normal 
		FVector3d AvgNormal(0, 0, 0);
		FVector3d Centroid(0, 0, 0);
		double WeightSum = 0;
		for (int32 tid : UVRegion.Indices)
		{
			FVector3d TriNormal, TriCentroid; double TriArea;
			EditMesh.GetTriInfo(tid, TriNormal, TriArea, TriCentroid);
			Centroid += TriArea * TriCentroid;
			AvgNormal += TriArea * TriNormal;
			WeightSum += TriArea;
		}
		AvgNormal.Normalize();
		Centroid /= WeightSum;

		FFrame3d ProjectionFrame(Centroid, AvgNormal);

		// try to align projection frame w/ world directions so that textures are oriented more nicely...
		if (FMathd::Abs(AvgNormal.Dot(FVector3d::UnitX())) > 0.9)
		{
			ProjectionFrame.ConstrainedAlignAxis(1, FVector3d::UnitY(), AvgNormal);
		}
		else
		{
			ProjectionFrame.ConstrainedAlignAxis(0, FVector3d::UnitX(), AvgNormal);
		}

		// initialize UVs with initial projection
		UVEditor.SetTriangleUVsFromProjection(UVRegion.Indices, ProjectionFrame);

		// TODO: fit oriented box to projection here and check if it is dramatically better, and if so rotate ProjectionFrame
		// (or remap in 2D below?)

		// save correspondence list between UV element IDs and mesh vertices
		TSet<int32> RegionUVSet;
		for (int32 tid : UVRegion.Indices)
		{
			FIndex3i TriUVs = UseOverlay->GetTriangle(tid);
			RegionUVSet.Add(TriUVs.A); RegionUVSet.Add(TriUVs.B); RegionUVSet.Add(TriUVs.C);
		}
		TArray<int32> RegionUVs = RegionUVSet.Array();
		TArray<int32> RegionUVVertices;
		int32 NumElements = RegionUVs.Num();
		RegionUVVertices.SetNum(NumElements);
		for (int32 k = 0; k < NumElements; ++k)
		{
			RegionUVVertices[k] = UseOverlay->GetParentVertex(RegionUVs[k]);
		}

		// compute oriented bounds
		FAxisAlignedBox2d AxisAlignedBounds = FAxisAlignedBox2d::Empty();
		TArray<FVector2d> Points2D;
		for (int32 elemid : RegionUVs)
		{
			Points2D.Add((FVector2d)UseOverlay->GetElement(elemid));
			AxisAlignedBounds.Contain(Points2D.Last());
		}
		FOrientedBox2d OrientedBounds = UE::Geometry::FitOrientedBox2Points(TArrayView<const TVector2<double>>(Points2D), UE::Geometry::EBox2FitCriteria::Area);
		double RotAngle2D = SignedAngleR(OrientedBounds.UnitAxisX, FVector2d::UnitX());
		FMatrix2d Rotation = FMatrix2d::RotationRad(RotAngle2D);
		
		//UVEditor.ScaleUVAreaToBoundingBox(UVRegion.Indices, FAxisAlignedBox2f(FVector2f::Zero(), FVector2f::One()), true, true);

		// extract patch as submesh and map to 2D space
		FDynamicSubmesh3 SubmeshCalc(&EditMesh, UVRegion.Indices);
		FDynamicMesh3& RegionMesh = SubmeshCalc.GetSubmesh();
		TArray<int32> SubmeshVertices = RegionUVVertices;
		for ( int32 k = 0; k < NumElements; ++k )
		{
			FVector2d UV = (FVector2d)UseOverlay->GetElement(RegionUVs[k]);
			UV = Rotation * UV;
			SubmeshVertices[k] = SubmeshCalc.MapVertexToSubmesh(SubmeshVertices[k]);
			RegionMesh.SetVertex(SubmeshVertices[k], FVector3d(UV.X, UV.Y, 0));
		}
		FMeshBoundaryLoops BoundaryLoops(&RegionMesh, true);
		if (BoundaryLoops.Loops.Num() == 0) continue;		// fail!
		FEdgeLoop Loop = BoundaryLoops[BoundaryLoops.GetLongestLoopIndex()];		// assuming this is outer loop...could be more robust here

		TArray<FVector3d> LoopVertices;
		Loop.GetVertices(LoopVertices);
		FPolygon2d ProjectedLoop;
		for (FVector3d v : LoopVertices)
		{
			ProjectedLoop.AppendVertex(FVector2d(v.X, v.Y));
		}
		FAxisAlignedBox2d ProjectedBounds = ProjectedLoop.Bounds();


		double Aspect = ProjectedBounds.Width() / ProjectedBounds.Height();
		double TargetWidth = ProjectedBounds.Width();
		double TargetHeight = ProjectedBounds.Height();
		double UseScale = ScaleFactor;

		// use texel density?
		// use some more advanced shape matching for non-rect patches?

		if (NumPatches == 0) continue;

		TArray<FVector3i> PatchScores;
		PatchScores.SetNum(NumPatches);
		for (int32 k = 0; k < NumPatches; ++k)
		{
			auto& Patch = HotSpots.Patches[k];
			double PatchAspect = Patch.ShapeBounds.Width() / Patch.ShapeBounds.Height();

			double dx = FMathd::Abs(TargetWidth - UseScale * Patch.ShapeBounds.Width());
			double dy = FMathd::Abs(TargetHeight - UseScale * Patch.ShapeBounds.Height());

			double AspectDeviation = FMathd::Abs(Aspect - PatchAspect);
			PatchScores[k].X = (int32)(AspectDeviation * 100000);
			
			double SizeDeviation = dx*dx + dy*dy; //FMathd::Sqrt(dx*dx + dy*dy);
			PatchScores[k].Y = (int32)SizeDeviation;

			PatchScores[k].Z = k;
		}

		PatchScores.Sort([&](const FVector3i& A, const FVector3i& B)
		{
			// ignoring aspect ratio for now...not clear how to fold scores together...
			return A.Y < B.Y;
		});
		int32 UseIndex = 0;
		if (Debug_ShiftSelection > 0) UseIndex = FMathd::Min(Debug_ShiftSelection, PatchScores.Num()-1);
		int32 BestMatchIndex = PatchScores[UseIndex].Z;

		if (BestMatchIndex >= 0)
		{
			auto& UsePatch = HotSpots.Patches[BestMatchIndex];
			double ShapeScaleU = UsePatch.ShapeBounds.Width() / ProjectedBounds.Width();
			double ShapeScaleV = UsePatch.ShapeBounds.Height() / ProjectedBounds.Height();
			FVector2d ShapeTranslate = UsePatch.ShapeBounds.Center() - ProjectedBounds.Center();

			// remap 2D region mesh into the local space/scale of the hotspot patch
			for (int32 k = 0; k < RegionUVs.Num(); ++k)
			{
				FVector3d PatchPos2D = RegionMesh.GetVertex(SubmeshVertices[k]);
				double shape_u = ShapeScaleU * (PatchPos2D.X + ShapeTranslate.X);
				double shape_v = ShapeScaleV * (PatchPos2D.Y + ShapeTranslate.Y);

				RegionMesh.SetVertex(SubmeshVertices[k], FVector3d(shape_u, shape_v, 0));
			}

			FAxisAlignedBox3d RegionMeshBounds = RegionMesh.GetBounds();

#if 0
			// should consider options here
			// - vertex snapping
			// - reparameterize by arc-length
			// - laplacian deform the interior vertices...


			// figure out remapping of boundary vertices onto region
			if (LoopVertices.Num() <= UsePatch.ShapeBoundary.VertexCount())
			{
				for (int32 vid : RegionMesh.VertexIndicesItr())
				{
					if (RegionMesh.IsBoundaryVertex(vid))
					{
						FVector3d Pos = RegionMesh.GetVertex(vid);
						FVector2d PosUV(Pos.X, Pos.Y);

						double NearestSqr = TNumericLimits<double>::Max();
						FVector2d SnapPos = PosUV;
						for (FVector2d V : UsePatch.ShapeBoundary.GetVertices())
						{
							double DistSqr = DistanceSquared(V, PosUV);
							if (DistSqr < NearestSqr)
							{
								NearestSqr = DistSqr;
								SnapPos = V;
							}
						}
						RegionMesh.SetVertex(vid, FVector3d(SnapPos.X, SnapPos.Y, 0.0));

						// snap to boundary polygon
						//int NearestSegIndex = -1; double NearestSegParam = 0;
						//UsePatch.ShapeBoundary.DistanceSquared(FVector2d(Pos.X, Pos.Y), NearestSegIndex, NearestSegParam);
						//FVector2d SnapPos = UsePatch.ShapeBoundary.GetSegmentPoint(NearestSegIndex, NearestSegParam);
						//RegionMesh.SetVertex(vid, FVector3d(SnapPos.X, SnapPos.Y, 0.0));
					}
				}
			}
#endif

			// now mesh patch is in local coords of patch shape, which linearly maps into UV bound rect of hotspot patch
			for (int32 k = 0; k < RegionUVs.Num(); ++k)
			{
				FVector3d PatchPosLocalUV = RegionMesh.GetVertex(SubmeshVertices[k]);
				double u = (PatchPosLocalUV.X - UsePatch.ShapeBounds.Min.X) / UsePatch.ShapeBounds.Width();
				double v = (PatchPosLocalUV.Y - UsePatch.ShapeBounds.Min.Y) / UsePatch.ShapeBounds.Height();
				double new_u = UsePatch.UVBounds.Min.X + u * UsePatch.UVBounds.Width();
				double new_v = UsePatch.UVBounds.Min.Y + v * UsePatch.UVBounds.Height();
				UseOverlay->SetElement(RegionUVs[k], FVector2f((float)new_u, (float)new_v) );
			}
		}


	}



	// find components to UV

	if (Progress && Progress->Cancelled()) { return false; }


	// do per-component UVs here

	return true;
}
UE_ENABLE_OPTIMIZATION



#undef LOCTEXT_NAMESPACE

