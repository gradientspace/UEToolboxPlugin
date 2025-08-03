// Copyright Gradientspace Corp. All Rights Reserved.

#include "Utility/GSUEMeshUtil.h"

#include "Mesh/DenseMesh.h"
#include "Mesh/PolyMesh.h"
#include "Core/gs_serializer.h"
#include "Mesh/MeshAttributeUtils.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "GroupTopology.h"


using namespace UE::Geometry;
using namespace GS;



template<typename AttribSetType, typename TriVtxType, typename ConversionFuncType, typename SetFuncType>
void CopyIndexedAttribToDenseMesh(
	DenseMesh& DenseMesh,
	TriVtxType DefaultValues,
	const AttribSetType* AttribSet,
	ConversionFuncType ConversionFunc,
	SetFuncType SetFunc)
{
	int TriangleCount = DenseMesh.GetTriangleCount();
	for (int tid = 0; tid < TriangleCount; ++tid)
	{
		Index3i Tri = DenseMesh.GetTriangle(tid);
		if (AttribSet->IsSetTriangle(tid))
		{
			TriVtxType TriValues;
			FIndex3i AttribSetTri = AttribSet->GetTriangle(tid);
			for (int j = 0; j < 3; ++j)
			{
				auto ElementValue = AttribSet->GetElement(AttribSetTri[j]);
				TriValues[j] = ConversionFunc( ElementValue );
			}
			SetFunc(tid, TriValues);
		}
		else
		{
			SetFunc(tid, DefaultValues);
		}
	}
}


template<typename TriVtxType, typename SetFuncType>
void InitializeDenseMeshIndexedAttribute(
	DenseMesh& DenseMesh,
	TriVtxType DefaultValues,
	SetFuncType SetFunc)
{
	int TriangleCount = DenseMesh.GetTriangleCount();
	for (int tid = 0; tid < TriangleCount; ++tid)
	{
		SetFunc(tid, DefaultValues);
	}
}


void GS::ConvertDynamicMeshToDenseMesh(const FDynamicMesh3& DynamicMeshIn, DenseMesh& DenseMesh)
{
	static bool TODO_SRGB_CONVERSION = true;

	const FDynamicMesh3* UseMesh = &DynamicMeshIn;
	FDynamicMesh3 LocalMesh;
	if (DynamicMeshIn.IsCompactV() == false)
	{
		LocalMesh.CompactCopy(DynamicMeshIn);
		UseMesh = &LocalMesh;
	}

	int VertexCount = UseMesh->VertexCount();
	int TriangleCount = UseMesh->TriangleCount();
	const FDynamicMeshNormalOverlay* Normals = (UseMesh->HasAttributes()) ? UseMesh->Attributes()->PrimaryNormals() : nullptr;
	const FDynamicMeshUVOverlay* UVs = (UseMesh->HasAttributes()) ? UseMesh->Attributes()->GetUVLayer(0) : nullptr;
	const FDynamicMeshColorOverlay* Colors = (UseMesh->HasAttributes()) ? UseMesh->Attributes()->PrimaryColors() : nullptr;
	const FDynamicMeshMaterialAttribute* MaterialIDs = (UseMesh->HasAttributes()) ? UseMesh->Attributes()->GetMaterialID() : nullptr;

	DenseMesh.Resize(VertexCount, TriangleCount);
	for (int vid = 0; vid < VertexCount; ++vid)
	{
		Vector3d V( UseMesh->GetVertex(vid) );
		DenseMesh.SetPosition(vid, V);
	}

	bool bHasGroupIDs = UseMesh->HasTriangleGroups();
	for (int tid = 0; tid < TriangleCount; ++tid)
	{
		Index3i Tri = UseMesh->GetTriangle(tid);
		DenseMesh.SetTriangle(tid, Tri);
		if (bHasGroupIDs) DenseMesh.SetTriGroup( tid, UseMesh->GetTriangleGroup(tid) );
		if (MaterialIDs) DenseMesh.SetTriMaterialIndex(tid, MaterialIDs->GetValue(tid) );
	}

	TriVtxNormals DefaultNormals(Vector3f(0, 0, 1));
	if (Normals)
	{
		CopyIndexedAttribToDenseMesh(DenseMesh, DefaultNormals, Normals,
			[](FVector3f Normal) { return Normal; },
			[&](int tid, const TriVtxNormals& TriNormals) { DenseMesh.SetTriVtxNormals(tid, TriNormals); } );
	}
	else
	{
		InitializeDenseMeshIndexedAttribute(DenseMesh, DefaultNormals, 
			[&](int tid, const TriVtxNormals& TriNormals) { DenseMesh.SetTriVtxNormals(tid, TriNormals); });
	}

	TriVtxUVs DefaultUVs(Vector2f(0, 0));
	if ( UVs )
	{ 
		CopyIndexedAttribToDenseMesh(DenseMesh, DefaultUVs, UVs,
			[](FVector2f UV ) { return UV; },
			[&](int tid, const TriVtxUVs& TriUVs) { DenseMesh.SetTriVtxUVs(tid, TriUVs); });
	}
	else
	{
		InitializeDenseMeshIndexedAttribute(DenseMesh, DefaultUVs,
			[&](int tid, const TriVtxUVs& TriUVs) { DenseMesh.SetTriVtxUVs(tid, TriUVs); });
	}

	TriVtxColors DefaultColors(Color4b(0, 0, 0, 255));
	if (Colors)
	{
		CopyIndexedAttribToDenseMesh(DenseMesh, DefaultColors, Colors,
			[](FVector4f Colorf) { return (Color4b)(ToLinearColor(Colorf).ToFColor(TODO_SRGB_CONVERSION)); },
			[&](int tid, const TriVtxColors& TriColors) { DenseMesh.SetTriVtxColors(tid, TriColors); });
	}
	else
	{
		InitializeDenseMeshIndexedAttribute(DenseMesh, DefaultColors,
			[&](int tid, const TriVtxColors& TriColors) { DenseMesh.SetTriVtxColors(tid, TriColors); });
	}

}




