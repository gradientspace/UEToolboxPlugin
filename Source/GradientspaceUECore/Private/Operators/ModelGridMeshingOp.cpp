// Copyright Gradientspace Corp. All Rights Reserved.
#include "Operators/ModelGridMeshingOp.h"
#include "Core/UEVersionCompat.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "GroupTopology.h"
#include "MeshSimplification.h"
#include "ConstrainedDelaunay2.h"
#include "DynamicMeshEditor.h"
#include "Polygroups/PolygroupsGenerator.h"
#include "Operations/PolygroupRemesh.h"
#include "Parameterization/DynamicMeshUVEditor.h"
#include "Operations/MeshSelfUnion.h"
#include "Selections/MeshConnectedComponents.h"

#include "Utility/GSUEModelGridUtil.h"



using namespace UE::Geometry;
using namespace GS;



template<typename OverlayType>
void EnumerateSeamEdgesFromOverlay(
	const FDynamicMesh3& Mesh,
	const OverlayType& Overlay,
	TFunctionRef<void(int)> SeamEdgeFunc)
{
	for (int32 eid : Mesh.EdgeIndicesItr()) {
		if (Overlay.IsSeamEdge(eid))
			SeamEdgeFunc(eid);
	}
}


static void EnumerateMeshOverlaySeamEdges(const FDynamicMesh3& Mesh,
	bool bUVSeams, 
	bool bHardNormalSeams,
	bool bHardColorSeams,
	TFunctionRef<void(int)> SeamEdgeFunc)
{
	if (!Mesh.HasAttributes()) return;
	const FDynamicMeshAttributeSet* Attributes = Mesh.Attributes();

	if (bUVSeams)
	{
		for (int k = 0; k < Attributes->NumUVLayers(); ++k)
		{
			if (const FDynamicMeshUVOverlay* UVLayer = Attributes->GetUVLayer(k))
				EnumerateSeamEdgesFromOverlay(Mesh, *UVLayer, SeamEdgeFunc);
		}
	}
	if (bHardNormalSeams)
	{
		if (const FDynamicMeshNormalOverlay* NormalLayer = Attributes->PrimaryNormals())
			EnumerateSeamEdgesFromOverlay(Mesh, *NormalLayer, SeamEdgeFunc);
	}
	if (bHardColorSeams)
	{
		if (const FDynamicMeshColorOverlay* Colors = Attributes->PrimaryColors())
			EnumerateSeamEdgesFromOverlay(Mesh, *Colors, SeamEdgeFunc);
	}
}


static void EnumerateColorDiscontinuityEdges(const FDynamicMesh3& Mesh,
	TFunctionRef<void(int)> SeamEdgeFunc,
	double ColorChannelAbsTolerance = 0.0001)
{
	if (!Mesh.HasAttributes()) return;
	if (const FDynamicMeshColorOverlay* Colors = Mesh.Attributes()->PrimaryColors())
	{
		for (int32 eid : Mesh.EdgeIndicesItr()) 
		{
			FDynamicMesh3::FEdge EdgeInfo = Mesh.GetEdge(eid);
			if (EdgeInfo.Tri.B == IndexConstants::InvalidID) continue;
			if (Colors->IsSetTriangle(EdgeInfo.Tri.A) == false || Colors->IsSetTriangle(EdgeInfo.Tri.B) == false) continue;
			EdgeInfo.Vert.Sort();
			FIndex3i MeshTriA = Mesh.GetTriangle(EdgeInfo.Tri.A);
			FIndex3i ColorTriA = Colors->GetTriangle(EdgeInfo.Tri.A);
			FIndex3i MeshTriB = Mesh.GetTriangle(EdgeInfo.Tri.B);
			FIndex3i ColorTriB = Colors->GetTriangle(EdgeInfo.Tri.B);
			bool bIsSeam = false;
			for (int j = 0; j < 2 && !bIsSeam; ++j)
			{
				int iA = MeshTriA.IndexOf(EdgeInfo.Vert[j]);
				FVector4f ColorA = Colors->GetElement(ColorTriA[iA]);
				int iB = MeshTriB.IndexOf(EdgeInfo.Vert[j]);
				FVector4f ColorB = Colors->GetElement(ColorTriB[iB]);
				if (FMathd::Abs(ColorA.X - ColorB.X) > ColorChannelAbsTolerance
					|| FMathd::Abs(ColorA.Y - ColorB.Y) > ColorChannelAbsTolerance
					|| FMathd::Abs(ColorA.Z - ColorB.Z) > ColorChannelAbsTolerance)
					bIsSeam = true;
			}
			if (bIsSeam)
				SeamEdgeFunc(eid);
		}
	}
}


