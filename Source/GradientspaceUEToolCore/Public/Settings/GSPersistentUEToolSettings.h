// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Engine/DeveloperSettings.h"
#include "GSPersistentUEToolSettings.generated.h"


// need runtime and editor versions of this?

//UCLASS(config=GradientspaceToolCore, MinimalAPI, DefaultConfig)		// what does DefaultConfig do here?
UCLASS(config=GradientspaceToolCore, MinimalAPI)		// will be stored in GradientspaceToolCore.ini in /Saved/Config/
class UGSPersistentUEToolSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:

	// UDeveloperSettings 
	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName() const override { return FName("Gradientspace UE Tools"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

protected:
	UPROPERTY(config)
	FString	LastMeshImportFolder = TEXT("C:\\");
	UPROPERTY(config)
	FString	LastMeshExportFolder = TEXT("C:\\");
public:
	static GRADIENTSPACEUETOOLCORE_API 
	FString GetLastMeshImportFolder();

	static GRADIENTSPACEUETOOLCORE_API 
	void SetLastMeshImportFolder(FString NewFolder);

	static GRADIENTSPACEUETOOLCORE_API
	FString GetLastMeshExportFolder();

	static GRADIENTSPACEUETOOLCORE_API
	void SetLastMeshExportFolder(FString NewFolder);


protected:
	UPROPERTY(config)
	TArray<FString> RecentImportedMeshFiles;
public:
	static GRADIENTSPACEUETOOLCORE_API
	TArray<FString> GetRecentMeshImportsList();

	static GRADIENTSPACEUETOOLCORE_API
	void AddFileToRecentMeshImportsList(FString NewFile);


};
