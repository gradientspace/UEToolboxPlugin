// Copyright Gradientspace Corp. All Rights Reserved.
#include "GSJobSubsystem.h"
#include "Engine/Engine.h"
#include "Tasks/Task.h"
#include "Core/UEVersionCompat.h"

UGSJobSubsystem::UGSJobSubsystem()
{
	DefaultJobManager = CreateDefaultSubobject<UStandardGSJobManager>(TEXT("DefaultJobManager"), true);
	JobManagers.Add(NAME_None, DefaultJobManager);
}


uint64 UGSJobSubsystem::GetGlobalTickCounter()
{
	if (UGSJobSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGSJobSubsystem>())
		return Subsystem->GetGlobalTickCounter();
	return 0;
}


void UGSJobSubsystem::EnqueueStandardJob(
	UObject* Owner, UObject* Target,
	TFunction<bool()>&& ComputeFunc,
	TFunction<void()>&& GameThreadUpdateFunc,
	FJobOptions Options)
{
	if (UGSJobSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGSJobSubsystem>())
	{
		Subsystem->DefaultJobManager->EnqueueStandardJob(Owner, Target,
			MoveTemp(ComputeFunc), MoveTemp(GameThreadUpdateFunc), Options);
	}
	else
	{
		ensure(false);
	}
}


void UGSJobSubsystem::EnqueueGameThreadTickJob(
	UObject* Owner, UObject* Target,
	TFunction<void()>&& GameThreadFunc)
{
	if (UGSJobSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGSJobSubsystem>())
	{
		Subsystem->DefaultJobManager->EnqueueGameThreadTickJob_ThreadSafe(Owner, Target,
			MoveTemp(GameThreadFunc));
	}
	else
	{
		ensure(false);
	}
}


void UBaseGSJobManager::EnqueueStandardJob(
	UObject* Owner, UObject* Target,
	TFunction<bool()>&& ComputeFunc,
	TFunction<void()>&& GameThreadUpdateFunc,
	UGSJobSubsystem::FJobOptions Options)
{
	if (!ensure(IsValid(Owner) && IsValid(Target)))
		return;

	TimestampCounter++;

	UGSJobSubsystem::FJobIdentification NewID;
	NewID.JobOwner = Owner;
	NewID.JobTarget = Target;
	NewID.Timestamp = TimestampCounter;

	TSharedPtr<UGSJobSubsystem::FPendingJob> Job = MakeShared<UGSJobSubsystem::FPendingJob>();
	Job->ID = NewID;
	Job->ComputeFunc = MoveTemp(ComputeFunc);
	Job->GameThreadUpdateFunc = MoveTemp(GameThreadUpdateFunc);
	Job->Options = Options;

	PendingJobs.Add(Job);
}


void UBaseGSJobManager::EnqueueGameThreadTickJob_ThreadSafe(
	UObject* Owner, UObject* Target,
	TFunction<void()>&& GameThreadFunc)
{
	if (!ensure(IsValid(Owner) && IsValid(Target)))
		return;

	TimestampCounter++;

	UGSJobSubsystem::FJobIdentification NewID;
	NewID.JobOwner = Owner;
	NewID.JobTarget = Target;
	NewID.Timestamp = TimestampCounter;

	TSharedPtr<UGSJobSubsystem::FGameThreadTickJob> Job = MakeShared<UGSJobSubsystem::FGameThreadTickJob>();
	Job->ID = NewID;
	Job->GameThreadFunc = MoveTemp(GameThreadFunc);

	PendingTickJobsLock.Lock();
	PendingTickJobs.Add(Job);
	PendingTickJobsLock.Unlock();
}



void UBaseGSJobManager::ExecuteStandardJob(TSharedPtr<UGSJobSubsystem::FPendingJob>& Job)
{
	if (Job->ID.JobOwner.IsValid() == false || Job->ID.JobTarget.IsValid() == false )
	{
		return;
	}

	if (Job->Options.bComputeFuncIsThreadSafe)
	{
		UE::Tasks::FTask ComputeTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [Job, this]()
		{
			if (Job->ComputeFunc())
			{
				this->PendingGameThreadUpdatesLock.Lock();
				PendingGameThreadUpdates.Add(Job);
				this->PendingGameThreadUpdatesLock.Unlock();
			}
		});
	}
	else
	{
		if ( Job->ComputeFunc() )
			Job->GameThreadUpdateFunc();
	}
}


void UBaseGSJobManager::Tick(float DeltaTime)
{
	TimestampCounter++;
	TickCounter++;

	while (PendingJobs.Num() > 0)
	{
		TSharedPtr<UGSJobSubsystem::FPendingJob> Job = GSUE::TArrayPop(PendingJobs);
		ExecuteStandardJob(Job);
	}

	// run any pending gamethread updates
	TArray<TSharedPtr<UGSJobSubsystem::FPendingJob>> GameThreadUpdates;
	bool bHaveUpdates = false;
	PendingGameThreadUpdatesLock.Lock();
	if (PendingGameThreadUpdates.Num() > 0)
	{
		GameThreadUpdates = PendingGameThreadUpdates;
		bHaveUpdates = true;
		PendingGameThreadUpdates.Reset();
	}
	PendingGameThreadUpdatesLock.Unlock();

	if (bHaveUpdates)
	{
		while (GameThreadUpdates.Num() > 0)
		{
			TSharedPtr<UGSJobSubsystem::FPendingJob> Job = GSUE::TArrayPop(GameThreadUpdates);
			if (Job->ID.JobOwner.IsValid() && Job->ID.JobTarget.IsValid())
			{
				Job->GameThreadUpdateFunc();
			}
		}
	}



	TArray<TSharedPtr<UGSJobSubsystem::FGameThreadTickJob>> RunTickJobs;
	PendingTickJobsLock.Lock();
	if (PendingTickJobs.Num() > 0)
		::Swap(RunTickJobs, PendingTickJobs);
	PendingTickJobsLock.Unlock();

	for (auto Job : RunTickJobs) 
		Job->GameThreadFunc();

}

bool UBaseGSJobManager::IsTickable() const
{
	// todo need to also check PendingGameThreadUpdates but annoying to lock it, need some atomic?
	return true;
	//return (PendingJobs.Num() > 0);
}

TStatId UBaseGSJobManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBaseGSJobManager, STATGROUP_Tickables);
}