static void EnumerateMaterialSeamEdges(const FDynamicMesh3& Mesh,
	TFunctionRef<void(int)> SeamEdgeFunc)
{
	if (!Mesh.HasAttributes()) return;
	if (const FDynamicMeshMaterialAttribute* MaterialID = Mesh.Attributes()->GetMaterialID())
	{
		for (int32 eid : Mesh.EdgeIndicesItr()) {
			FIndex2i Tris = Mesh.GetEdgeT(eid);
			if (Tris.B != IndexConstants::InvalidID && MaterialID->GetValue(Tris.A) != MaterialID->GetValue(Tris.B))
				SeamEdgeFunc(eid);
		}
	}
}


static void EnumerateCoplanarFaceGroups(
	FDynamicMesh3& Mesh, 
	double FaceAngleToleranceDeg,
	TFunctionRef<bool(int TriA, int TriB, int EdgeID)> TrisConnectedPredicate,
	TFunctionRef<void(TArray<int>&)> ProcessGroupFunc )
{
	double DotTolerance = 1.0 - FMathd::Cos(FaceAngleToleranceDeg * FMathd::DegToRad);
	DotTolerance = 1.0 - DotTolerance;

	// compute face normals
	FMeshNormals Normals(&Mesh);
	Normals.ComputeTriangleNormals();

	TArray<bool> DoneTriangle;
	DoneTriangle.SetNum(Mesh.MaxTriangleID());

	TArray<int> Stack, Group;

	for (int tid : Mesh.TriangleIndicesItr())
	{
		if (DoneTriangle[tid] == true)
			continue;

		Group.Reset();
		Group.Add(tid);
		DoneTriangle[tid] = true;

		Stack.SetNum(0);
		Stack.Add(tid);
		while (Stack.Num() > 0)
		{
			int CurTri = GSUE::TArrayPop(Stack);
			FIndex3i NbrTris = Mesh.GetTriNeighbourTris(CurTri);
			FIndex3i NbrEdges = Mesh.GetTriEdges(CurTri);
			for (int j = 0; j < 3; ++j)
			{
				int eid = NbrEdges[j];
				if (NbrTris[j] >= 0 && DoneTriangle[NbrTris[j]] == false)
				{
					double Dot = Normals[CurTri].Dot(Normals[NbrTris[j]]);
					if (Dot > DotTolerance)
					{
						if (TrisConnectedPredicate(CurTri, NbrTris[j], eid))
						{
							Group.Add(NbrTris[j]);
							Stack.Add(NbrTris[j]);
							DoneTriangle[NbrTris[j]] = true;
						}
					}
				}
			}
		}

		ProcessGroupFunc(Group);
	}

}




