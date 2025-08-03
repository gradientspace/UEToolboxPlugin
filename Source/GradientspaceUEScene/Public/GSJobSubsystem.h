// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "Tickable.h"

#include "GSJobSubsystem.generated.h"



UCLASS()
class GRADIENTSPACEUESCENE_API UGSJobSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<UBaseGSJobManager> DefaultJobManager;

	UPROPERTY()
	TMap<FName, TObjectPtr<UBaseGSJobManager>> JobManagers;

public:
	UGSJobSubsystem();

	//! this counter increments once per tick of the engine subsystem. The subsystem
	//! ticks with the game thread. So this can be used to identify things happening
	//! on the same game-thread-tick (eg to avoid sending duplicate jobs/etc)
	static uint64 GetGlobalTickCounter();


	struct FJobIdentification
	{
		TWeakObjectPtr<UObject> JobOwner;
		TWeakObjectPtr<UObject> JobTarget;
		uint64 Timestamp = 0;
	};

	struct FJobOptions
	{
		bool bComputeFuncIsThreadSafe = false;
		
		static FJobOptions ThreadSafe() { FJobOptions Tmp; Tmp.bComputeFuncIsThreadSafe = true; return Tmp; }
	};

	struct FPendingJob
	{
		FJobIdentification ID;

		TFunction<bool()> ComputeFunc;
		TFunction<void()> GameThreadUpdateFunc;

		FJobOptions Options;
	};

	static void EnqueueStandardJob(
		UObject* Owner, UObject* Target,
		TFunction<bool()>&& ComputeFunc,
		TFunction<void()>&& GameThreadUpdateFunc,
		FJobOptions Options = FJobOptions());



	struct FGameThreadTickJob
	{
		FJobIdentification ID;
		TFunction<void()> GameThreadFunc;
	};

	static void EnqueueGameThreadTickJob(
		UObject* Owner, UObject* Target,
		TFunction<void()>&& GameThreadFunc);
};



// the idea here is that there could be 'EnqueueJobAdvanced' in UGSJobSubsystem/etc, that 
// can take a FPendingJob subclass (or something?) and pass to UBaseGSJobManager subclasses
// that might handle scheduling/compute/etc differently...
UCLASS(Abstract)
class GRADIENTSPACEUESCENE_API UBaseGSJobManager : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
public:

	virtual void EnqueueStandardJob(
		UObject* Owner, UObject* Target, 
		TFunction<bool()>&& ComputeFunc,
		TFunction<void()>&& GameThreadUpdateFunc,
		UGSJobSubsystem::FJobOptions Options);

	virtual void EnqueueGameThreadTickJob_ThreadSafe(
		UObject* Owner, UObject* Target,
		TFunction<void()>&& GameThreadFunc);

	uint64 GetGlobalTickCounter() const { return TickCounter; }

public:
	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickableInEditor() const override { return true; }
	//~ End FTickableGameObject interface


protected:
	TArray<TSharedPtr<UGSJobSubsystem::FPendingJob>> PendingJobs;

	std::atomic<uint64> TickCounter = 0;

	// not clear what this counter is for, exactly...increments on tick but also whenever a job is enqueued?
	std::atomic<uint64> TimestampCounter = 0;

	void ExecuteStandardJob(TSharedPtr<UGSJobSubsystem::FPendingJob>& Job);

	TArray<TSharedPtr<UGSJobSubsystem::FPendingJob>> PendingGameThreadUpdates;
	FCriticalSection PendingGameThreadUpdatesLock;


	TArray<TSharedPtr<UGSJobSubsystem::FGameThreadTickJob>> PendingTickJobs;
	FCriticalSection PendingTickJobsLock;
};



UCLASS()
class GRADIENTSPACEUESCENE_API UStandardGSJobManager : public UBaseGSJobManager
{
	GENERATED_BODY()

public:
	//virtual void Shutdown();
public:


};
