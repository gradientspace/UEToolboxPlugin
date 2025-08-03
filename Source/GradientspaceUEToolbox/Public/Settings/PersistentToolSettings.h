// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Engine/DeveloperSettings.h"
#include "PersistentToolSettings.generated.h"


UENUM()
enum class EGSUEToolboxStartupEditorMode : uint8
{
	Disabled = 0,
	ModelingMode = 1,
	FractureMode = 2
};

USTRUCT()
struct FGSSettingsModeOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Modes")
	bool bSelectionMode = true;

	UPROPERTY(EditAnywhere, Category = "Modes")
	bool bLandscapeMode = true;

	UPROPERTY(EditAnywhere, Category = "Modes")
	bool bFoliageMode = false;

	UPROPERTY(EditAnywhere, Category = "Modes")
	bool bPaintMode = false;

	UPROPERTY(EditAnywhere, Category = "Modes")
	bool bFractureMode = false;

	UPROPERTY(EditAnywhere, Category = "Modes", meta=(DisplayName="Geometry (BSP) Mode"))
	bool bGeometryMode = false;

	UPROPERTY(EditAnywhere, Category = "Modes")
	bool bAnimationMode = false;

	UPROPERTY(EditAnywhere, Category = "Modes")
	bool bModelingMode = true;

	/** Set this string to the FEditorModeID FName for the custom Editor Mode (specified in C++ source code) */
	UPROPERTY(EditAnywhere, Category = "Modes")
	FString CustomMode1;

	/** Set this string to the FEditorModeID FName for the custom Editor Mode (specified in C++ source code) */
	UPROPERTY(EditAnywhere, Category = "Modes")
	FString CustomMode2;

	/** Set this string to the FEditorModeID FName for the custom Editor Mode (specified in C++ source code) */
	UPROPERTY(EditAnywhere, Category = "Modes")
	FString CustomMode3;
};

UCLASS(config = Editor)
class GRADIENTSPACEUETOOLBOX_API UGradientspaceUEToolboxToolSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:

	// UDeveloperSettings 
	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName() const override { return FName("Gradientspace UE Toolbox"); }

	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;



public:
	/**
	 * Configure which Editor Mode the UE Editor will switch to on Startup
	 */
	UPROPERTY(config, EditAnywhere, Category = "Editor Tweaks")
	EGSUEToolboxStartupEditorMode StartupEditorMode = EGSUEToolboxStartupEditorMode::Disabled;

	/** When enabled, buttons for the Editor Modes selected below are added to the main Editor toolbar, next to the Modes dropdown */
	UPROPERTY(config, EditAnywhere, Category = "Editor Tweaks")
	bool bAddModeButtonsToMainToolbar = true;

	UPROPERTY(config, EditAnywhere, Category = "Editor Tweaks", meta = (EditCondition="bAddModeButtonsToMainToolbar==true"))
	FGSSettingsModeOptions ModeButtons;

	/** When enabled, the Editor Modes combo-box in the main Editor Toolbar will (possibly) be hidden */
	UPROPERTY(config, EditAnywhere, Category = "Editor Tweaks", meta = (EditCondition = "bAddModeButtonsToMainToolbar==true"))
	bool bHideModesDropDown = false;

protected:
	UPROPERTY(config)
	FString	LastMeshImportFolder = TEXT("C:\\");
	UPROPERTY(config)
	FString	LastMeshExportFolder = TEXT("C:\\");
public:
	static FString GetLastMeshImportFolder();
	static void SetLastMeshImportFolder(FString NewFolder);
	static FString GetLastMeshExportFolder();
	static void SetLastMeshExportFolder(FString NewFolder);


protected:
	UPROPERTY(config)
	FString	LastGridImportFolder = TEXT("C:\\");
public:
	static FString GetLastGridImportFolder();
	static void SetLastGridImportFolder(FString NewFolder);


};
