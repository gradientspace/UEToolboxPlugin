// Copyright Gradientspace Corp. All Rights Reserved.

#include "Utility/UEGeometryCollisionScene.h"
#include "Utility/GSUEBoundsUtil.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "UDynamicMesh.h"
#include "Spatial/SparseDynamicOctree3.h"

#include "Engine/StaticMesh.h"
#include "StaticMeshLODResourcesAdapter.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"

#include "Async/ParallelFor.h"

using namespace GS;
using namespace UE::Geometry;




struct PackedMinimalMesh
{
	TArray<FVector3d> Positions;
	TArray<FIndex3i> Triangles;

	void Initialize(const FDynamicMesh3& DynamicMesh)
	{
		check(DynamicMesh.IsCompactV());		// for now

		int NV = DynamicMesh.VertexCount();
		Positions.SetNum(NV);
		for (int k = 0; k < NV; ++k)
			Positions[k] = DynamicMesh.GetVertex(k);

		int NT = DynamicMesh.TriangleCount();
		Triangles.SetNum(NT);
		for (int k = 0; k < NT; ++k)
			Triangles[k] = DynamicMesh.GetTriangle(k);
	}

	template<typename AdapterType>
	void Initialize(const AdapterType& TriAdapter)
	{
		int NV = TriAdapter.VertexCount();
		Positions.SetNum(NV);
		for (int k = 0; k < NV; ++k)
			Positions[k] = TriAdapter.GetVertex(k);

		int NT = TriAdapter.TriangleCount();
		Triangles.SetNum(NT);
		for (int k = 0; k < NT; ++k)
			Triangles[k] = TriAdapter.GetTriangle(k);
	}

	// mesh API
	inline bool IsTriangle(int32 index) const { return true; }
	inline bool IsVertex(int32 index) const { return true; }
	inline int32 MaxTriangleID() const { return Triangles.Num(); }
	inline int32 MaxVertexID() const { return Positions.Num(); }
	inline int32 TriangleCount() const { return Triangles.Num(); }
	inline int32 VertexCount() const { return Positions.Num(); }
	inline uint64 GetChangeStamp() const { return 0; }

	inline FIndex3i GetTriangle(int ti) const { return Triangles[ti]; }
	inline FVector3d GetVertex(int vi) const { return Positions[vi]; }
	inline void GetTriVertices(int TID, FVector3d& V0, FVector3d& V1, FVector3d& V2) const {
		FIndex3i TriIndices = GetTriangle(TID);
		V0 = Positions[TriIndices.A];
		V1 = Positions[TriIndices.B];
		V2 = Positions[TriIndices.C];
	}
};
typedef TMeshAABBTree3<PackedMinimalMesh> MinimalMeshAABBTree3;



class FBaseCollider : public IGeometryCollider
{
public:
	FGeometryHandle ParentGeometryHandle;
	PackedMinimalMesh MinimalMesh;
	TUniquePtr<MinimalMeshAABBTree3> AABBTree;

	FBaseCollider() = default;

	virtual bool Build(const FGeometryCollisionSceneBuildOptions& BuildOptions) override
	{
		AABBTree = MakeUnique<MinimalMeshAABBTree3>(&MinimalMesh, true);
		ensure(MinimalMesh.TriangleCount() > 0);
		return true;
	}

	virtual int GetTriangleCount() const { return MinimalMesh.Triangles.Num(); }

	virtual FAxisAlignedBox3d GetWorldBounds(TFunctionRef<FVector3d(const FVector3d&)> LocalToWorldFunc) override
	{
		FAxisAlignedBox3d Bounds = GS::ComputeMeshVerticesBounds<PackedMinimalMesh>(MinimalMesh, LocalToWorldFunc);
		return Bounds;
	}