static void RemoveCoplanarFaces(
	FDynamicMesh3& EditMesh)
{
	double AngleToleranceDeg = 2.0;

	TArray<TArray<int>> Faces;
	EnumerateCoplanarFaceGroups(EditMesh, AngleToleranceDeg,
		[&](int TriA, int TriB, int EdgeID) { return true; },
		[&](TArray<int>& NewFace) { Faces.Add(MoveTemp(NewFace)); });
	int NumFaces = Faces.Num();

	TArray<FVector3d> Centroids;
	Centroids.Init(FVector3d::Zero(), NumFaces);
	TArray<double> Areas;
	Areas.Init(0, NumFaces);

	for (int fi = 0; fi < NumFaces; ++fi)
	{
		const TArray<int>& Face = Faces[fi];
		for (int tid : Face)
		{
			FVector3d Normal, Centroid; double Area;
			EditMesh.GetTriInfo(tid, Normal, Area, Centroid);
			Centroids[fi] += Area*Centroid;
			Areas[fi] += Area;
		}
		Centroids[fi] /= Areas[fi];
	}

	// TODO: this is dumb, we should be able to use integer coordinates for this as
	// only tris on grid cell faces can be coincident. Then could put in an array and
	// sort to find possible matches
	TArray<bool> ToRemove;
	ToRemove.Init(false, NumFaces);

	for (int k = 0; k < NumFaces; ++k)
	{
		if (ToRemove[k] == true) continue;
		for (int j = k + 1; j < NumFaces; ++j)
		{
			if (ToRemove[j] == true) continue;

			if (Distance(Centroids[k], Centroids[j]) < 0.001 && FMathd::Abs(Areas[k] - Areas[j]) < 0.001)
			{
				ToRemove[k] = true;
				ToRemove[j] = true;
				break;
			}
		}
	}

	TArray<int32> RemoveTris;
	for (int k = 0; k < NumFaces; ++k)
	{
		if (ToRemove[k] == true)
			RemoveTris.Append(Faces[k]);
	}

	FDynamicMeshEditor MeshEditor(&EditMesh);
	MeshEditor.RemoveTriangles(RemoveTris, true);

	// compact result
	EditMesh.CompactInPlace();
}


static void ApplySelfUnionCleanup(
	FDynamicMesh3& EditMesh)
{
	// really only should be done on cell faces and could probably be more efficient there...
	// (but self-union removes interior which is nice)

	FMeshSelfUnion Union(&EditMesh);
	Union.WindingThreshold = 0.5;
	Union.bTrimFlaps = true;
	Union.bSimplifyAlongNewEdges = true;
	bool bSuccess = Union.Compute();

	if (bSuccess)
	{
		EditMesh.CompactInPlace();
	}
}


