// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Async/ParallelFor.h"
#include "Tasks/Task.h"

#include "Core/gs_parallel_api.h"


namespace GS
{

class UETaskWrapper : public IExternalTaskWrapper
{
public:
	UE::Tasks::FTask Task;

	UETaskWrapper(UE::Tasks::FTask task) : Task(task) {}
};


class GRADIENTSPACEUECORE_API UEParallel_Impl : public GS::Parallel::gs_parallel_api
{
public:
	virtual ~UEParallel_Impl() {}

	virtual void parallel_for_jobcount(
		uint32_t NumJobs,
		FunctionRef<void(uint32_t JobIndex)> JobFunction,
		ParallelForFlags Flags
	) override
	{
		EParallelForFlags UseFlags = (Flags.bForceSingleThread) ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None;
		if (Flags.bUnbalanced)
			UseFlags |= EParallelForFlags::Unbalanced;

		::ParallelForTemplate((int32)NumJobs, JobFunction, UseFlags);
	}


	virtual TaskContainer launch_task(
		const char* Identifier,
		std::function<void()> task,
		TaskFlags Flags
	) override
	{
		UE::Tasks::FTask NewTask = UE::Tasks::Launch(ANSI_TO_TCHAR(Identifier), [task]()
		{
			task();
		});

		std::shared_ptr<UETaskWrapper> wrapper = std::make_shared<UETaskWrapper>(NewTask);

		TaskContainer result;
		result.ExternalTask = std::static_pointer_cast<IExternalTaskWrapper, UETaskWrapper>(wrapper);
		return result;
	}


	virtual void wait_for_task(
		TaskContainer& task
	) override
	{
		if (task.ExternalTask)
		{
			std::shared_ptr<UETaskWrapper> wrapper = std::static_pointer_cast<UETaskWrapper, IExternalTaskWrapper>(task.ExternalTask);
			wrapper->Task.Wait();
		}
	}


};


}