	virtual bool RayIntersection(const FRay3d& WorldRay, 
		const FTransformSequence3d& LocalToWorldTransform, 
		FGeometryCollisionSceneRayHit& WorldHitResultOut) override
	{
		FVector3d LocalOrigin = LocalToWorldTransform.InverseTransformPosition(WorldRay.Origin);
		FVector3d LocalDir = Normalized(LocalToWorldTransform.InverseTransformPosition(WorldRay.PointAt(1.0)) - LocalOrigin);
		FRay3d LocalRay(LocalOrigin, LocalDir);

		double RayHitParam; int32 HitTID; FVector3d HitBaryCoords;
		if (!AABBTree->FindNearestHitTriangle(LocalRay, RayHitParam, HitTID, HitBaryCoords))
			return false;

		WorldHitResultOut.WorldRay = WorldRay;
		WorldHitResultOut.LocalHitPos = LocalRay.PointAt(RayHitParam);
		WorldHitResultOut.WorldHitPos = LocalToWorldTransform.TransformPosition(WorldHitResultOut.LocalHitPos);
		WorldHitResultOut.RayDistance = WorldRay.GetParameter(WorldHitResultOut.WorldHitPos);
		WorldHitResultOut.GeometryIndex = HitTID;
		WorldHitResultOut.HitBaryCoords = HitBaryCoords;
		WorldHitResultOut.HitGeometryHandle = ParentGeometryHandle;
		WorldHitResultOut.LocalToWorld = LocalToWorldTransform;
		return true;
	}

	virtual bool TestForCollision(const IGeometryCollider& OtherSpatial,
		const UE::Geometry::FTransformSequence3d& OtherTreeLocalToWorld,
		const UE::Geometry::FTransformSequence3d& LocalToWorldTransform)
	{
		const MinimalMeshAABBTree3* RealOtherSpatial = ((FBaseCollider&)OtherSpatial).AABBTree.Get();

		auto OtherLocalToLocalT = [&](const FVector3d& OtherP)
		{
			FVector3d WorldP = OtherTreeLocalToWorld.TransformPosition(OtherP);
			WorldP = LocalToWorldTransform.InverseTransformPosition(WorldP);
			return WorldP;
		};

		IMeshSpatial::FQueryOptions QueryOptions;
		bool bIntersects = AABBTree->TestIntersection(*RealOtherSpatial, OtherLocalToLocalT, QueryOptions, QueryOptions);
		return bIntersects;
	}
};


class FStaticMeshCollider : public FBaseCollider
{
public:
	virtual bool Build(const FGeometryCollisionSceneBuildOptions& BuildOptions) override
	{
		UStaticMesh* StaticMesh = ParentGeometryHandle.GetStaticMesh();
		if (!IsValid(StaticMesh))
			return false;

		const FStaticMeshLODResources* LODResources =
			StaticMesh->GetRenderData()->GetCurrentFirstLOD(0);
		if (!LODResources)
			return false;

		FStaticMeshLODResourcesMeshAdapter Adapter(LODResources);
		//Adapter.SetBuildScale();		// build scale is baked into LODResources data, no?

		this->MinimalMesh.Initialize(Adapter);
		return FBaseCollider::Build(BuildOptions);
	}
};



class FDynamicMeshCollider : public FBaseCollider
{
public:
	virtual bool Build(const FGeometryCollisionSceneBuildOptions& BuildOptions) override
	{
		UDynamicMesh* DynamicMesh = ParentGeometryHandle.GetDynamicMesh();
		if (!IsValid(DynamicMesh))
			return false;

		DynamicMesh->ProcessMesh([&](const FDynamicMesh3& Mesh) {
			this->MinimalMesh.Initialize(Mesh);
		});
		if (this->MinimalMesh.TriangleCount() == 0)
			return false;
		return FBaseCollider::Build(BuildOptions);
	}
};




static TUniquePtr<IGeometryCollider> ConstructColliderForMesh( const FGeometryHandle& GeometryHandle, const FGeometryCollisionSceneBuildOptions& BuildOptions )
{
	if (GeometryHandle.GeometryType == ESceneGeometryType::StaticMeshAsset )
	{
		UStaticMesh* StaticMesh = GeometryHandle.GetStaticMesh();
		if (StaticMesh != nullptr) {
			TUniquePtr<FStaticMeshCollider> Collider = MakeUnique<FStaticMeshCollider>();
			Collider->ParentGeometryHandle = GeometryHandle;
			return Collider;
		}
	}
	else if (GeometryHandle.GeometryType == ESceneGeometryType::UDynamicMeshObject)
	{
		UDynamicMesh* DynamicMesh = GeometryHandle.GetDynamicMesh();
		if (DynamicMesh != nullptr) {
			TUniquePtr<FDynamicMeshCollider> Collider = MakeUnique<FDynamicMeshCollider>();
			Collider->ParentGeometryHandle = GeometryHandle;
			return Collider;
		}
	}

	return TUniquePtr<IGeometryCollider>();
}




