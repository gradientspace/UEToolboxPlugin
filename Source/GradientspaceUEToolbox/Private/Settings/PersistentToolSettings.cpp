// Copyright Gradientspace Corp. All Rights Reserved.
#include "Settings/PersistentToolSettings.h"

#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "GradientspaceUEToolboxToolSettings"


FText UGradientspaceUEToolboxToolSettings::GetSectionText() const
{
	return LOCTEXT("SettingsName", "Gradientspace UE Toolbox");
}

FText UGradientspaceUEToolboxToolSettings::GetSectionDescription() const
{
	return LOCTEXT("SettingsDescription", "Configure the Gradientspace UE Toolbox plugin");
}

FString UGradientspaceUEToolboxToolSettings::GetLastMeshImportFolder()
{
	if (UGradientspaceUEToolboxToolSettings* Settings = GetMutableDefault<UGradientspaceUEToolboxToolSettings>())
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

void UGradientspaceUEToolboxToolSettings::SetLastMeshImportFolder(FString NewFolder)
{
	if (UGradientspaceUEToolboxToolSettings* Settings = GetMutableDefault<UGradientspaceUEToolboxToolSettings>())
	{
		if (FPaths::DirectoryExists(NewFolder))
		{
			Settings->LastMeshImportFolder = NewFolder;
			Settings->SaveConfig();
		}
	}
}


FString UGradientspaceUEToolboxToolSettings::GetLastMeshExportFolder()
{
	if (UGradientspaceUEToolboxToolSettings* Settings = GetMutableDefault<UGradientspaceUEToolboxToolSettings>())
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

void UGradientspaceUEToolboxToolSettings::SetLastMeshExportFolder(FString NewFolder)
{
	if (UGradientspaceUEToolboxToolSettings* Settings = GetMutableDefault<UGradientspaceUEToolboxToolSettings>())
	{
		if (FPaths::DirectoryExists(NewFolder))
		{
			Settings->LastMeshExportFolder = NewFolder;
			Settings->SaveConfig();
		}
	}
}



FString UGradientspaceUEToolboxToolSettings::GetLastGridImportFolder()
{
	if (UGradientspaceUEToolboxToolSettings* Settings = GetMutableDefault<UGradientspaceUEToolboxToolSettings>())
	{
		FString UsePath = Settings->LastGridImportFolder;
		if (FPaths::DirectoryExists(UsePath) == false)
		{
			UsePath = FPaths::RootDir();
		}
		return UsePath;
	}
	return FPaths::RootDir();
}

void UGradientspaceUEToolboxToolSettings::SetLastGridImportFolder(FString NewFolder)
{
	if (UGradientspaceUEToolboxToolSettings* Settings = GetMutableDefault<UGradientspaceUEToolboxToolSettings>())
	{
		if (FPaths::DirectoryExists(NewFolder))
		{
			Settings->LastGridImportFolder = NewFolder;
			Settings->SaveConfig();
		}
	}
}



#undef LOCTEXT_NAMESPACE
