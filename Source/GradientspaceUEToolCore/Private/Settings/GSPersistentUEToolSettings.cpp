// Copyright Gradientspace Corp. All Rights Reserved.
#include "Settings/GSPersistentUEToolSettings.h"

#include "Misc/Paths.h"
#include "Core/UEVersionCompat.h"

#define LOCTEXT_NAMESPACE "UGradientspaceUEToolboxToolSettings"


#if WITH_EDITOR
FText UGSPersistentUEToolSettings::GetSectionText() const
{
	return LOCTEXT("SettingsName", "Gradientspace Tools");
}
FText UGSPersistentUEToolSettings::GetSectionDescription() const
{
	return LOCTEXT("SettingsDescription", "Gradientspace Tools Settings");
}
#endif

FString UGSPersistentUEToolSettings::GetLastMeshImportFolder()
{
	if (UGSPersistentUEToolSettings* Settings = GetMutableDefault<UGSPersistentUEToolSettings>())
	{
		FString UsePath = Settings->LastMeshImportFolder;
		if (FPaths::DirectoryExists(UsePath) == false)
		{
			UsePath = FPaths::RootDir();
		}
		return UsePath;
	}
	return FPaths::RootDir();
}

void UGSPersistentUEToolSettings::SetLastMeshImportFolder(FString NewFolder)
{
	if (UGSPersistentUEToolSettings* Settings = GetMutableDefault<UGSPersistentUEToolSettings>())
	{
		if (FPaths::DirectoryExists(NewFolder))
		{
			Settings->LastMeshImportFolder = NewFolder;
			Settings->SaveConfig();
		}
	}
}


FString UGSPersistentUEToolSettings::GetLastMeshExportFolder()
{
	if (UGSPersistentUEToolSettings* Settings = GetMutableDefault<UGSPersistentUEToolSettings>())
	{
		FString UsePath = Settings->LastMeshExportFolder;
		if (FPaths::DirectoryExists(UsePath) == false)
		{
			UsePath = FPaths::RootDir();
		}
		return UsePath;
	}
	return FPaths::RootDir();
}

void UGSPersistentUEToolSettings::SetLastMeshExportFolder(FString NewFolder)
{
	if (UGSPersistentUEToolSettings* Settings = GetMutableDefault<UGSPersistentUEToolSettings>())
	{
		if (FPaths::DirectoryExists(NewFolder))
		{
			Settings->LastMeshExportFolder = NewFolder;
			Settings->SaveConfig();
		}
	}
}




TArray<FString> UGSPersistentUEToolSettings::GetRecentMeshImportsList()
{
	TArray<FString> ResultList;
	if (UGSPersistentUEToolSettings* Settings = GetMutableDefault<UGSPersistentUEToolSettings>()) {
		for (FString FilePath : Settings->RecentImportedMeshFiles) {
			if (FPaths::FileExists(FilePath))
				ResultList.Add(FilePath);
		}
	}
	return ResultList;
}

void UGSPersistentUEToolSettings::AddFileToRecentMeshImportsList(FString NewFile)
{
	const int MaxRecentFiles = 20;

	if (UGSPersistentUEToolSettings* Settings = GetMutableDefault<UGSPersistentUEToolSettings>())
	{
		int32 ExistingIndex = Settings->RecentImportedMeshFiles.IndexOfByKey(NewFile);
		if (ExistingIndex == INDEX_NONE) {
			Settings->RecentImportedMeshFiles.Insert(NewFile, 0);
			if (Settings->RecentImportedMeshFiles.Num() > MaxRecentFiles)
				GSUE::TArraySetNum(Settings->RecentImportedMeshFiles, MaxRecentFiles, false);
		}
		else if (ExistingIndex != 0) {
			GSUE::TArrayRemoveAt(Settings->RecentImportedMeshFiles, ExistingIndex, 1, false);
			Settings->RecentImportedMeshFiles.Insert(NewFile, 0);
		}
	}
}


#undef LOCTEXT_NAMESPACE