void GS::ConvertDynamicMeshToPolyMesh(const FDynamicMesh3& DynamicMeshIn, PolyMesh& PolyMesh)
{
	const FDynamicMesh3* UseMesh = &DynamicMeshIn;
	FDynamicMesh3 LocalMesh;
	if (DynamicMeshIn.IsCompactV() == false)
	{
		LocalMesh.CompactCopy(DynamicMeshIn);
		UseMesh = &LocalMesh;
	}

	int VertexCount = UseMesh->VertexCount();
	int TriangleCount = UseMesh->TriangleCount();
	const FDynamicMeshNormalOverlay* Normals = (UseMesh->HasAttributes()) ? UseMesh->Attributes()->PrimaryNormals() : nullptr;
	const FDynamicMeshUVOverlay* UVs = (UseMesh->HasAttributes()) ? UseMesh->Attributes()->GetUVLayer(0) : nullptr;
	const FDynamicMeshColorOverlay* Colors = (UseMesh->HasAttributes()) ? UseMesh->Attributes()->PrimaryColors() : nullptr;
	const FDynamicMeshMaterialAttribute* MaterialIDs = (UseMesh->HasAttributes()) ? UseMesh->Attributes()->GetMaterialID() : nullptr;

	PolyMesh.Clear();

	bool bHaveGroups = UseMesh->HasTriangleGroups();
	if (bHaveGroups)
		PolyMesh.SetNumGroupSets(1);

	PolyMesh.ReserveVertices(VertexCount);
	for (int vid = 0; vid < VertexCount; ++vid)
	{
		int new_vid = PolyMesh.AddVertex(UseMesh->GetVertex(vid));
		gs_debug_assert(new_vid == vid);
	}

	// todo copy all normal sets...?
	TArray<int> NormalIndexMap;
	if (Normals != nullptr)
	{
		int NormalSetIndex = 0;
		NormalIndexMap.SetNum(Normals->MaxElementID());
		PolyMesh.AddNormalSet(Normals->ElementCount());
		int NormalIndex = 0;
		for (int elemid : Normals->ElementIndicesItr())
		{
			PolyMesh.SetNormal(NormalIndex, Normals->GetElement(elemid), NormalSetIndex);
			NormalIndexMap[elemid] = NormalIndex;
			NormalIndex++;
		}
	}

	// todo copy all UV sets...?
	TArray<int> UVIndexMap;
	if (UVs != nullptr)
	{
		int UVSetIndex = 0;
		UVIndexMap.SetNum(UVs->MaxElementID());
		PolyMesh.AddUVSet(UVs->ElementCount());
		int UVIndex = 0;
		for (int elemid : UVs->ElementIndicesItr())
		{
			PolyMesh.SetUV(UVIndex, (Vector2d)UVs->GetElement(elemid), UVSetIndex);
			UVIndexMap[elemid] = UVIndex;
			UVIndex++;
		}
	}

	PolyMesh.ReserveFaces(TriangleCount);
	PolyMesh.ReserveTriangles(TriangleCount);

	std::vector<Index3i> TriNormals, TriUVs;
	TriNormals.resize(1); TriUVs.resize(1);
	for (int tid = 0; tid < TriangleCount; ++tid)
	{
		Index3i Tri = UseMesh->GetTriangle(tid);
		int GroupID = (bHaveGroups) ? UseMesh->GetTriangleGroup(tid) : 0;

		const Index3i* NormalTriangles = nullptr;
		if (Normals != nullptr && Normals->IsSetTriangle(tid))
		{
			Index3i NormalTri = Normals->GetTriangle(tid);
			NormalTri.A = NormalIndexMap[NormalTri.A];  NormalTri.B = NormalIndexMap[NormalTri.B];  NormalTri.C = NormalIndexMap[NormalTri.C];
			TriNormals[0] = NormalTri;
			NormalTriangles = &TriNormals[0];
		}

		const Index3i* UVTriangles = nullptr;
		if (UVs != nullptr && UVs->IsSetTriangle(tid))
		{
			Index3i UVTri = UVs->GetTriangle(tid);
			UVTri.A = UVIndexMap[UVTri.A];  UVTri.B = UVIndexMap[UVTri.B];  UVTri.C = UVIndexMap[UVTri.C];
			TriUVs[0] = UVTri;
			UVTriangles = &TriUVs[0];
		}

		bool bSkipInitialization = false;
		std::pair<PolyMesh::Face, int> NewFace = PolyMesh.AddTriangle(Tri, GroupID, NormalTriangles, UVTriangles, bSkipInitialization);
	}

	// done...
}