static void PlanarAreaRetriangulation(
	FDynamicMesh3& EditMesh,
	double Tolerance,
	bool bPreserveUVSeams,
	bool bPreserveColorBoders,
	bool bPreserveMaterialBorders,
	double NormalAngleThresholdDeg)
{
	FDynamicMeshEditor MeshEditor(&EditMesh);
	FDynamicMeshEditResult EditResult;
	MeshEditor.SplitBowties(EditResult);

	FMergeCoincidentMeshEdges Welder(&EditMesh);
	Welder.MergeVertexTolerance = Tolerance * 0.001;
	Welder.OnlyUniquePairs = false;
	Welder.Apply();

	double AngleToleranceDeg = 2.0;


	TSet<int> HardEdges;
	if (bPreserveUVSeams)
	{
		EnumerateMeshOverlaySeamEdges(EditMesh, true, false, false,
			[&](int eid) { HardEdges.Add(eid); });
	}
	if (bPreserveColorBoders)
		EnumerateColorDiscontinuityEdges(EditMesh, [&](int eid) { HardEdges.Add(eid); });
	if (bPreserveMaterialBorders)
		EnumerateMaterialSeamEdges(EditMesh, [&](int eid) { HardEdges.Add(eid); });

	EditMesh.DiscardTriangleGroups();
	EditMesh.EnableTriangleGroups(0);
	EnumerateCoplanarFaceGroups(EditMesh, AngleToleranceDeg,
		[&](int TriA, int TriB, int EdgeID) { return HardEdges.Contains(EdgeID) == false; },
		[&](TArray<int>& NewFace) {
			int NewGroupID = EditMesh.AllocateTriangleGroup();
			for (int tid : NewFace) EditMesh.SetTriangleGroup(tid, NewGroupID);
		} );

	int MaxGroupID = EditMesh.MaxGroupID();

	// TODO: FPolygroupRemesh does not preserve MaterialID or VertexColors...have to do ourselves
	TMap<int, int> GroupToMaterialMap;
	FDynamicMeshMaterialAttribute* MaterialID = EditMesh.Attributes()->GetMaterialID();
	if (MaterialID) {
		for (int tid : EditMesh.TriangleIndicesItr()) {
			int gid = EditMesh.GetTriangleGroup(tid);
			if (GroupToMaterialMap.Contains(gid) == false) 
				GroupToMaterialMap.Add(gid, MaterialID->GetValue(tid));
		}
	}
	TArray<FVector4f> GroupToColorMap;
	GroupToColorMap.Init(FVector4f::Zero(), MaxGroupID);
	FDynamicMeshColorOverlay* VertexColor = EditMesh.Attributes()->PrimaryColors();
	if (VertexColor)
	{
		for (int tid : EditMesh.TriangleIndicesItr()) {
			if (VertexColor->IsSetTriangle(tid) == false) continue;
			FVector4f A, B, C;
			VertexColor->GetTriElements(tid, A, B, C);
			int gid = EditMesh.GetTriangleGroup(tid);
			FVector4f AvgColor = (A + B + C) / 3.0;
			GroupToColorMap[gid] += FVector4f(AvgColor.X, AvgColor.Y, AvgColor.Z, 1.0);
		}
		for (int k = 0; k < MaxGroupID; ++k)
			GroupToColorMap[k] /= (GroupToColorMap[k].W > 0) ? GroupToColorMap[k].W : 1.0;
	}

	FGroupTopology UseTopology(&EditMesh, true);

	FPolygroupRemesh Simplifier(&EditMesh, &UseTopology, ConstrainedDelaunayTriangulate<double>);
	Simplifier.SimplificationAngleTolerance = AngleToleranceDeg;
	Simplifier.Compute();

	if (MaterialID) {
		for (int tid : EditMesh.TriangleIndicesItr()) {
			int gid = EditMesh.GetTriangleGroup(tid);
			MaterialID->SetValue(tid, GroupToMaterialMap[gid]);
		}
	}
	if (VertexColor)
	{
		VertexColor->ClearElements();
		VertexColor->CreateFromPredicate([&](int vid, int TriA, int TriB) {
			return EditMesh.GetTriangleGroup(TriA) == EditMesh.GetTriangleGroup(TriB);
		}, 1.0f);
		for (int tid : EditMesh.TriangleIndicesItr()) {
			FVector4f TriColor = GroupToColorMap[EditMesh.GetTriangleGroup(tid)];
			FIndex3i ColorTri = VertexColor->GetTriangle(tid);
			VertexColor->SetElement(ColorTri.A, TriColor);
			VertexColor->SetElement(ColorTri.B, TriColor);
			VertexColor->SetElement(ColorTri.C, TriColor);
		}
	}

	// compact result
	EditMesh.CompactInPlace();

	// recompute normals
	FMeshNormals::InitializeOverlayTopologyFromOpeningAngle(&EditMesh, EditMesh.Attributes()->PrimaryNormals(), NormalAngleThresholdDeg);
	FMeshNormals::QuickRecomputeOverlayNormals(EditMesh);
}




static void ComputeUVsFromPlanarGroupProjections(
	FDynamicMesh3& EditMesh )
{
	FDynamicMeshUVEditor UVEditor(&EditMesh, 0, true);
	FDynamicMeshUVOverlay* UVOverlay = UVEditor.GetOverlay();
	UVEditor.ResetUVs();

	double AngleToleranceDeg = 2.0;
	FPolygroupsGenerator Generator(&EditMesh);
	Generator.bApplyPostProcessing = false;
	Generator.bCopyToMesh = false;
	double DotTolerance = 1.0 - FMathd::Cos(AngleToleranceDeg * FMathd::DegToRad);
	Generator.FindPolygroupsFromFaceNormals(DotTolerance, false, true);
	int NumGroups = Generator.FoundPolygroups.Num();
	for (int gi = 0; gi < NumGroups; ++gi)
	{
		const TArray<int32>& Tris = Generator.FoundPolygroups[gi];

		FVector3d AvgNormal(0, 0, 0);
		FAxisAlignedBox3d Bounds = FAxisAlignedBox3d::Empty();
		for (int tid : Tris)
		{
			AvgNormal += EditMesh.GetTriNormal(tid);
			FVector3d A, B, C;
			EditMesh.GetTriVertices(tid, A, B, C);
			Bounds.Contain(A); Bounds.Contain(B); Bounds.Contain(C);
		}
		AvgNormal.Normalize();
		FFrame3d ProjectionFrame(Bounds.Center(), AvgNormal);
		// todo orient X/Y nicely

		UVEditor.SetTriangleUVsFromProjection(Tris, ProjectionFrame);
	}
	
	// todo
	//UVEditor.QuickPack();
}