void FGeometryCollisionScene::Initialize(FGeometryCollisionSceneBuildOptions BuildOptionsIn)
{
	this->BuildOptions = BuildOptionsIn;
}


void FGeometryCollisionScene::OnUniqueGeometryAdded(FUniqueGeometry* Geometry)
{
	int Index = Geometry->GeometryIndex;
	
	Colliders.SetNum(Index + 1);
	
	Colliders[Index] = ConstructColliderForMesh(Geometry->GeometryHandle, BuildOptions);
	PendingColliderBuilds.Add(Index);
}



void FGeometryCollisionScene::UpdateBuild()
{
	TArray<IGeometryCollider*> BuildList;
	for (int Index : PendingColliderBuilds)
		BuildList.Add(Colliders[Index].Get());

	// parallel build of all the spatial data structures
	ParallelFor(BuildList.Num(), [&](int32 i)
	{
		bool bOK = BuildList[i]->Build(BuildOptions);
		if (!bOK) {
			// print message?
		}
	});

	InitializeAndBuildGeometryInstances();
}


void FGeometryCollisionScene::InitializeAndBuildGeometryInstances()
{
	AllGeometryInstances.Reset();
	for (const TUniquePtr<FGeometryActor>& Actor : SceneActors)
	{
		int NumInstances = Actor->Instances.Num();
		for (int i = 0; i < NumInstances; ++i)
		{
			FPlacedGeometryInstance InstanceInfo;
			InstanceInfo.Actor = Actor.Get();
			InstanceInfo.InstanceIndex = i;
			if (const FUniqueGeometry* FoundGeo = this->FindGeometry(Actor->Instances[i].GeometryHandle) ) {
				InstanceInfo.ColliderIndex = FoundGeo->GeometryIndex;
				AllGeometryInstances.Add(InstanceInfo);
			}
		}
	}

	AllGeometryInstances.Sort([&](const FPlacedGeometryInstance& A, const FPlacedGeometryInstance& B)	{
		return A.ColliderIndex < B.ColliderIndex;
	});
	int32 TotalNumInstances = AllGeometryInstances.Num();

	// todo this could be done async if we could lock base class...
	RecomputeAllGeometryInstanceBounds();
	RecomputeSceneWorldBounds();
	RebuildSceneOctree();
}



void FGeometryCollisionScene::UpdateAllTransforms()
{
	FGeometryScene::UpdateAllTransforms();

	// todo this could be done async if we could lock base class...
	RecomputeAllGeometryInstanceBounds();
	RecomputeSceneWorldBounds();
	RebuildSceneOctree();
}


void FGeometryCollisionScene::RecomputeAllGeometryInstanceBounds()
{
	int32 TotalNumInstances = AllGeometryInstances.Num();

	// parallel-compute the world bounds for each geometry instance
	ParallelFor(TotalNumInstances, [&](int32 k) {
		int ColliderIndex = AllGeometryInstances[k].ColliderIndex;
		const TUniquePtr<IGeometryCollider>& Collider = Colliders[ColliderIndex];

		const auto& Transform = AllGeometryInstances[k].GetInstanceTransform();
		AllGeometryInstances[k].Bounds = Collider->GetWorldBounds(
			[&](const FVector3d& P) { return Transform.TransformPosition(P); });
	});
}

void FGeometryCollisionScene::RecomputeSceneWorldBounds()
{
	// compute total world bounds, used to initialize octree
	SceneWorldBounds = FAxisAlignedBox3d::Empty();
	for (const FPlacedGeometryInstance& Instance : AllGeometryInstances)
		SceneWorldBounds.Contain(Instance.Bounds);
}

