// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "Tickable.h"

#include "GSTemporarySubsystem.generated.h"

class UDynamicMesh;


/**
 * UGSTemporarySubsystem is intended to support allocation/reuse of temporary UObjects in Blueprints
 */
UCLASS()
class GRADIENTSPACEUESCENE_API UGSTemporarySubsystem : public UEngineSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UGSTemporarySubsystem();

	// UEngineSubsystem API
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;


public:
	UDynamicMesh* GetOrAllocateOneFrameMesh();

	void ReturnMesh(UDynamicMesh* Mesh);


protected:
	UPROPERTY()
	TSet<TObjectPtr<UDynamicMesh>> AllAllocatedMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UDynamicMesh>> AvailableMeshes;

	// TODO: we actually would like to perhaps store an array-of-arrays here? so we
	// know which frame. Then we can only release up to frame N-1 on frame N+1...
	UPROPERTY()
	TArray<TObjectPtr<UDynamicMesh>> PendingOneFrameMeshes;
	
	void ResetMesh(UDynamicMesh* Mesh);
	UDynamicMesh* AllocateNewMesh();

	void OnEndFrame();
	uint32 OnEndFrameWithoutTickCounter = 0;

	void ProcessSingleFrameReturns();


public:
	// Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickableInEditor() const override { return true; }
	// End FTickableGameObject interface

};