static bool PixelLayoutAndPack(
	FDynamicMesh3& EditMesh, FVector3d CellDimensions, int DimensionPixels, int FacePixelBorder,
	int& AllocatedImageDimOut )
{
	AllocatedImageDimOut = 0;

	double UseDimension = CellDimensions.GetAbsMin();
	double WorldDimPerPixel = UseDimension / (double)DimensionPixels;

	FDynamicMeshUVEditor UVEditor(&EditMesh, 0, true);
	FDynamicMeshUVOverlay* UVOverlay = UVEditor.GetOverlay();

	// TODO WHY IS THIS NECESSARY? WHY ARE THERE BOWTIES IN GENERATED MESH UVS!?!?
	UVOverlay->SplitBowties();

	// only using existing UV topology...could calc from groups?
	FMeshConnectedComponents UVIslandComponents(&EditMesh);
	UVIslandComponents.FindConnectedTriangles([&](int32 t0, int32 t1) {
		return UVOverlay->AreTrianglesConnected(t0, t1);
	});
	int NumIslands = UVIslandComponents.Num();
	
	struct FaceInfo {
		int Index;
		TArray<int32> Triangles;
		FFrame3d ProjectionPlane;
		FAxisAlignedBox2d ProjectionBounds;
		FVector2d WorldSize;
		TArray<int32> UVElements;
		FVector2i PixelDims;
		FVector2i PixelLocation;
	};
	TArray<FaceInfo> Faces;
	TArray<int> FaceOrdering;
	Faces.SetNum(NumIslands);
	FaceOrdering.SetNum(NumIslands);

	int TotalPixelArea = 0;
	FVector2i MaxBoxDims = FVector2i::Zero();
	FVector2i MaxBoxSize = FVector2i::Zero();
	FVector2i MinBoxSize = FVector2i(9999999,9999999);
	for (int k = 0; k < NumIslands; ++k) {
		FaceInfo& Face = Faces[k];
		FaceOrdering[k] = k;

		Face.Index = k;
		Face.Triangles = MoveTemp(UVIslandComponents[k].Indices);
		int NT = Face.Triangles.Num();

		FVector3d AvgFaceNormal = EditMesh.GetTriNormal(Face.Triangles[0]);
		for (int j = 1; j < NT; ++j)
			AvgFaceNormal += EditMesh.GetTriNormal(Face.Triangles[j]);
		AvgFaceNormal.Normalize();
		Face.ProjectionPlane = FFrame3d(FVector3d::Zero(), AvgFaceNormal);

		// somehow axis-align here? or fit oriented bounds?
		

		// currently flipping to smaller Y-height but really the policy should be more like 
		// (1) prefer standard unit-cell Y-height
		// (2) prefer existing allocated Y-height...or <= existing height? since we can pack in that row then?
		// (3) prefer smaller Y-height
		// Could track set of heights that have occurred for (2)...
		FAxisAlignedBox2d InPlaneBounds;
		FVector3d A, B, C;
		for (int iter = 0; iter < 2; ++iter) {
			InPlaneBounds = FAxisAlignedBox2d::Empty();
			for (int j = 0; j < NT; ++j) {
				EditMesh.GetTriVertices(Face.Triangles[j], A, B, C);
				InPlaneBounds.Contain(Face.ProjectionPlane.ToPlaneUV(A, 2));
				InPlaneBounds.Contain(Face.ProjectionPlane.ToPlaneUV(B, 2));
				InPlaneBounds.Contain(Face.ProjectionPlane.ToPlaneUV(C, 2));
			}
			if (InPlaneBounds.Height() > InPlaneBounds.Width() && FMathd::Abs(InPlaneBounds.Height() - InPlaneBounds.Width()) > FMathf::Epsilon) {
				Face.ProjectionPlane.Rotate(FQuaterniond(Face.ProjectionPlane.Z(), 90, true));
			}
			else
				break;
		}

		Face.WorldSize = FVector2d(InPlaneBounds.Width(), InPlaneBounds.Height());
		Face.ProjectionBounds = InPlaneBounds;

		// ideally want to round to some decimal place here...eg if we end up w/ 1.00000001
		Face.PixelDims.X = (int)std::ceil(Face.WorldSize.X / WorldDimPerPixel - FMathf::ZeroTolerance);
		Face.PixelDims.Y = (int)std::ceil(Face.WorldSize.Y / WorldDimPerPixel - FMathf::ZeroTolerance);
		TotalPixelArea += (Face.PixelDims.X + 2*FacePixelBorder) * (Face.PixelDims.Y + 2*FacePixelBorder);

		MaxBoxDims.X = FMath::Max(MaxBoxDims.X, Face.PixelDims.X);
		MaxBoxDims.Y = FMath::Max(MaxBoxDims.Y, Face.PixelDims.Y);
		if (Face.PixelDims.X >= MinBoxSize.X && Face.PixelDims.Y >= MinBoxSize.Y)
			MaxBoxSize = Face.PixelDims;
		if (Face.PixelDims.X <= MinBoxSize.X && Face.PixelDims.Y <= MinBoxSize.Y)
			MinBoxSize = Face.PixelDims;

		// collect list of UV elements
		for (int j = 0; j < NT; ++j) {
			FIndex3i UVTri = UVOverlay->GetTriangle(Face.Triangles[j]);
			Face.UVElements.AddUnique(UVTri.A);
			Face.UVElements.AddUnique(UVTri.B);
			Face.UVElements.AddUnique(UVTri.C);
		}

		Face.PixelLocation = FVector2i::Zero();
	}

	// find initial power-of-two image size
	int InitialPixelTargetDim = (int)sqrt((double)TotalPixelArea);
	for (int CurDim = 8; CurDim <= 4096; CurDim *= 2) {
		if (CurDim > InitialPixelTargetDim) {
			InitialPixelTargetDim = CurDim;
			break;
		}
	}

	// todo some kind of sorting to pack largest faces first?

	// sort by heights, largest to smallest...
	FaceOrdering.Sort([&](const int& A, const int& B) {
		return Faces[A].PixelDims.Y > Faces[B].PixelDims.Y;
	});

	int AvailableImageDimensions = InitialPixelTargetDim;
	bool bFoundPacking = false;
	while (!bFoundPacking && AvailableImageDimensions <= 4096)
	{
		// todo be smarter here...
		// for example, if we don't fill an entire row of height=5, then we could fit some height=4
		// in that row. So we should basically do full-rows at a time...or maybe start from 'tallest' row
		// and then work backwards?

		int CurIndex = 0;
		int CurX = FacePixelBorder, CurY = FacePixelBorder;
		int CurRowYHeight = Faces[FaceOrdering[0]].PixelDims.Y;
		int AllocatedY = 0;
		while (CurIndex < NumIslands) {

			int FaceIndex = FaceOrdering[CurIndex++];
			FaceInfo& Face = Faces[FaceIndex];
			// start new row if adding this face will either change y-height of current row, or go too far
			//if (Face.PixelDims.Y != CurRowYHeight || CurX + Face.PixelDims.X >= AvailableImageDimensions)
			if ( (CurX + Face.PixelDims.X + 2*FacePixelBorder) >= AvailableImageDimensions)
			{
				CurY += CurRowYHeight + 2*FacePixelBorder;
				CurRowYHeight = Face.PixelDims.Y;
				CurX = 0;
			}

			//ensure(Face.PixelDims.Y == CurRowYHeight);
			Face.PixelLocation = FVector2i(CurX, CurY);
			CurX += Face.PixelDims.X + 2*FacePixelBorder;
			// CurY does not change along row
			
			AllocatedY = FMath::Max(AllocatedY, CurY + Face.PixelDims.Y);
			if (AllocatedY >= AvailableImageDimensions)
				break;		// spilled off the bottom
		}

		if (AllocatedY < AvailableImageDimensions) {
			bFoundPacking = true;
		} else {
			AvailableImageDimensions *= 2;
		}
	}

	if (bFoundPacking == false)
		return false;

	// ok convert to UVs...
	double PixelToUVScale = 1.0 / (double)AvailableImageDimensions;
	for (int k = 0; k < NumIslands; ++k) {
		FaceInfo& Face = Faces[k];
		FVector2i PixelLocation = Face.PixelLocation;
		FVector2i PixelDim = Face.PixelDims;

		FVector2d AllocatedUVBoundsMin = PixelToUVScale * (FVector2d)PixelLocation;
		//FVector2d UVMax = UVMin + (d * (FVector2d)PixelDim);

		for (int ei : Face.UVElements)
		{
			int vid = UVOverlay->GetParentVertex(ei);
			FVector3d Pos3D = EditMesh.GetVertex(vid);
			// world-space planar projection
			FVector2d PlaneUV = Face.ProjectionPlane.ToPlaneUV(Pos3D, 2);
			// shifted to local bounds origin
			FVector2d LocalPlaneUV = PlaneUV - Face.ProjectionBounds.Min;
			// scale to pixels
			FVector2d PixelCoords = LocalPlaneUV / WorldDimPerPixel;
			// scale to target-image UV space
			FVector2d ImageRelativeUV = PixelCoords * PixelToUVScale;
			
			// todo somehow handle min & max aspect? is it necessary?

			ImageRelativeUV += AllocatedUVBoundsMin;

			UVOverlay->SetElement(ei, (FVector2f)ImageRelativeUV);
		}
	}
	
	AllocatedImageDimOut = AvailableImageDimensions;
	return true;
}








