// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Utility/UEGeometryScene.h"
#include "Core/GSResult.h"

namespace UE::Geometry { class FSparseDynamicOctree3; }

namespace GS
{


struct FGeometryCollisionSceneBuildOptions
{
	// no functional options yet...maybe get rid of this
};

struct GRADIENTSPACEUECORE_API FGeometryCollisionSceneRayHit
{
	FRay3d WorldRay;
	double RayDistance = -1.0;
	FVector3d LocalHitPos;
	FVector3d WorldHitPos;

	AActor* Actor = nullptr;
	UActorComponent* Component = nullptr;
	int32 ComponentInstanceIndex = -1;		// eg ISM Instance Index/ID
	int32 GeometryIndex = -1;				// eg Triangle Index/ID

	FVector3d HitBaryCoords = FVector3d::Zero();	// barycentric coords of hit point if available

	FGeometryHandle HitGeometryHandle;		// handle to hit geometry

	UE::Geometry::FTransformSequence3d LocalToWorld;	// LocalToWorld Transform to hit Geometry
};



// IGeometryCollider is a container/wrapper for some geometry object (ie so far always a Mesh)
class GRADIENTSPACEUECORE_API IGeometryCollider
{
public:
	UE_NONCOPYABLE(IGeometryCollider);
	IGeometryCollider() = default;
	virtual ~IGeometryCollider() {}

	FGeometryHandle SourceHandle;

	// build expensive data structures for this Collider. Must be thread-safe.
	virtual bool Build(const FGeometryCollisionSceneBuildOptions& BuildOptions) = 0;

	virtual int32 GetTriangleCount() const = 0;

	// compute world bounds. Must be thread-safe.
	virtual UE::Geometry::FAxisAlignedBox3d GetWorldBounds(TFunctionRef<FVector3d(const FVector3d&)> LocalToWorldFunc) = 0;

	// find the nearest ray-intersection. Must be thread-safe.
	virtual bool RayIntersection(const FRay3d& WorldRay, 
		const UE::Geometry::FTransformSequence3d& LocalToWorldTransform, 
		FGeometryCollisionSceneRayHit& WorldHitResultOut) = 0;

	// test for object/object intersection . Must be thread-safe.
	virtual bool TestForCollision( const IGeometryCollider& OtherCollider,
		const UE::Geometry::FTransformSequence3d& OtherTreeLocalToWorld,
		const UE::Geometry::FTransformSequence3d& LocalToWorldTransform) = 0;
};





/**
 * GeometryCollisionScene extends the base GeometryScene with:
 *   1) a packed mesh (copy) & AABBTree of each unique Geometry   (only tris & verts, currently)
 *   2) a cache of "placed" instances, w/ world-space bounds for each instance
 * 	 3) an Octree of placed instances (ie their bounding-boxes)
 * 
 * 	 Queries/etc:
 *   1) Collision query between one of the existing scene Actors w/ a new Transform, and the rest of the Actors
 */
class GRADIENTSPACEUECORE_API FGeometryCollisionScene : public FGeometryScene
{
public:
	virtual void Initialize(FGeometryCollisionSceneBuildOptions BuildOptions);

	// call after GeometryScene::AddActors to build any pending Colliders, and then fully rebuild instance bounds/octree
	virtual void UpdateBuild();

	// updates all instance transforms and bounds, and then rebuild octree
	virtual void UpdateAllTransforms() override;


	// todo!
	//virtual ResultOrFail<FGeometryCollisionSceneRayHit> FindNearestRayIntersection(const FRay3d& WorldRay) const;


	struct CollisionTestResult
	{
		AActor* CollidingActor = nullptr;
	};
	virtual ResultOrFail<CollisionTestResult> TestCollisionWithOtherObjects(const AActor* TestActor, const FVector& Translation) const;


protected:
	// overrides from FGeometryScene
	virtual void OnUniqueGeometryAdded(FUniqueGeometry* Geometry) override;



protected:
	FGeometryCollisionSceneBuildOptions BuildOptions;

	TArray<TUniquePtr<IGeometryCollider>> Colliders;
	TArray<int> PendingColliderBuilds;

	struct FPlacedGeometryInstance
	{
		FGeometryActor* Actor;
		int InstanceIndex;		// index into Actor->Instances list

		int ColliderIndex;		// index into this->Colliders
		UE::Geometry::FAxisAlignedBox3d Bounds;

		const UE::Geometry::FTransformSequence3d& GetInstanceTransform() const {
			return Actor->Instances[InstanceIndex].WorldTransform;
		}
	};
	TArray<FPlacedGeometryInstance> AllGeometryInstances;
	UE::Geometry::FAxisAlignedBox3d SceneWorldBounds;
	TSharedPtr<UE::Geometry::FSparseDynamicOctree3> InstancesOctree;

	void InitializeAndBuildGeometryInstances();
	void RecomputeAllGeometryInstanceBounds();		// recalc bounds for each AllGeometryInstances 
	void RecomputeSceneWorldBounds();				// recalc SceneWorldBounds
	void RebuildSceneOctree();						// reallocate and rebuild InstancesOctree
};



}  // end namespace GS