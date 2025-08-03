// Copyright Gradientspace Corp. All Rights Reserved.
#include "GradientspaceUECoreModule.h"
#include "GradientspaceCoreModule.h"
#include "GradientspaceIOModule.h"
#include "GradientspaceGridModule.h"
#include "GradientspaceUELogging.h"

#include "Core/UEParallelImplementation.h"


#define LOCTEXT_NAMESPACE "FGradientspaceUECoreModule"


void FGradientspaceUECoreModule::StartupModule()
{
	FModuleManager::LoadModuleChecked<FGradientspaceCoreModule>("GradientspaceCore");
	FModuleManager::LoadModuleChecked<FGradientspaceIOModule>("GradientspaceIO");
	FModuleManager::LoadModuleChecked<FGradientspaceGridModule>("GradientspaceGrid");

	// register default parallel implementation
	GS::UniquePtr<GS::UEParallel_Impl> ParallelImpl = GSMakeUniquePtr<GS::UEParallel_Impl>();
	GS::Parallel::RegisterAPI( std::move(ParallelImpl) );
}

void FGradientspaceUECoreModule::ShutdownModule()
{
}


DEFINE_LOG_CATEGORY(LogGradientspace);


#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGradientspaceUECoreModule, GradientspaceUECore)
