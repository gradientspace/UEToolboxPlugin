// Copyright Gradientspace Corp. All Rights Reserved.

#include "Platform/GSPlatformSubsystem.h"

#include "Engine/Engine.h"



class FDummyPlatformAPI : public GS::UEPlatformAPI
{
public:
	virtual bool ShowModalOpenFilesDialog(
		const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile,
		const FString& FileTypesFilters, TArray<FString>& FilenamesOut, int32& SelectedFilterIndexOut, bool bAllowMultipleFiles) override { return false; }
	virtual bool ShowModalSaveFileDialog(
		const FString& DialogTitle,
		const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypesFilters, FString& FilenameOut) override { return false; }
	virtual bool ShowModalSelectFolderDialog(
		const FString& DialogTitle, const FString& DefaultPath, FString& FolderNameOut) override { return false; }
};


void UGSPlatformSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	TSharedPtr<GS::UEPlatformAPI> BaseAPI = MakeShared<FDummyPlatformAPI>();
	PlatformAPIStack.Add(BaseAPI);
}


void UGSPlatformSubsystem::PushPlatformAPI(TSharedPtr<GS::UEPlatformAPI> API)
{
	UGSPlatformSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGSPlatformSubsystem>();
	if (ensure(Subsystem))
		Subsystem->PlatformAPIStack.Add(API);
}

void UGSPlatformSubsystem::PopPlatformAPI()
{
	UGSPlatformSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGSPlatformSubsystem>();
	if (ensure(Subsystem)) {
		if (Subsystem->PlatformAPIStack.Num() == 1) {
			ensureMsgf(false, TEXT("UGSPlatformSubsystem::PopPlatformAPI() : unmatched Push/Pop, trying to pop default API"));
			return;
		}
		Subsystem->PlatformAPIStack.Pop();
	}
}


GS::UEPlatformAPI& UGSPlatformSubsystem::GetPlatformAPI()
{
	UGSPlatformSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGSPlatformSubsystem>();
	check(Subsystem);
	return * (Subsystem->PlatformAPIStack.Last());
}