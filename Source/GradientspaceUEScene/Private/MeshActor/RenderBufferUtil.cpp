// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshActor/RenderBufferUtil.h"
#include "MeshActor/DefaultMeshRenderBuffers.h"
#include "Mesh/DenseMesh.h"
#include "IndexTypes.h"
#include "VectorUtil.h"

using namespace GS;
using namespace UE::Geometry;


void GS::InitializeRenderBuffersFromMesh(
	const DenseMesh& Mesh,
	FMeshRenderBuffers& RenderBuffers)
{
	InitializeRenderBuffersFromMesh_LocalOptimize(Mesh, RenderBuffers);
}

void GS::InitializeRenderBuffersFromMesh_Fastest(
	const DenseMesh& Mesh,
	FMeshRenderBuffers& RenderBuffers)
{
	int NumTriangles = Mesh.GetTriangleCount();
	if (NumTriangles == 0) return;

	RenderBuffers.TriangleCount = NumTriangles;

	int32 NumTexCoords = 1;
	TArray<FIntVector, TFixedAllocator<MAX_STATIC_TEXCOORDS>> UVTriangles;
	UVTriangles.SetNum(NumTexCoords);

	int NumVertices = NumTriangles * 3;
	RenderBuffers.PositionVertexBuffer.Init(NumVertices);
	RenderBuffers.StaticMeshVertexBuffer.Init(NumVertices, NumTexCoords);
	RenderBuffers.ColorVertexBuffer.Init(NumVertices);
	RenderBuffers.IndexBuffer.Indices.AddUninitialized(NumTriangles * 3);

	int AccumVertIndex = 0;
	int AccumTriIndex = 0;
	for (int tid = 0; tid < NumTriangles; ++tid)
	{
		Index3i Triangle = Mesh.GetTriangle(tid);
		TriVtxNormals TriNormals = Mesh.GetTriVtxNormals(tid);
		TriVtxUVs TriUVs = Mesh.GetTriVtxUVs(tid);
		TriVtxColors TriColors = Mesh.GetTriVtxColors(tid);

		for (int j = 0; j < 3; ++j)
		{
			int32 VertIndex = AccumVertIndex++;

			Vector3d V = Mesh.GetPosition(Triangle[j]);
			RenderBuffers.PositionVertexBuffer.VertexPosition(VertIndex) = FVector3f((float)V.X, (float)V.Y, (float)V.Z);

			FVector3f Normal(TriNormals[j].X, TriNormals[j].Y, TriNormals[j].Z), TangentX, TangentY;
			UE::Geometry::VectorUtil::MakePerpVectors(Normal, TangentX, TangentY);
			RenderBuffers.StaticMeshVertexBuffer.SetVertexTangents(VertIndex, TangentX, TangentY, Normal);

			int UVIndex = 0;
			FVector2f UV(TriUVs[j].X, TriUVs[j].Y);
			RenderBuffers.StaticMeshVertexBuffer.SetVertexUV(VertIndex, UVIndex, UV);

			FColor Color(TriColors[j].R, TriColors[j].G, TriColors[j].B, TriColors[j].A);
			RenderBuffers.ColorVertexBuffer.VertexColor(VertIndex) = Color;

			RenderBuffers.IndexBuffer.Indices[AccumTriIndex++] = VertIndex;
		}
	}
}



