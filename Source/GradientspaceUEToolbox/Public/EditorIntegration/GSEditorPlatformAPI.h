// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once 

#include "Platform/GSUEPlatformAPI.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"


namespace GS
{

class GSEditorPlatformAPI : public GS::UEPlatformAPI
{
public:
	virtual bool ShowModalOpenFilesDialog(
		const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile,
		const FString& FileTypesFilters, TArray<FString>& FilenamesOut, int32& SelectedFilterIndexOut, bool bAllowMultipleFiles) override 
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		const void* TopWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		if (DesktopPlatform == nullptr || TopWindowHandle == nullptr) 
			return false;

		EFileDialogFlags::Type Flags = (bAllowMultipleFiles) ? EFileDialogFlags::None : EFileDialogFlags::Multiple;
		bool bSelected = DesktopPlatform->OpenFileDialog(
			TopWindowHandle, DialogTitle, DefaultPath, DefaultFile,
			FileTypesFilters, (uint32)Flags, FilenamesOut, SelectedFilterIndexOut);

		return bSelected;
	}
	virtual bool ShowModalSaveFileDialog(
		const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypeFilters, FString& FilenameOut) override
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		const void* TopWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		if (DesktopPlatform == nullptr || TopWindowHandle == nullptr)
			return false;

		TArray<FString> Filenames;
		bool bFileSelected = DesktopPlatform->SaveFileDialog(
			TopWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypeFilters, (uint32)EFileDialogFlags::None, Filenames);
		if (bFileSelected && Filenames.Num() > 0) {
			FilenameOut = Filenames[0]; return true;
		}
		return false;
	}
	virtual bool ShowModalSelectFolderDialog(
		const FString& DialogTitle, const FString& DefaultPath, FString& FolderNameOut) override 
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		const void* TopWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		if (DesktopPlatform == nullptr || TopWindowHandle == nullptr)
			return false;

		bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
			TopWindowHandle, DialogTitle, DefaultPath, FolderNameOut);
		return bFolderSelected;
	}

};


}