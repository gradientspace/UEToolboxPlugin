// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "BoxTypes.h"
#include "TransformSequence.h"
#include "Templates/TypeHash.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"

class AActor;
class UActorComponent;
class UDynamicMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UDynamicMesh;

namespace GS
{


enum class ESceneGeometryType
{
	StaticMeshAsset,
	UDynamicMeshObject,		// could this be UDynamicMesh?
	Unknown
};


struct GRADIENTSPACEUECORE_API FGeometryHandle
{
	ESceneGeometryType GeometryType = ESceneGeometryType::Unknown;
	void* RawDataPointer = nullptr;		// raw pointer to owning Geometry object (must exist and be stable for lifetime of GeometryScene/etc)
	uint64_t DataSubID = 0;				// ID in that Geometry object, optionally used to refer to sub-geometry objects

	FGeometryHandle() {}
	FGeometryHandle(UStaticMesh* StaticMesh) {
		GeometryType = ESceneGeometryType::StaticMeshAsset;
		RawDataPointer = (void*)StaticMesh;
	}
	FGeometryHandle(UDynamicMesh* DynamicMesh) {
		GeometryType = ESceneGeometryType::UDynamicMeshObject;
		RawDataPointer = (void*)DynamicMesh;
	}

	UStaticMesh* GetStaticMesh() const {
		if (ensure(GeometryType == ESceneGeometryType::StaticMeshAsset))
			return (UStaticMesh*)RawDataPointer;
		return nullptr;
	}

	UDynamicMesh* GetDynamicMesh() const {
		if (ensure(GeometryType == ESceneGeometryType::UDynamicMeshObject))
			return (UDynamicMesh*)RawDataPointer;
		return nullptr;
	}

	bool operator==(const FGeometryHandle& other) const {
		return GeometryType == other.GeometryType && RawDataPointer == other.RawDataPointer && DataSubID == other.DataSubID;
	}
};
inline uint32 GetTypeHash(const FGeometryHandle& Handle) {
	return ::HashCombineFast(PointerHash(Handle.RawDataPointer), ::HashCombineFast(::GetTypeHash(Handle.DataSubID), (uint32)Handle.GeometryType));
}



enum class EGeometryComponentType
{
	StaticMesh,
	InstancedStaticMesh,
	HierarchicalInstancedStaticMesh,
	DynamicMesh,

	Unknown
};



struct GRADIENTSPACEUECORE_API FGeometryInstance
{
public:
	UActorComponent* SourceComponent = nullptr;
	EGeometryComponentType ComponentType = EGeometryComponentType::Unknown;
	int32 InstanceIndex = 0;		// index of this instance in SourceComponent

	UE::Geometry::FTransformSequence3d WorldTransform;		// local-to-world transform stack
															// (maybe better to just use an inline array here?)

	FGeometryHandle GeometryHandle;
};


struct GRADIENTSPACEUECORE_API FGeometryActor
{
public:
	AActor* SourceActor = nullptr;
	UE::Geometry::FAxisAlignedBox3d WorldBounds;		// world-space bounds of the Geometry of this Actor

	TArray<FGeometryInstance> Instances;	// set of geometry instances owned by this Actor   (why a pointer??)
};


/**
 * FGeometryScene is intended to represent multiple views of an Actor/Component/Geometry hierarchy,
 * in particular to allow other code to be able to take advantange of Geometry instancing, ie where
 * the same Geometry (eg StaticMesh) is used by many different Components/Instances inside different Actors.
 * 
 * The base class just builds the relationship between Actors->Instances->Geometries, 
 * and tracks unique Geometries. Subclasses can override this class to (eg) build
 * additional data structures for each unique Geometry.
 *
 */
class GRADIENTSPACEUECORE_API FGeometryScene
{
public:
	virtual ~FGeometryScene() {}
	FGeometryScene() {}
	UE_NONCOPYABLE(FGeometryScene);

	// include Actors and child Instances in SceneActors list
	virtual void AddActors(const TArray<AActor*>& Actors);

	// update each Actor->Instance.WorldTransform. Subclasses can override to rebuild things after those updates.
	virtual void UpdateAllTransforms();



protected:
	TArray<TUniquePtr<FGeometryActor>> SceneActors;

	virtual void CollectComponentMeshInstances(UActorComponent* Component, FGeometryActor& Actor);
	virtual void CollectComponentMeshInstances_DynamicMesh(UDynamicMeshComponent* DynamicMeshComponent, FGeometryActor& Actor);
	virtual void CollectComponentMeshInstances_StaticMesh(UStaticMeshComponent* StaticMeshComponent, FGeometryActor& Actor);
	virtual void CollectComponentMeshInstances_Other(UActorComponent* Component, FGeometryActor& Actor);

	virtual void UpdateActorInstanceTransforms(FGeometryActor& Actor);

	struct FUniqueGeometry
	{
		FGeometryHandle GeometryHandle;
		int GeometryIndex = -1;				// this index can be used by subclasses to build additional data for each UniqueGeometry
	};

	TMap<FGeometryHandle, TSharedPtr<FUniqueGeometry>> UniqueGeometries;
	std::atomic<int> UniqueGeometryCounter;
	const FUniqueGeometry* FindGeometry(FGeometryHandle Handle) const;
	FUniqueGeometry* FindOrAddGeometry(FGeometryHandle Handle);


protected:
	// these are callback-type functions that child classes can override to extend the base class

	// this will be called each time a new FUniqueGeometry is discovered, subclasses can use it to create additional data
	virtual void OnUniqueGeometryAdded(FUniqueGeometry* Geometry) {}

	// this is called to update instance transforms for unknown Component types in UpdateActorInstanceTransforms
	virtual void OnUpdateInstanceTransform(FGeometryActor& Actor, FGeometryInstance& Instance) {}
};





} // end namespace GS
