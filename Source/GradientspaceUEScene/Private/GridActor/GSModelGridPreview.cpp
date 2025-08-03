// Copyright Gradientspace Corp. All Rights Reserved.
#include "GridActor/GSModelGridPreview.h"

#include "Components/DynamicMeshComponent.h"

#include "Engine/World.h"
#include "Components/SphereComponent.h"

#include "Tasks/Task.h"
#include "Async/ParallelFor.h"

#include "MeshQueries.h"

using namespace UE::Geometry;
using namespace GS;

AGSModelGridPreviewActor::AGSModelGridPreviewActor()
{
#if WITH_EDITORONLY_DATA
	bListedInSceneOutliner = false;
#endif
}


void UGSModelGridPreview::Initialize(UObject* Parent, UWorld* TargetWorld)
{
	PreviewActor = TargetWorld->SpawnActor<AGSModelGridPreviewActor>(FActorSpawnParameters());
	USphereComponent* RootComponent = NewObject<USphereComponent>(PreviewActor);
	PreviewActor->SetRootComponent(RootComponent);
	RootComponent->RegisterComponent();
	RootComponent->SetVisibility(false);

	WorldTransform = FTransform();
	bMeshCollisionUpdatesPending = false;
}

void UGSModelGridPreview::Shutdown()
{
	if (PreviewActor) {
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
}

void UGSModelGridPreview::SetMaterials(const TArray<UMaterialInterface*>& Materials)
{
	ActiveMaterials = Materials;
	// update all components
}

void UGSModelGridPreview::SetWorldTransform(const FTransform& NewTransform)
{
	check(PreviewActor);
	WorldTransform = NewTransform;
	PreviewActor->SetActorTransform(NewTransform);
}




UDynamicMeshComponent* UGSModelGridPreview::SpawnNewComponent()
{
	UDynamicMeshComponent* Component = NewObject<UDynamicMeshComponent>(PreviewActor);
	Component->SetupAttachment(PreviewActor->GetRootComponent());
	Component->RegisterComponent();

	// todo need to incorporate centering transform
	//WorldTransform = UE::ToolTarget::GetLocalToWorldTransform(Target);
	//DynamicMeshComponent->SetWorldTransform((FTransform)WorldTransform);

	// todo: this just enables raytracing from UModelingComponentsSettings settings, need
	// to make it optional...
	//ToolSetupUtil::ApplyRenderingConfigurationToPreview(Component, nullptr);

	for (int i = 0; i < ActiveMaterials.Num(); ++i)
		Component->SetMaterial(i, ActiveMaterials[i]);

	//Component->SetColorOverrideMode(EDynamicMeshComponentColorOverrideMode::VertexColors);
	Component->SetVertexColorSpaceTransformMode(EDynamicMeshVertexColorTransformMode::NoTransform);

	VisibleComponents.Add(Component);

	return Component;
}


void UGSModelGridPreview::WaitForPendingCollisionUpdates()
{
	if (bMeshCollisionUpdatesPending) {
		PendingCollisionUpdateTask.Wait();
		check(bMeshCollisionUpdatesPending == false);
	}
}


void UGSModelGridPreview::ParallelUpdateColumns(
	const TArray<GS::Vector2i>& ColumnsToUpdate,
	TFunctionRef<void(GS::Vector2i, FDynamicMesh3& Mesh)> UpdateColumnMeshFunc)
{
	WaitForPendingCollisionUpdates();

	TArray<FMeshChunk*, TInlineAllocator<16>> ProcessChunks;

	for (GS::Vector2i Column : ColumnsToUpdate)
	{
		GS::Vector3i ChunkIndex(Column.X, Column.Y, 0);
		TSharedPtr<FMeshChunk>* Found = MeshChunks.Find(ChunkIndex);
		if (Found == nullptr)
		{
			TSharedPtr<FMeshChunk> NewChunk = MakeShared<FMeshChunk>();
			NewChunk->ChunkIndex = ChunkIndex;
			NewChunk->MeshComponent = SpawnNewComponent();

			Found = &MeshChunks.Add(ChunkIndex, NewChunk);
		}
		ProcessChunks.Add(Found->Get());
	}

	check(ProcessChunks.Num() == ColumnsToUpdate.Num());
	ParallelFor(ProcessChunks.Num(), [&](int Index)
	{
		FMeshChunk& Chunk = *ProcessChunks[Index];
		Vector2i Column(Chunk.ChunkIndex.X, Chunk.ChunkIndex.Y);

		Chunk.MeshLock.Lock();
		FDynamicMesh3* Mesh = Chunk.MeshComponent->GetMesh();
		UpdateColumnMeshFunc(Column, *Mesh);
		Chunk.bCollisionUpdatePending = true;
		Chunk.MeshLock.Unlock();
	});

	for (FMeshChunk* Chunk : ProcessChunks)
		Chunk->MeshComponent->NotifyMeshUpdated();

	//MeshCollisionUpdatesPending = true;
	//PendingCollisionUpdateTask = UE::Tasks::Launch(TEXT("UpdateModelGridCollision"), [this, ProcessChunks]()
	//{
	//	ParallelFor(ProcessChunks.Num(), [&](int Index)
	//	{
	//		FMeshChunk& Chunk = *ProcessChunks[Index];
	//		FDynamicMesh3* Mesh = Chunk.MeshComponent->GetMesh();
	//		Chunk.MeshCollision->Build();
	//	});

	//	MeshCollisionUpdatesPending = false;
	//});

}


void UGSModelGridPreview::BeginCollisionUpdate()
{
	WaitForPendingCollisionUpdates();
	bMeshCollisionUpdatesPending = true;

	PendingCollisionUpdateTask = UE::Tasks::Launch(TEXT("UpdateModelGridCollision"), [this]()
	{
		TArray<FMeshChunk*, TInlineAllocator<16>> Pending;
		for (auto& Pair : MeshChunks) {
			if (Pair.Value->bCollisionUpdatePending)
				Pending.Add(Pair.Value.Get());
		}

		ParallelFor(Pending.Num(), [&](int Index)
		{
			FMeshChunk& Chunk = *Pending[Index];

			// copy mesh and rebuild BVH
			Chunk.MeshLock.Lock();

			if ( Chunk.CollisionBVH.GetMesh() == nullptr )
				Chunk.CollisionBVH.SetMesh(&Chunk.CollisionMesh, false);

			Chunk.CollisionMesh.Copy(*Chunk.MeshComponent->GetMesh(), false, false, false, false);
			Chunk.bCollisionUpdatePending = false;
			Chunk.MeshLock.Unlock();

			Chunk.CollisionBVH.Build();
		});

		// done collision updates
		bMeshCollisionUpdatesPending = false;
	});




}


bool UGSModelGridPreview::FindRayIntersection(const FRay3d& WorldRay, FHitResult& HitOut)
{
	WaitForPendingCollisionUpdates();

	FRay3d LocalRay = WorldTransform.InverseTransformRay(WorldRay);

	// todo could improve this a lot - DDA, test chunk bounds, etc

	double NearHitDist = TNumericLimits<float>::Max();
	int NearHitTID = -1;
	FMeshChunk* NearHitChunk = nullptr;
	for (auto& Pair : MeshChunks)
	{
		FMeshChunk& Chunk = *Pair.Value;
		if (! Chunk.CollisionBVH.IsValid(false)) 
			continue;

		double NearT; int HitTID;
		if (Chunk.CollisionBVH.FindNearestHitTriangle(LocalRay, NearT, HitTID))
		{
			if (NearT < NearHitDist) {
				NearHitDist = NearT;
				NearHitTID = HitTID;
				NearHitChunk = &Chunk;
			}
		}
	}
	if (NearHitChunk == nullptr)
		return false;

	FTriangle3d Triangle;
	NearHitChunk->CollisionMesh.GetTriVertices(NearHitTID, Triangle.V[0], Triangle.V[1], Triangle.V[2]);
	FIntrRay3Triangle3d Query(LocalRay, Triangle);
	Query.Find();
	
	HitOut.FaceIndex = NearHitTID;
	HitOut.Distance = Query.RayParameter;
	HitOut.Normal = WorldTransform.TransformNormal(Triangle.Normal());
	HitOut.Location = WorldTransform.TransformPosition(LocalRay.PointAt(Query.RayParameter));
	HitOut.ImpactNormal = HitOut.Normal;
	HitOut.ImpactPoint = HitOut.Location;
	
	return true;
}


AActor* UGSModelGridPreview::GetPreviewActor() const
{
	return PreviewActor;
}
