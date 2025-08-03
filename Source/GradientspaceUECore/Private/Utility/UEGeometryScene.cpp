// Copyright Gradientspace Corp. All Rights Reserved.

#include "Utility/UEGeometryScene.h"


#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"

#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"

using namespace GS;
using namespace UE::Geometry;

void FGeometryScene::CollectComponentMeshInstances_StaticMesh(UStaticMeshComponent* StaticMeshComponent, FGeometryActor& Actor)
{
	UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh();
	if (Mesh == nullptr)
		return;

	FGeometryInstance MeshInstance;
	MeshInstance.SourceComponent = StaticMeshComponent;
	MeshInstance.GeometryHandle = FGeometryHandle(Mesh);

	UInstancedStaticMeshComponent* ISMComponent = Cast<UInstancedStaticMeshComponent>(StaticMeshComponent);
	if (ISMComponent == nullptr) {
		// this is a base StaticMeshComponent...right?
		MeshInstance.ComponentType = EGeometryComponentType::StaticMesh;
		MeshInstance.WorldTransform.Append(StaticMeshComponent->GetComponentTransform());
		Actor.Instances.Add(MeshInstance);
		return;
	}

	if ( Cast<UHierarchicalInstancedStaticMeshComponent>(ISMComponent) != nullptr )
		MeshInstance.ComponentType = EGeometryComponentType::HierarchicalInstancedStaticMesh;
	else
		MeshInstance.ComponentType = EGeometryComponentType::InstancedStaticMesh;

	int32 NumInstances = ISMComponent->GetInstanceCount();
	for (int32 i = 0; i < NumInstances; ++i)
	{
		if (!ISMComponent->IsValidInstance(i)) continue;
		FTransform InstanceTransform;
		if (!ISMComponent->GetInstanceTransform(i, InstanceTransform, /*bWorldSpace=*/true)) continue;

		FGeometryInstance InstanceInstance = MeshInstance;
		InstanceInstance.InstanceIndex = i;
		InstanceInstance.WorldTransform.Append(InstanceTransform);
		Actor.Instances.Add(InstanceInstance);
	}
}
void FGeometryScene::CollectComponentMeshInstances_DynamicMesh(UDynamicMeshComponent* DynamicMeshComponent, FGeometryActor& Actor)
{
	UDynamicMesh* Mesh = DynamicMeshComponent->GetDynamicMesh();
	if (Mesh == nullptr) return;

	FGeometryInstance MeshInstance;
	MeshInstance.SourceComponent = DynamicMeshComponent;
	MeshInstance.GeometryHandle = FGeometryHandle(Mesh);
	MeshInstance.ComponentType = EGeometryComponentType::DynamicMesh;
	MeshInstance.WorldTransform.Append(DynamicMeshComponent->GetComponentTransform());
	Actor.Instances.Add(MeshInstance);
}
void FGeometryScene::CollectComponentMeshInstances_Other(UActorComponent* Component, FGeometryActor& Actor)
{
	// this is for subclasses
}
void FGeometryScene::CollectComponentMeshInstances(UActorComponent* Component, FGeometryActor& Actor)
{
	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component)) {
		CollectComponentMeshInstances_StaticMesh(StaticMeshComponent, Actor);
		return;
	}
	if ( UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component)) {
		CollectComponentMeshInstances_DynamicMesh(DynamicMeshComponent, Actor);
		return;
	}
	CollectComponentMeshInstances_Other(Component, Actor);
}



void FGeometryScene::AddActors(const TArray<AActor*>& Actors)
{
	TArray<AActor*> ChildActors;
	TArray<FGeometryActor*> NewActors;
	for (AActor* Actor : Actors)
	{
		TUniquePtr<FGeometryActor> GeoActor = MakeUnique<FGeometryActor>();
		GeoActor->SourceActor = Actor;

		for (UActorComponent* Component : Actor->GetComponents())
			CollectComponentMeshInstances(Component, *GeoActor);

		ChildActors.Reset();
		Actor->GetAllChildActors(ChildActors, true);
		for (AActor* ChildActor : ChildActors)
		{
			for (UActorComponent* Component : ChildActor->GetComponents())
				CollectComponentMeshInstances(Component, *GeoActor);
		}

		// ensure that geometry is created for each instance
		for (const FGeometryInstance& Instance : GeoActor->Instances)
			FindOrAddGeometry(Instance.GeometryHandle);

		// compute initial bounds here...
		FVector Origin, Extent;
		Actor->GetActorBounds(false, Origin, Extent, true);
		GeoActor->WorldBounds = FAxisAlignedBox3d(Origin - Extent, Origin + Extent);

		NewActors.Add(GeoActor.Get());
		SceneActors.Add(MoveTemp(GeoActor));
	}

}





void FGeometryScene::UpdateAllTransforms()
{
	for (TUniquePtr<FGeometryActor>& Actor : SceneActors)
		UpdateActorInstanceTransforms(*Actor);
}



void FGeometryScene::UpdateActorInstanceTransforms(FGeometryActor& Actor)
{
	for (FGeometryInstance& Instance : Actor.Instances)
	{
		Instance.WorldTransform = FTransformSequence3d();

		if ((Instance.ComponentType == EGeometryComponentType::InstancedStaticMesh) ||
			(Instance.ComponentType == EGeometryComponentType::HierarchicalInstancedStaticMesh))
		{
			UInstancedStaticMeshComponent* ISMComponent = Cast<UInstancedStaticMeshComponent>(Instance.SourceComponent);
			if (ISMComponent->IsValidInstance(Instance.InstanceIndex))
			{
				FTransform InstanceTransform;
				ISMComponent->GetInstanceTransform(Instance.InstanceIndex, InstanceTransform, true);
				Instance.WorldTransform.Append(InstanceTransform);
			}
		}
		else if (Instance.ComponentType == EGeometryComponentType::StaticMesh)
		{
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Instance.SourceComponent);
			Instance.WorldTransform.Append(StaticMeshComponent->GetComponentTransform());
		}
		else if (Instance.ComponentType == EGeometryComponentType::DynamicMesh)
		{
			UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Instance.SourceComponent);
			Instance.WorldTransform.Append(DynamicMeshComponent->GetComponentTransform());
		} 
		else
		{
			OnUpdateInstanceTransform(Actor, Instance);
		}
	}

	// recompute Actor bounds...  (maybe should allow subclass to override this, as eg collision scene can do it w/ it's own bounds...)
	FVector Origin, Extent;
	Actor.SourceActor->GetActorBounds(false, Origin, Extent, true);
	Actor.WorldBounds = FAxisAlignedBox3d(Origin - Extent, Origin + Extent);
}






FGeometryScene::FUniqueGeometry* FGeometryScene::FindOrAddGeometry(FGeometryHandle Handle)
{
	TSharedPtr<FUniqueGeometry>* Found = UniqueGeometries.Find(Handle);
	if (Found == nullptr)
	{
		TSharedPtr<FUniqueGeometry> NewGeo = MakeShared<FUniqueGeometry>();
		NewGeo->GeometryHandle = Handle;
		NewGeo->GeometryIndex = UniqueGeometryCounter++;
		UniqueGeometries.Add(Handle, NewGeo);
		OnUniqueGeometryAdded(NewGeo.Get());
		return NewGeo.Get();
	}
	return Found->Get();
}
const FGeometryScene::FUniqueGeometry* FGeometryScene::FindGeometry(FGeometryHandle Handle) const
{
	const TSharedPtr<FUniqueGeometry>* Found = UniqueGeometries.Find(Handle);
	return (Found != nullptr) ? Found->Get() : nullptr;
}


