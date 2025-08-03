// Copyright Gradientspace Corp. All Rights Reserved.
#include "GSTemporarySubsystem.h"
#include "Misc/CoreDelegates.h"
#include "Core/UEVersionCompat.h"

#include "UDynamicMesh.h"
#include "GradientspaceUELogging.h"


UGSTemporarySubsystem::UGSTemporarySubsystem()
{
}

void UGSTemporarySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	FCoreDelegates::OnEndFrame.AddUObject(this, &UGSTemporarySubsystem::OnEndFrame);
}

void UGSTemporarySubsystem::Deinitialize()
{
	FCoreDelegates::OnEndFrame.RemoveAll(this);
}


UDynamicMesh* UGSTemporarySubsystem::GetOrAllocateOneFrameMesh()
{
	UDynamicMesh* UseMesh = nullptr;
	if (AvailableMeshes.Num() == 0) {
		UseMesh = AllocateNewMesh();
		AllAllocatedMeshes.Add(UseMesh);
	}
	else {
		UseMesh = GSUE::TArrayPop(AvailableMeshes);
	}

	PendingOneFrameMeshes.Add(UseMesh);
	return UseMesh;
}


void UGSTemporarySubsystem::ReturnMesh(UDynamicMesh* Mesh)
{
	if (AllAllocatedMeshes.Contains(Mesh) == false) {
		UE_LOG(LogGradientspace, Warning, TEXT("[GSTemporarySubsystem::ReturnMesh] Returned Mesh was not created by TemporarySubsystem...ignoring"));
		return;
	}
	if (AvailableMeshes.Contains(Mesh) == false) {
		UE_LOG(LogGradientspace, Warning, TEXT("[GSTemporarySubsystem::ReturnMesh] Returned Mesh was already marked as Available...possibly mismatched request/return"));
		return;
	}

	PendingOneFrameMeshes.Remove(Mesh);		// in case it was pending
	AvailableMeshes.AddUnique(Mesh);
}



void UGSTemporarySubsystem::Tick(float DeltaTime)
{
	// there is a function IsReferenced(Object) which could be used for debugging here...

	//UE_LOG(LogTemp, Warning, TEXT("TEMPORARY SUBSYSTEM - ALLOCATED %d AVAILABLE %d ONEFRAME %d"),
	//	AllAllocatedMeshes.Num(), AvailableMeshes.Num(), PendingOneFrameMeshes.Num());

	ProcessSingleFrameReturns();

	OnEndFrameWithoutTickCounter = 0;
}
void UGSTemporarySubsystem::OnEndFrame()
{
	// During certain operations, eg for example drag-and-drop from content browser, the Tick above will be disabled.
	// In that situation we will accumulate allocated meshes until the Tick runs.
	// To avoid it, we also listen to end-of-frame events, and if enough frames pass, we run a return pass
	OnEndFrameWithoutTickCounter++;
	if (OnEndFrameWithoutTickCounter > 10)
	{
		ProcessSingleFrameReturns();
		OnEndFrameWithoutTickCounter = 0;
	}
}

void UGSTemporarySubsystem::ProcessSingleFrameReturns()
{
	// return any temporary meshes requested this frame
	for (UDynamicMesh* Mesh : PendingOneFrameMeshes) {
		Mesh->Reset();
		AvailableMeshes.AddUnique(Mesh);
	}
	PendingOneFrameMeshes.Reset();

	// emergency leak detector...
	if (AllAllocatedMeshes.Num() > 4096) {
		UE_LOG(LogGradientspace, Warning, TEXT("[GSTemporarySubsystem] Over 4096 meshes have been allocated, there is probably a leak. Releasing references and garbage-collecting..."));
		AllAllocatedMeshes.Reset();
		AvailableMeshes.Reset();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
}


UDynamicMesh* UGSTemporarySubsystem::AllocateNewMesh()
{
	UDynamicMesh* Mesh = NewObject<UDynamicMesh>(this);
	return Mesh;
}


bool UGSTemporarySubsystem::IsTickable() const
{
	return PendingOneFrameMeshes.Num() > 0 || AvailableMeshes.Num() > 0;
}

TStatId UGSTemporarySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTemporarySubsystem, STATGROUP_Tickables);
}