template<typename OverlayType, typename AttribType>
void ExtractPolygonAttribLoopsFromOverlay(
	const FDynamicMesh3& Mesh,
	const FGroupTopology& Topology,
	const OverlayType* AttribOverlay,
	int FaceCount,
	const TArray<int>& PackedFaceLoops,
	const TArray<Index2i>& FaceLoopSpans,
	// outputs
	VertexElementMapper<AttribType>& AttribElementsOut,
	TArray<int>& PackedPolyAttribLoopsOut )
{
	check(AttribOverlay != nullptr);

	//int NumUVSets = 0;

	PackedPolyAttribLoopsOut.Init(-1, PackedFaceLoops.Num());

	for (int fi = 0; fi < FaceCount; ++fi)
	{
		const FGroupTopology::FGroup& Group = Topology.Groups[fi];
		Index2i LoopSpan = FaceLoopSpans[fi];
		if (LoopSpan[0] == -1) continue;

		const int* CornerLoop = &PackedFaceLoops[LoopSpan[0]];
		int* AttribLoop = &PackedPolyAttribLoopsOut[LoopSpan[0]];
		int NumCorners = LoopSpan[1];
		bool bAnyFailures = false;
		for (int j = 0; j < NumCorners; ++j)
		{
			const FGroupTopology::FCorner& Corner = Topology.Corners[CornerLoop[j]];
			int FoundElementID = -1;
			Mesh.EnumerateVertexTriangles(Corner.VertexID, [&](int tid) {
				if (FoundElementID == -1 && AttribOverlay->IsSetTriangle(tid) && Group.Triangles.Contains(tid))
				{
					FIndex3i MeshTri = Mesh.GetTriangle(tid);
					int TriVertIndex = MeshTri.IndexOf(Corner.VertexID);
					FIndex3i ElementTri = AttribOverlay->GetTriangle(tid);
					FoundElementID = ElementTri[TriVertIndex];
				}
			});
			if (FoundElementID == -1) 
			{ 
				bAnyFailures = true;		// need to know if this happens...
				ensure(false);
				break; 
			}

			AttribType Element = AttribOverlay->GetElement(FoundElementID);
			int ElementIndex = AttribElementsOut.FindOrInsertValue(FoundElementID, Corner.VertexID, Element);
			check(ElementIndex >= 0);
			AttribLoop[j] = ElementIndex;
		}
	}

	// append unique UV list to PolyMesh
	//int UVSetIndex = 0;
	//int NumPolyUVs = AttribMapper.Values.size();
	//PolyMesh.AddUVSet(NumPolyUVs);
	//for (int k = 0; k < NumPolyUVs; ++k) {
	//	PolyMesh.SetUV(k, AttribMapper.Values[k], UVSetIndex);
	//}
	//NumUVSets++;

}