void GS::InitializeRenderBuffersFromMesh_LocalOptimize(
	const DenseMesh& Mesh,
	FMeshRenderBuffers& RenderBuffers)
{
	int NumTriangles = Mesh.GetTriangleCount();
	if (NumTriangles == 0) return;

	RenderBuffers.TriangleCount = NumTriangles;

	int32 NumTexCoords = 1;
	TArray<FIntVector, TFixedAllocator<MAX_STATIC_TEXCOORDS>> UVTriangles;
	UVTriangles.SetNum(NumTexCoords);

	// todo: investigate FMeshBuildVertexData / FMeshBuildVertexView / TStridedView,

	int LRUSize = 32;   // TODO: experiments to try to find a good trade-off here
	TArray<int32> RecentSourceVtxIDs, RecentBufferVtxIndices;
	RecentSourceVtxIDs.Init(-1, LRUSize);
	RecentBufferVtxIndices.Init(-1, LRUSize);
	TArray<FStaticMeshBuildVertex> RecentVtxData;
	FStaticMeshBuildVertex EmptyVertex;
	EmptyVertex.Position = EmptyVertex.TangentX = EmptyVertex.TangentY = EmptyVertex.TangentZ = FVector3f::Zero();
	for (int k = 0; k < MAX_STATIC_TEXCOORDS; ++k) { EmptyVertex.UVs[k] = FVector2f::Zero(); }
	EmptyVertex.Color = FColor::Black;
	RecentVtxData.Init(EmptyVertex, LRUSize);
	int LRUIndex = 0;

	auto IsSameVertex = [](const FStaticMeshBuildVertex& A, const FStaticMeshBuildVertex& B)
	{
		return A.Color == B.Color && A.TangentZ == B.TangentZ && A.UVs[0] == B.UVs[0];
	};

	TArray<FStaticMeshBuildVertex> Vertices;
	Vertices.Reserve(NumTriangles * 3);		// worst case

	int AccumTriIndex = 0;
	RenderBuffers.IndexBuffer.Indices.AddUninitialized(NumTriangles * 3);

	int SavedVertices = 0;
	for (int tid = 0; tid < NumTriangles; ++tid)
	{
		Index3i Triangle = Mesh.GetTriangle(tid);
		const TriVtxNormals& TriNormals = Mesh.GetTriVtxNormals(tid);
		const TriVtxUVs& TriUVs = Mesh.GetTriVtxUVs(tid);
		const TriVtxColors& TriColors = Mesh.GetTriVtxColors(tid);

		FIndex3i BufferTriangle(-1, -1, -1);
		for (int j = 0; j < 3; ++j)
		{
			int vid = Triangle[j];

			FStaticMeshBuildVertex SMVertex = EmptyVertex;
			SMVertex.TangentZ = TriNormals[j];
			SMVertex.Color = (FColor)(TriColors[j]);
			SMVertex.UVs[0] = TriUVs[j];

			int FoundIndex = RecentSourceVtxIDs.IndexOfByKey(vid);
			if (FoundIndex != INDEX_NONE)
			{
				if (IsSameVertex(RecentVtxData[FoundIndex], SMVertex))
				{
					BufferTriangle[j] = RecentBufferVtxIndices[FoundIndex];
					SavedVertices++;
				}
			}

			if (BufferTriangle[j] == -1)
			{
				SMVertex.Position = (FVector3f)Mesh.GetPosition(vid);
				UE::Geometry::VectorUtil::MakePerpVectors(SMVertex.TangentZ, SMVertex.TangentX, SMVertex.TangentY);
				BufferTriangle[j] = Vertices.Num();
				Vertices.Add(SMVertex);

				if (FoundIndex != INDEX_NONE)
				{
					RecentBufferVtxIndices[FoundIndex] = BufferTriangle[j];
					RecentVtxData[FoundIndex] = SMVertex;
				}
				else
				{
					// TODO: this is not really correct, probably if we find a vertex above we need to swap it with the "oldest"
					// vertex

					RecentSourceVtxIDs[LRUIndex] = vid;
					RecentBufferVtxIndices[LRUIndex] = BufferTriangle[j];
					RecentVtxData[LRUIndex] = SMVertex;
					LRUIndex = (LRUIndex + 1) % LRUSize;
				}
			}
		}

		for (int j = 0; j < 3; ++j)
		{
			check(BufferTriangle[j] >= 0);
			RenderBuffers.IndexBuffer.Indices[AccumTriIndex++] = BufferTriangle[j];
		}
	};

	//UE_LOG(LogTemp, Warning, TEXT("Saved %d vertices - %d of %d"), SavedVertices, Vertices.Num(), 3 * NumTriangles);

	RenderBuffers.PositionVertexBuffer.Init(Vertices, false);
	RenderBuffers.StaticMeshVertexBuffer.Init(Vertices, NumTexCoords, false);
	RenderBuffers.ColorVertexBuffer.Init(Vertices, false);
}
