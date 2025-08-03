// Copyright Gradientspace Corp. All Rights Reserved.
#include "GSTemporaryFunctions.h"
#include "GradientspaceScriptUtil.h"
#include "GSTemporarySubsystem.h"
#include "Engine/Engine.h"


#define LOCTEXT_NAMESPACE "UGSScriptLibrary_TemporaryFunctions"

UDynamicMesh* UGSScriptLibrary_TemporaryFunctions::RequestOneFrameTemporaryDynamicMesh()
{
	UGSTemporarySubsystem* Subsystem = GEngine->GetEngineSubsystem<UGSTemporarySubsystem>();
	if (!Subsystem)
		return nullptr;
	return Subsystem->GetOrAllocateOneFrameMesh();
}


void UGSScriptLibrary_TemporaryFunctions::ReturnTemporaryDynamicMesh(UDynamicMesh* Mesh)
{
	UGSTemporarySubsystem* Subsystem = GEngine->GetEngineSubsystem<UGSTemporarySubsystem>();
	if (Subsystem)
		Subsystem->ReturnMesh(Mesh);
}



#undef LOCTEXT_NAMESPACE