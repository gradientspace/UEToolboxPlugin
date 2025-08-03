// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "Tickable.h"

#include "GSGenerationSubsystem.generated.h"

class AGSGeneratedModelGridActor;


/**
 * GSGenerationSubsystem handles regeneration of procedural actors, currently only GeneratedModelGridActor.
 * 
 * TODO: refactor into Subsystem and manager object, like GSJobSubsystem. This will allow
 *    alternate Managers to be dynamically swapped in, eg so different Tick() policies can be 
 *    added by clients
 */
UCLASS()
class GRADIENTSPACEUESCENE_API UGSGenerationSubsystem : public UEngineSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
protected:


public:
	UGSGenerationSubsystem();

	// UEngineSubsystem API
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
protected:
	virtual void OnShutdown();
	static bool bIsGenerationSubsystemShuttingDown;



public:
	virtual bool RegisterGeneratedModelGrid(AGSGeneratedModelGridActor* Actor);
	virtual bool UnregisterGeneratedModelGrid(AGSGeneratedModelGridActor* Actor);

	static bool RegisterGeneratedActor(AActor* Actor);
	static bool UnregisterGeneratedActor(AActor* Actor);

protected:
	TSet<AGSGeneratedModelGridActor*> ActiveGeneratedModelGrids;



public:
	// Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickableInEditor() const override { return true; }
	// End FTickableGameObject interface
};


