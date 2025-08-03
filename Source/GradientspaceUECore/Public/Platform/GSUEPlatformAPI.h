// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Containers/UnrealString.h"
#include "Containers/Array.h"

namespace GS
{

class GRADIENTSPACEUECORE_API UEPlatformAPI
{
public:
	virtual ~UEPlatformAPI() {}

	virtual bool ShowModalOpenFilesDialog(
		const FString& DialogTitle,
		const FString& DefaultPath,
		const FString& DefaultFilename,
		const FString& FileTypeFilters,
		TArray<FString>& FilenamesOut,
		int32& SelectedFilterIndexOut,
		bool bAllowMultipleFiles = false ) = 0;

	virtual bool ShowModalSaveFileDialog(
		const FString& DialogTitle,
		const FString& DefaultPath,
		const FString& DefaultFilename,
		const FString& FileTypeFilters,
		FString& FilenameOut) = 0;


	virtual bool ShowModalSelectFolderDialog(
		const FString& DialogTitle,
		const FString& DefaultPath,
		FString& FolderNameOut) = 0;


	static FString MakeFileTypeFilterString(FString Description, FString suffix) {
		return FString::Printf(TEXT("%s (*.%s)|*.%s"), *Description, *suffix, *suffix);
	}

};

}