void FModelGridMeshingOp::CalculateResult(FProgressCancel* Progress)
{
	//bool bMeshValid = OriginalMeshShared->AccessSharedObject([&](const FDynamicMesh3& Mesh) {
	//	*ResultMesh = Mesh;
	//});
	*ResultMesh = FDynamicMesh3();
	CalculateResultInPlace(*ResultMesh, Progress);
}


bool FModelGridMeshingOp::CalculateResultInPlace(FDynamicMesh3& EditMesh, FProgressCancel* Progress)
{
	// update mesh...
	FDynamicMesh3 FinalMesh;
	GS::ExtractGridFullMesh(SourceData->SourceGrid, FinalMesh, /*materials=*/true, /*uvs=*/true);

	// weld
	if (FinalMesh.TriangleCount() > 0)
	{
		FMergeCoincidentMeshEdges Welder(&FinalMesh);
		Welder.MergeVertexTolerance = 0.01;
		Welder.OnlyUniquePairs = false;
		Welder.bWeldAttrsOnMergedEdges = true;
		Welder.Apply();
		FinalMesh.CompactInPlace();
	}

	if (bRemoveCoincidentFaces)
	{
		RemoveCoplanarFaces(FinalMesh);
	}

	if (bSelfUnion)
	{
		ApplySelfUnionCleanup(FinalMesh);
	}

	if (bOptimizePlanarAreas)
	{
		bool bPresereUVSeams = false;
		PlanarAreaRetriangulation(FinalMesh, 0.1, bPresereUVSeams, bPreserveColorBorders, bPreserveMaterialBorders, 12.0f);
		ComputeUVsFromPlanarGroupProjections(FinalMesh);
	}

	if (bInvertFaces)
	{
		FinalMesh.ReverseOrientation();
		FDynamicMeshNormalOverlay* Normals = FinalMesh.Attributes()->PrimaryNormals();
		for (int ElID : Normals->ElementIndicesItr())
		{
			FVector3f Normal = Normals->GetElement(ElID);
			Normals->SetElement(ElID, -Normal);
		}
	}

	// weld again...
	if (FinalMesh.TriangleCount() > 0)
	{
		FMergeCoincidentMeshEdges Welder(&FinalMesh);
		Welder.MergeVertexTolerance = 0.01;
		Welder.OnlyUniquePairs = false;
		Welder.bWeldAttrsOnMergedEdges = true;
		Welder.Apply();
		FinalMesh.CompactInPlace();
	}

	if (bRecomputeGroups)
	{
		FPolygroupsGenerator Generator(&FinalMesh);
		double DotTolerance = 1.0 - FMathd::Cos(GroupAngleThreshDeg * FMathd::DegToRad);
		Generator.FindPolygroupsFromFaceNormals(DotTolerance, false, false);
		Generator.CopyPolygroupsToMesh();
	}

	if (UVMode == EUVMode::Discard)
	{
		FDynamicMeshUVEditor Editor(&FinalMesh, 0, true);
		Editor.ResetUVs();
	}
	else if (UVMode == EUVMode::Repack)
	{
		FDynamicMeshUVEditor Editor(&FinalMesh, 0, true);
		Editor.QuickPack(TargetUVResolution, 1.0f);

	}
	else if (UVMode == EUVMode::PixelLayoutRepack)
	{
		int UsePixelCount = FMathd::Clamp(DimensionPixelCount, 1, 2048);
		int UseFacePixelBorder = FMathd::Clamp(UVIslandPixelBorder, 0, 64);
		int AllocatedImageDims = 0;
		if (PixelLayoutAndPack(FinalMesh, SourceData->SourceGrid.GetCellDimensions(), UsePixelCount, UseFacePixelBorder, AllocatedImageDims))
		{
			PixelLayoutImageDimensionX_Result = PixelLayoutImageDimensionY_Result = AllocatedImageDims;
		}
	}

	// simplify

	EditMesh = MoveTemp(FinalMesh);

	return true;
}


