// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "Platform/GSUEPlatformAPI.h"
#include "Templates/SharedPointer.h"

#include "GSPlatformSubsystem.generated.h"

/**
 */
UCLASS()
class GRADIENTSPACEUECORE_API UGSPlatformSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:

	// UEngineSubsystem API
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//virtual void Deinitialize() override;

public:
	static void PushPlatformAPI(TSharedPtr<GS::UEPlatformAPI> API);
	static void PopPlatformAPI();

	static GS::UEPlatformAPI& GetPlatformAPI();

protected:
	TArray<TSharedPtr<GS::UEPlatformAPI>> PlatformAPIStack;

};