void FGeometryCollisionScene::RebuildSceneOctree()
{
	// build an octree of the instance bounding-boxes
	InstancesOctree = MakeShared<FSparseDynamicOctree3>();
	InstancesOctree->RootDimension = SceneWorldBounds.MaxDim() / 4.0;
	InstancesOctree->SetMaxTreeDepth(5);
	int32 TotalNumInstances = AllGeometryInstances.Num();
	for (int32 k = 0; k < TotalNumInstances; ++k) {
		InstancesOctree->InsertObject(k, AllGeometryInstances[k].Bounds);
	}
}




ResultOrFail<FGeometryCollisionScene::CollisionTestResult> FGeometryCollisionScene::TestCollisionWithOtherObjects(const AActor* TestActor, const FVector& Translation) const
{
	FGeometryCollisionScene::CollisionTestResult Result;
	Result.CollidingActor = nullptr;

	TArray<IGeometryCollider*> SourceColliders;
	TArray<FTransformSequence3d> SourceTransforms;

	bool bFound = false;
	FAxisAlignedBox3d TestActorBounds;
	for (const TUniquePtr<FGeometryActor>& Actor : SceneActors) {
		if (Actor->SourceActor == TestActor) {
			for (const auto& Instance : Actor->Instances) {
				const FUniqueGeometry* FoundGeo = this->FindGeometry(Instance.GeometryHandle);
				if (FoundGeo != nullptr) {
					SourceColliders.Add(this->Colliders[FoundGeo->GeometryIndex].Get());
					SourceTransforms.Add(Instance.WorldTransform);
				}
			}
			TestActorBounds = Actor->WorldBounds;
			bFound = true;
			break;
		}
	}
	int NumSources = SourceColliders.Num();
	if (!bFound || NumSources == 0) 
		return ResultOrFail<FGeometryCollisionScene::CollisionTestResult>();
	
	//FAxisAlignedBox3d TransformedBounds = FAxisAlignedBox3d::Empty();
	//for ( int i = 0; i < 8; ++i )
	//	TransformedBounds.Contain( NewTransform.TransformPosition( TestActorBounds.GetCorner(i) ) );
	FAxisAlignedBox3d TransformedBounds = TestActorBounds;
	TransformedBounds.Min += Translation; TransformedBounds.Max += Translation;
	for (FTransformSequence3d& seq : SourceTransforms)
		seq.Append(FTransform(Translation));

	TArray<int> OverlapIDs;
	InstancesOctree->RangeQuery(TransformedBounds, OverlapIDs);
	if (OverlapIDs.Num() == 0) 
		return ResultOrFail<FGeometryCollisionScene::CollisionTestResult>();

	TArray<bool> IsOverlapping;
	IsOverlapping.Init(false, OverlapIDs.Num());
	std::atomic<int> FoundOverlapIndexAtomic = -1;
	ParallelFor(OverlapIDs.Num(), [&](int k)
	{
		if (FoundOverlapIndexAtomic >= 0) return;		// early-out if we found any collision in another thread
		int id = OverlapIDs[k];
		const FPlacedGeometryInstance& InstanceInfo = AllGeometryInstances[id];
		if (InstanceInfo.Actor->SourceActor == TestActor)		// don't hit self
			return;
		if (InstanceInfo.Actor->WorldBounds.Intersects(TransformedBounds) == false)		// early-out on actor bounds
			return;

		for (int j = 0; j < NumSources; ++j)
		{
			const TUniquePtr<IGeometryCollider>& Collider = Colliders[InstanceInfo.ColliderIndex];
			
			bool bColliding = Collider->TestForCollision(*SourceColliders[j], SourceTransforms[j], InstanceInfo.GetInstanceTransform() );
			if (bColliding) {
				IsOverlapping[k] = true;
				FoundOverlapIndexAtomic = id;
				return;
			}
		}
	});
	if (FoundOverlapIndexAtomic >= 0) {
		const FPlacedGeometryInstance& InstanceInfo = AllGeometryInstances[FoundOverlapIndexAtomic];
		Result.CollidingActor = InstanceInfo.Actor->SourceActor;
		return Result;
	}
	return ResultOrFail<FGeometryCollisionScene::CollisionTestResult>();
}