void ApplySubDModifier(FDynamicMesh3& Mesh)
{
#if 0
	Mesh.CompactInPlace(nullptr);

	FDynamicMeshColorOverlay* Colors = Mesh.Attributes()->PrimaryColors();

	// this is kind of a hack, to map group colors to subdivided faces because
	// FSubdividePoly does not support vertex color attributes
	TArray<FVector4f> GroupToColorMap;
	GroupToColorMap.SetNum(Mesh.MaxGroupID());
	for (int tid : Mesh.TriangleIndicesItr())
	{
		int GroupID = Mesh.GetTriangleGroup(tid);
		Colors->GetTriElement(tid, 0, GroupToColorMap[GroupID]);
	}

	FGroupTopology GroupTopo(&Mesh, true);
	FSubdividePoly SubD(GroupTopo, Mesh, SubDLevel);
	if (SubD.ValidateTopology() == FSubdividePoly::ETopologyCheckResult::Ok)
	{
		SubD.ComputeTopologySubdivision();
		FDynamicMesh3 SubdividedMesh;
		SubD.ComputeSubdividedMesh(SubdividedMesh);
		Mesh = MoveTemp(SubdividedMesh);

		Mesh.Attributes()->EnablePrimaryColors();
		Colors = Mesh.Attributes()->PrimaryColors();
		for (int tid : Mesh.TriangleIndicesItr())
		{
			int gid = Mesh.GetTriangleGroup(tid);
			FVector4f Color = (gid >= 0 && gid < GroupToColorMap.Num()) ? GroupToColorMap[Mesh.GetTriangleGroup(tid)] : FVector4f::One();
			int A = Colors->AppendElement(Color), B = Colors->AppendElement(Color), C = Colors->AppendElement(Color);
			Colors->SetTriangle(tid, FIndex3i(A, B, C));
		}
	}
#endif
}