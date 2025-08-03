// Copyright Gradientspace Corp. All Rights Reserved.
#include "GSGenerationSubsystem.h"
#include "Engine/Engine.h"

#include "GameFramework/Actor.h"
#include "GridActor/GeneratedModelGridActor.h"

bool UGSGenerationSubsystem::bIsGenerationSubsystemShuttingDown = false;


UGSGenerationSubsystem::UGSGenerationSubsystem()
{
}


void UGSGenerationSubsystem::Tick(float DeltaTime)
{
	// If we discover any invalid actors during the iteration, we want to remove them.
	// (otherwise they will generate log spam every frame)
	TArray<AGSGeneratedModelGridActor*, TInlineAllocator<8>> ToRemove;

	for (AGSGeneratedModelGridActor* Actor : ActiveGeneratedModelGrids)
	{
		bool bValid = Actor->IsValidLowLevel() && IsValid(Actor);
		if (bValid)
			Actor->ExecuteRegenerateIfPending();
		else
			ToRemove.Add(Actor);
	}

	ensureMsgf(ToRemove.Num() == 0, TEXT("[UGSGenerationSubsystem::Tick] Found invalid Actors during regeneration check..."));

	for (AGSGeneratedModelGridActor* InvalidActor : ToRemove)
		ActiveGeneratedModelGrids.Remove(InvalidActor);
}


void UGSGenerationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// could conditionally include editor in build.cs ?
	//GEditor->OnEditorClose().AddUObject(this, &UEditorGeometryGenerationSubsystem::OnShutdown);
	FCoreDelegates::OnEnginePreExit.AddUObject(this, &UGSGenerationSubsystem::OnShutdown);
}

void UGSGenerationSubsystem::Deinitialize()
{
	UGSGenerationSubsystem::bIsGenerationSubsystemShuttingDown = true;
	ActiveGeneratedModelGrids.Reset();

	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
}


void UGSGenerationSubsystem::OnShutdown()
{
	UGSGenerationSubsystem::bIsGenerationSubsystemShuttingDown = true;

	ActiveGeneratedModelGrids.Reset();
}



bool UGSGenerationSubsystem::RegisterGeneratedModelGrid(AGSGeneratedModelGridActor* Actor)
{
	if (UGSGenerationSubsystem::bIsGenerationSubsystemShuttingDown)
		return false;

	if (ActiveGeneratedModelGrids.Contains(Actor) == false) {
		ActiveGeneratedModelGrids.Add(Actor);
		return true;
	}
	return false;
}

bool UGSGenerationSubsystem::UnregisterGeneratedModelGrid(AGSGeneratedModelGridActor* Actor)
{
	if (UGSGenerationSubsystem::bIsGenerationSubsystemShuttingDown)
		return false;

	if (ActiveGeneratedModelGrids.Contains(Actor)) {
		ActiveGeneratedModelGrids.Remove(Actor);
		return true;
	}
	return false;
}


bool UGSGenerationSubsystem::RegisterGeneratedActor(AActor* Actor)
{
	if (UGSGenerationSubsystem::bIsGenerationSubsystemShuttingDown)
		return false;

	UGSGenerationSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGSGenerationSubsystem>();
	if (!Subsystem) return false;

	if (AGSGeneratedModelGridActor* ModelGridActor = Cast<AGSGeneratedModelGridActor>(Actor)) {
		Subsystem->RegisterGeneratedModelGrid(ModelGridActor);
		return true;
	}
	return false;
}

bool UGSGenerationSubsystem::UnregisterGeneratedActor(AActor* Actor)
{
	if (UGSGenerationSubsystem::bIsGenerationSubsystemShuttingDown)
		return false;

	UGSGenerationSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGSGenerationSubsystem>();
	if (!Subsystem) return false;

	if (AGSGeneratedModelGridActor* ModelGridActor = Cast<AGSGeneratedModelGridActor>(Actor)) {
		Subsystem->UnregisterGeneratedModelGrid(ModelGridActor);
		return true;
	}
	return false;
}



bool UGSGenerationSubsystem::IsTickable() const
{
	return !ActiveGeneratedModelGrids.IsEmpty();
}

TStatId UGSGenerationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSGenerationSubsystem, STATGROUP_Tickables);
}
