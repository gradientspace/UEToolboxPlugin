// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "GameFramework/Actor.h"
#include "Tasks/Task.h"
#include "Engine/HitResult.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "TransformTypes.h"

#include "Math/GSIntVector2.h"
#include "Math/GSIntVector3.h"


#include "GSModelGridPreview.generated.h"



class UDynamicMeshComponent;
class UWorld;
class AActor;
class UMaterialInterface;


UCLASS(Transient)
class GRADIENTSPACEUESCENE_API UGSModelGridPreview : public UObject
{
	GENERATED_BODY()
public:

	void Initialize(UObject* Parent, UWorld* TargetWorld);
	void Shutdown();

	void SetMaterials(const TArray<UMaterialInterface*>& Materials);

	void SetWorldTransform(const FTransform& NewTransform);

	void ParallelUpdateColumns(
		const TArray<GS::Vector2i>& ColumnsToUpdate,
		TFunctionRef<void(GS::Vector2i Column, UE::Geometry::FDynamicMesh3& Mesh)> UpdateColumnMeshFunc);

	void BeginCollisionUpdate();

	bool FindRayIntersection(const FRay3d& WorldRay, FHitResult& HitOut);

	AActor* GetPreviewActor() const;
	//! useful for filtering hit queries
	const TArray<const UPrimitiveComponent*>& GetVisibleComponents() const { return VisibleComponents; }

protected:
	UPROPERTY()
	TObjectPtr<AActor> PreviewActor;

	UE::Geometry::FTransformSRT3d WorldTransform;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> ActiveMaterials;

	struct FMeshChunk
	{
		GS::Vector3i ChunkIndex;
		TWeakObjectPtr<UDynamicMeshComponent> MeshComponent;
		FCriticalSection MeshLock;

		// does it make sense to use FDynamicMesh3 here? could copy into something else...
		bool bCollisionUpdatePending;
		UE::Geometry::FDynamicMesh3 CollisionMesh;
		UE::Geometry::FDynamicMeshAABBTree3 CollisionBVH;
	};
	TMap<GS::Vector3i, TSharedPtr<FMeshChunk>> MeshChunks;
	TArray<const UPrimitiveComponent*> VisibleComponents;

	std::atomic<bool> bMeshCollisionUpdatesPending;
	UE::Tasks::FTask PendingCollisionUpdateTask;
	void WaitForPendingCollisionUpdates();

	UDynamicMeshComponent* SpawnNewComponent();

};


// Just an empty parent actor we can attach things to, that will never appear elsewhere.
// Maybe should be generalized
UCLASS(Transient, NotPlaceable, Hidden, NotBlueprintable, NotBlueprintType, MinimalAPI)
class AGSModelGridPreviewActor : public AActor
{
	GENERATED_BODY()
private:
	AGSModelGridPreviewActor();
public:

#if WITH_EDITOR
	virtual bool IsSelectable() const override { return false; }
#endif
};