// used to represent a set of per-tri/quad attribute values for a face, 
// one per attribute set/layer (eg UVs, Normals, etc)
template<int MaxNumSets>
struct PolyFaceAttributeBuilder
{
	Index3i Triangles[MaxNumSets];
	Index4i Quads[MaxNumSets];
	int MaxInitialized = -1;

	void Reset()
	{
		for (int j = 0; j < MaxNumSets; ++j)
		{
			Triangles[j] = Index3i(-1, -1, -1);
			Quads[j] = Index4i(-1, -1, -1, -1);
		}
		MaxInitialized = -1;
	}

	void SetTriangle(int SetIndex, const Index3i& AttribTri) {
		Triangles[SetIndex] = AttribTri;
		MaxInitialized = GS::Max(MaxInitialized, SetIndex);
	}
	void SetTriangle(int SetIndex, const int* Values) {
		for (int j = 0; j < 3; ++j)
			Triangles[SetIndex][j] = Values[j];
		MaxInitialized = GS::Max(MaxInitialized, SetIndex);
	}
	const Index3i* GetTriangles() const { return (MaxInitialized == 0) ? nullptr : &Triangles[0]; }

	void SetQuad(int SetIndex, const Index4i& AttribTri) {
		Quads[SetIndex] = AttribTri;
		MaxInitialized = GS::Max(MaxInitialized, SetIndex);
	}
	void SetQuad(int SetIndex, const int* Values) {
		for (int j = 0; j < 4; ++j)
			Quads[SetIndex][j] = Values[j];
		MaxInitialized = GS::Max(MaxInitialized, SetIndex);
	}
	const Index4i* GetQuads() const { return (MaxInitialized >= 0) ? &Quads[0] : nullptr; }
};


// helper class that stores a set of sequentially-packed per-face index loops,
// for up to 4 attribute-set layers.
template<int MaxNumSets>
struct PackedPolyFaceAttribLoops
{
	TArray<int> PackedLoopsSets[4];
	int NumSetsInUse = 0;

	TArray<int>& GetNextLoopsSet(int& SetIndexOut)
	{
		gs_debug_assert(NumSetsInUse < 4);
		SetIndexOut = NumSetsInUse;
		NumSetsInUse++;
		return PackedLoopsSets[SetIndexOut];
	}

	bool HasData() const { return NumSetsInUse > 0; }

	void CopyToBuilder_Tri(PolyFaceAttributeBuilder<MaxNumSets>& FaceBuilder, Index2i FaceLoopSpan)
	{
		for (int SetIndex = 0; SetIndex < NumSetsInUse; ++SetIndex)
		{
			const int* FaceLoop = &PackedLoopsSets[SetIndex][FaceLoopSpan[0]];
			FaceBuilder.SetTriangle(SetIndex, FaceLoop);
		}
	}

	void CopyToBuilder_Quad(PolyFaceAttributeBuilder<MaxNumSets>& FaceBuilder, Index2i FaceLoopSpan)
	{
		for (int SetIndex = 0; SetIndex < NumSetsInUse; ++SetIndex)
		{
			const int* FaceLoop = &PackedLoopsSets[SetIndex][FaceLoopSpan[0]];
			FaceBuilder.SetQuad(SetIndex, FaceLoop);
		}
	}

	void CopyToArray(GS::unsafe_vector<int>& ArrayOut, Index2i FaceLoopSpan)
	{
		for (int SetIndex = 0; SetIndex < NumSetsInUse; ++SetIndex)
		{
			const int* FaceLoop = &PackedLoopsSets[SetIndex][FaceLoopSpan[0]];
			ArrayOut.append(FaceLoop, FaceLoopSpan[1]);
		}
	}
};




bool GS::ConvertDynamicMeshGroupsToPolyMesh(const FDynamicMesh3& DynamicMeshIn, int GroupLayer, PolyMesh& PolyMesh, GSErrorSet* Errors)
{
	check(GroupLayer == 0);		// todo support this

	const FDynamicMesh3* UseMesh = &DynamicMeshIn;
	FDynamicMesh3 LocalMesh;
	if (DynamicMeshIn.IsCompactV() == false)
	{
		LocalMesh.CompactCopy(DynamicMeshIn);
		UseMesh = &LocalMesh;
	}

	const FDynamicMeshMaterialAttribute* MaterialIDs = (UseMesh->HasAttributes()) ? UseMesh->Attributes()->GetMaterialID() : nullptr;

	// ARGH need extra corner support or some way to workaround faces w/ invalid loops...
	FGroupTopology Topology(UseMesh, true);

	// build up a list of face polygons, while also sanity-checking the faces for validity.
	// face polygons (ie loops of corner indices) are packed into a linear array, indexable via FaceLoopSpans
	int FaceCount = Topology.Groups.Num();
	TArray<int> PackedFaceLoops;
	PackedFaceLoops.Reserve(FaceCount * 4);
	TArray<Index2i> FaceLoopSpans;
	FaceLoopSpans.Reserve(FaceCount);
	for (int fi = 0; fi < FaceCount; ++fi)
	{
		if (Topology.Groups[fi].Boundaries.Num() != 1) {	// only one boundary loop per face
			GS::AppendErrorUnique(Errors, EGSStandardErrors::InvalidTopology_MultipleGroupBoundaries,
				"A Polygroup Face with multiple boundary loops was found. This face cannot be converted to a simple polygon.");
			return false;
		}
		const FGroupTopology::FGroupBoundary& FaceBoundary = Topology.Groups[fi].Boundaries[0];
		int NumEdges = FaceBoundary.GroupEdges.Num();
		if (NumEdges < 3) {
			GS::AppendErrorUnique(Errors, EGSStandardErrors::InvalidTopology_DegenerateFace,
				"A Polygroup Face bounded by only 1 or 2 Polygroup Edges was found. This face would result in a degenerate polygon.");
			// face needs at least 3 corners, going to need to do something here...
			FaceLoopSpans.Add(Index2i(-1, NumEdges));
			continue;
		}

		TArray<int> CornerLoop;
		FGroupTopology::FGroupEdge PrevEdge = Topology.Edges[FaceBoundary.GroupEdges[0]];
		CornerLoop.Add(PrevEdge.EndpointCorners.A);
		CornerLoop.Add(PrevEdge.EndpointCorners.B);
		for (int ei = 1; ei < NumEdges - 1; ++ei)
		{
			FGroupTopology::FGroupEdge NextEdge = Topology.Edges[FaceBoundary.GroupEdges[ei]];
			int SharedCorner = NextEdge.EndpointCorners.Contains(PrevEdge.EndpointCorners.A) ?
				PrevEdge.EndpointCorners.A : PrevEdge.EndpointCorners.B;
			check(NextEdge.EndpointCorners.Contains(SharedCorner));
			if (ei == 1 && CornerLoop[1] != SharedCorner)
				Swap(CornerLoop[0], CornerLoop[1]);
			int NextCorner = NextEdge.EndpointCorners.OtherElement(SharedCorner);
			CornerLoop.Add(NextCorner);
			PrevEdge = NextEdge;
		}
		FGroupTopology::FGroupEdge LastEdge = Topology.Edges[FaceBoundary.GroupEdges[NumEdges-1]];
		check(CornerLoop.Contains(LastEdge.EndpointCorners.A) && CornerLoop.Contains(LastEdge.EndpointCorners.B));

		int CurStart = PackedFaceLoops.Num();
		int Index = FaceLoopSpans.Add( Index2i( CurStart, CornerLoop.Num() ) );
		check(Index == fi);
		for (int j = 0; j < NumEdges; ++j)
			PackedFaceLoops.Add(CornerLoop[j]);
	}
	

	PolyMesh.Clear();

	// since we are building a mesh from a GroupTopology, other group layers might not nicely align
	// with single group faces. But they might! should improve this...
	PolyMesh.SetNumGroupSets(1);		

	int TopoVertexCount = Topology.Corners.Num();
	for (int CornerID = 0; CornerID < TopoVertexCount; ++CornerID)
	{
		int vid = Topology.GetCornerVertexID(CornerID);
		int new_vid = PolyMesh.AddVertex( UseMesh->GetVertex(vid) );
		gs_debug_assert(new_vid == CornerID);
	}

	PackedPolyFaceAttribLoops<4> PackedNormalLoopsSets;
	int NumSourceNormalLayers = UseMesh->HasAttributes() ? GS::Min(UseMesh->Attributes()->NumNormalLayers(),4) : 0;
	for (int li = 0; li < NumSourceNormalLayers; ++li)
	{
		const FDynamicMeshNormalOverlay* Normals = UseMesh->Attributes()->GetNormalLayer(li);
		if (!ensure(Normals)) continue;
		int NormalSetIndex = 0;
		TArray<int>& UsePackedNormalLoops = PackedNormalLoopsSets.GetNextLoopsSet(NormalSetIndex);
		VertexElementMapper<Vector3f> NormalMapper;
		ExtractPolygonAttribLoopsFromOverlay<FDynamicMeshNormalOverlay, Vector3f>(
			*UseMesh, Topology, Normals,
			FaceCount, PackedFaceLoops, FaceLoopSpans,
			NormalMapper, UsePackedNormalLoops);

		int NumPolyNormals = NormalMapper.Values.size();
		PolyMesh.AddNormalSet(NumPolyNormals);
		for (int k = 0; k < NumPolyNormals; ++k) 
			PolyMesh.SetNormal(k, NormalMapper.Values[k], NormalSetIndex);
	}

	PackedPolyFaceAttribLoops<4> PackedUVLoopsSets;
	int NumSourceUVLayers = UseMesh->HasAttributes() ? GS::Min(UseMesh->Attributes()->NumUVLayers(),4) : 0;
	for ( int li = 0; li < NumSourceUVLayers; ++li)
	{
		const FDynamicMeshUVOverlay* UVs = UseMesh->Attributes()->GetUVLayer(li);
		if (!ensure(UVs)) continue;
		int UVSetIndex = 0;
		TArray<int>& UsePackedUVLoops = PackedUVLoopsSets.GetNextLoopsSet(UVSetIndex);
		VertexElementMapper<Vector2f> UVMapper;
		ExtractPolygonAttribLoopsFromOverlay<FDynamicMeshUVOverlay, Vector2f>(
			*UseMesh, Topology, UVs,
			FaceCount, PackedFaceLoops, FaceLoopSpans,
			UVMapper, UsePackedUVLoops);

		int NumPolyUVs = UVMapper.Values.size();
		PolyMesh.AddUVSet(NumPolyUVs);
		for (int k = 0; k < NumPolyUVs; ++k) 
			PolyMesh.SetUV(k, (Vector2d)UVMapper.Values[k], UVSetIndex);
	}


	PackedPolyFaceAttribLoops<1> PackedColorLoopsSet;
	int NumSourceColorLayers = (UseMesh->HasAttributes() && UseMesh->Attributes()->HasPrimaryColors()) ? 1 : 0;
	for (int li = 0; li < NumSourceColorLayers; ++li)
	{
		const FDynamicMeshColorOverlay* Colors = UseMesh->Attributes()->PrimaryColors();
		if (ensure(Colors))
		{
			int ColorSetIndex = 0;
			TArray<int>& UsePackedColorLoops = PackedColorLoopsSet.GetNextLoopsSet(ColorSetIndex);
			VertexElementMapper<Vector4f> ColorMapper;
			ExtractPolygonAttribLoopsFromOverlay<FDynamicMeshColorOverlay, Vector4f>(
				*UseMesh, Topology, Colors,
				FaceCount, PackedFaceLoops, FaceLoopSpans,
				ColorMapper, UsePackedColorLoops);

			int NumPolyColors = ColorMapper.Values.size();
			PolyMesh.AddColorSet(NumPolyColors);
			for (int k = 0; k < NumPolyColors; ++k)
				PolyMesh.SetColor(k, ColorMapper.Values[k], ColorSetIndex);
		}
	}


	PolyFaceAttributeBuilder<4> UVBuilder, NormalBuilder;
	PolyFaceAttributeBuilder<1> ColorBuilder;
	for (int fi = 0; fi < FaceCount; ++fi)
	{
		const FGroupTopology::FGroup& Group = Topology.Groups[fi];
		Index2i LoopSpan = FaceLoopSpans[fi];
		if (LoopSpan[0] == -1)
		{
			// broken face, needs extra corners - emit as triangles?
			continue;
		}

		const int* CornerLoop = &PackedFaceLoops[LoopSpan[0]];
		int NumCorners = LoopSpan[1];
		UVBuilder.Reset();
		NormalBuilder.Reset();
		ColorBuilder.Reset();

		bool bSkipInitialization = false;
		std::pair<PolyMesh::Face, int> NewFace;
		if (NumCorners == 3)
		{
			const Index3i* NormalTriangles = nullptr;
			PackedUVLoopsSets.CopyToBuilder_Tri(UVBuilder, LoopSpan);
			PackedNormalLoopsSets.CopyToBuilder_Tri(NormalBuilder, LoopSpan);
			PackedColorLoopsSet.CopyToBuilder_Tri(ColorBuilder, LoopSpan);
			FIndex3i Triangle(CornerLoop[0], CornerLoop[1], CornerLoop[2]);
			NewFace = PolyMesh.AddTriangle(Triangle, Group.GroupID, NormalBuilder.GetTriangles(), UVBuilder.GetTriangles(), bSkipInitialization);
			PolyMesh.InitializeFaceAttributes(NewFace.second, PolyMesh::EAttributeType::Color, (const void*)ColorBuilder.GetTriangles());
		}
		else if (NumCorners == 4)
		{
			const Index4i* NormalQuads = nullptr;
			PackedUVLoopsSets.CopyToBuilder_Quad(UVBuilder, LoopSpan);
			PackedNormalLoopsSets.CopyToBuilder_Quad(NormalBuilder, LoopSpan);
			PackedColorLoopsSet.CopyToBuilder_Quad(ColorBuilder, LoopSpan);
			FIndex4i Quad(CornerLoop[0], CornerLoop[1], CornerLoop[2], CornerLoop[3]);
			NewFace = PolyMesh.AddQuad(Quad, Group.GroupID, NormalBuilder.GetQuads(), UVBuilder.GetQuads(), bSkipInitialization);
			PolyMesh.InitializeFaceAttributes(NewFace.second, PolyMesh::EAttributeType::Color, (const void*)ColorBuilder.GetQuads());
		}
		else
		{
			check(NumCorners > 4);

			GS::PolyMesh::Polygon NewPoly;
			NewPoly.VertexCount = NumCorners;
			NewPoly.Vertices.append(CornerLoop, NumCorners);
			PackedUVLoopsSets.CopyToArray(NewPoly.UVs, LoopSpan);
			PackedNormalLoopsSets.CopyToArray(NewPoly.Normals, LoopSpan);

			NewFace = PolyMesh.AddPolygon(std::move(NewPoly), Group.GroupID);
			
			GS::unsafe_vector<int> ColorArray;
			PackedColorLoopsSet.CopyToArray(ColorArray, LoopSpan);
			PolyMesh.InitializeFaceAttributes(NewFace.second, PolyMesh::EAttributeType::Color, (const void*)&ColorArray);
		}

	}

	return true;
}






void GS::ConvertPolyMeshToDynamicMesh(const PolyMesh& PolyMesh, FDynamicMesh3& DynamicMesh)
{
	bool bWantNormals = true;
	bool bWantUVs = true;
	bool bWantVtxColors = true;

	int UseNormalSet = 0;
	bool bHaveNormals = bWantNormals && (PolyMesh.GetNumNormalSets() > UseNormalSet) && (PolyMesh.GetNormalCount(UseNormalSet) > 0);

	int UseUVSet = 0;
	bool bHaveUVs = bWantUVs && (PolyMesh.GetNumUVSets() > UseUVSet) && (PolyMesh.GetUVCount(UseUVSet) > 0);

	int UseColorSet = 0;
	bool bHaveColors = bWantVtxColors && (PolyMesh.GetNumColorSets() > UseColorSet) && (PolyMesh.GetColorCount(UseColorSet) > 0);

	int UseGroupSet = 0;
	bool bHaveGroups = (PolyMesh.GetNumFaceGroupSets() > UseGroupSet);

	DynamicMesh.Clear();
	DynamicMesh.EnableTriangleGroups();

	int NumVertices = PolyMesh.GetVertexCount();
	for (int vi = 0; vi < NumVertices; ++vi) {
		/*int vid = */DynamicMesh.AppendVertex(PolyMesh.GetPosition(vi));
	}


	FDynamicMeshAttributeSet* Attribs = nullptr;
	if (bWantNormals || bWantUVs || bWantVtxColors) {
		DynamicMesh.EnableAttributes();
		Attribs = DynamicMesh.Attributes();
	}

	FDynamicMeshNormalOverlay* NormalOverlay = (Attribs && bWantNormals) ? Attribs->PrimaryNormals() : nullptr;
	if (bHaveNormals)
	{
		int NumNormals = PolyMesh.GetNormalCount(UseNormalSet);
		for (int ni = 0; ni < NumNormals; ++ni) {
			Vector3f Normal = PolyMesh.GetNormal(ni, UseNormalSet);
			NormalOverlay->AppendElement(Normal);
		}
	}

	FDynamicMeshUVOverlay* UVOverlay = (Attribs && bWantUVs) ? Attribs->GetUVLayer(UseUVSet) : nullptr;
	if (bHaveUVs)
	{
		int NumUVs = PolyMesh.GetUVCount(UseUVSet);
		for (int ui = 0; ui < NumUVs; ++ui) {
			Vector2d UV = PolyMesh.GetUV(ui, UseUVSet);
			UVOverlay->AppendElement((Vector2f)UV);
		}
	}

	FDynamicMeshColorOverlay* ColorOverlay = (Attribs && bWantVtxColors) ? Attribs->PrimaryColors() : nullptr;
	if (bHaveColors)
	{
		int NumColors = PolyMesh.GetColorCount(UseColorSet);
		for (int ci = 0; ci < NumColors; ++ci) {
			Vector4f Color = PolyMesh.GetColor(ci, UseColorSet);
			ColorOverlay->AppendElement(Color);
		}
	}

	int NumFaces = PolyMesh.GetFaceCount();
	for (int FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		int GroupID = (bHaveGroups) ? PolyMesh.GetFaceGroup(FaceIndex, UseGroupSet) : 0;
		PolyMesh::Face Face = PolyMesh.GetFace(FaceIndex);

		if (Face.IsTriangle())
		{
			Index3i TriV = PolyMesh.GetTriangle(Face);
			int tid = DynamicMesh.AppendTriangle(TriV, GroupID);

			if (bHaveNormals) {
				Index3i TriN;
				for (int j = 0; j < 3; ++j)
					TriN[j] = PolyMesh.GetFaceVertexNormalIndex(Face, j, UseNormalSet);
				NormalOverlay->SetTriangle(tid, TriN);
			}
			if (bHaveUVs) {
				Index3i TriUV;
				for (int j = 0; j < 3; ++j)
					TriUV[j] = PolyMesh.GetFaceVertexUVIndex(Face, j, UseUVSet);
				UVOverlay->SetTriangle(tid, TriUV);
			}
			if (bHaveColors) {
				Index3i TriC;
				for (int j = 0; j < 3; ++j)
					TriC[j] = PolyMesh.GetFaceVertexColorIndex(Face, j, UseColorSet);
				ColorOverlay->SetTriangle(tid, TriC);
			}

		}
		else if (Face.IsQuad())
		{
			Index4i QuadV = PolyMesh.GetQuad(Face);
			int tida = DynamicMesh.AppendTriangle(QuadV.A, QuadV.B, QuadV.C, GroupID);
			int tidb = DynamicMesh.AppendTriangle(QuadV.A, QuadV.C, QuadV.D, GroupID);
			if (bHaveNormals) {
				Index4i QuadN;
				for (int j = 0; j < 4; ++j)
					QuadN[j] = PolyMesh.GetFaceVertexNormalIndex(Face, j, UseNormalSet);
				NormalOverlay->SetTriangle(tida, FIndex3i(QuadN.A, QuadN.B, QuadN.C));
				NormalOverlay->SetTriangle(tidb, FIndex3i(QuadN.A, QuadN.C, QuadN.D));
			}
			if (bHaveUVs) {
				Index4i QuadUV;
				for (int j = 0; j < 4; ++j)
					QuadUV[j] = PolyMesh.GetFaceVertexUVIndex(Face, j, UseUVSet);
				UVOverlay->SetTriangle(tida, FIndex3i(QuadUV.A, QuadUV.B, QuadUV.C));
				UVOverlay->SetTriangle(tidb, FIndex3i(QuadUV.A, QuadUV.C, QuadUV.D));
			}
			if (bHaveColors) {
				Index4i QuadC;
				for (int j = 0; j < 4; ++j)
					QuadC[j] = PolyMesh.GetFaceVertexColorIndex(Face, j, UseColorSet);
				ColorOverlay->SetTriangle(tida, FIndex3i(QuadC.A, QuadC.B, QuadC.C));
				ColorOverlay->SetTriangle(tidb, FIndex3i(QuadC.A, QuadC.C, QuadC.D));
			}
		}
		else if (Face.IsPolygon())
		{
			// TODO: need to call a triangulator here
			ensure(false);
		}

	}



}







void GS::AddUV1BarycentricsChannel( FDynamicMesh3& DynamicMesh )
{
	FDynamicMeshAttributeSet* Attribs = DynamicMesh.Attributes();
	Attribs->SetNumUVLayers(2);
	FDynamicMeshUVOverlay* UVOverlay = Attribs->GetUVLayer(1);
	UVOverlay->ClearElements();
	for (int tid : DynamicMesh.TriangleIndicesItr()) {
		int a = UVOverlay->AppendElement(FVector2f(1, 0));
		int b = UVOverlay->AppendElement(FVector2f(0, 1));
		int c = UVOverlay->AppendElement(FVector2f(0, 0));
		UVOverlay->SetTriangle(tid, FIndex3i(a, b, c));
	}
}