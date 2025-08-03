// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "PropertyTypes/ActionButtonGroup.h"
#include "Engine/EngineTypes.h"
#include "Settings/PersistentToolSettings.h"

#include "GradientspaceSettingsTool.generated.h"

UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGradientspaceSettingsToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGradientspaceSettingsToolSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/**
	 * Configure which Editor Mode the UE Editor will switch to on Startup
	 */
	UPROPERTY(EditAnywhere, Category = "Gradientspace Settings", meta = (TransientToolProperty))
	EGSUEToolboxStartupEditorMode StartupMode = EGSUEToolboxStartupEditorMode::Disabled;

	/** When enabled, buttons for the Editor Modes selected below are added to the main Editor toolbar, next to the Modes dropdown */
	UPROPERTY(EditAnywhere, Category = "Gradientspace Settings", meta = (TransientToolProperty))
	bool bModesToolbar = true;

	UPROPERTY(EditAnywhere, Category = "Gradientspace Settings")
	FGSActionButtonGroup GSSettingsActions;



	/**
	 * Configure the size of the Undo Buffer in Megabytes.
	 * The default is 32MB which is very small for tasks like texture painting or mesh editing
	 *
	 * An Editor Restart is required after changing this setting.
	 */
	UPROPERTY(EditAnywhere, Category="Editor Settings", meta=(TransientToolProperty, UIMin=16, UIMax=4096, ClampMin=16, ClampMax=16384, Units="mb"))
	int UndoBufferSize = 32;

	UPROPERTY(EditAnywhere, Category = "Editor Settings", meta = (TransientToolProperty))
	bool bEnableAutosave = true;





	/**
	 * Configure whether Virtual Texturing is enabled or not.
	 * 
	 * An Editor Restart is required after changing this setting.
	 */
	UPROPERTY(EditAnywhere, Category = "Rendering Settings", meta = (DisplayName="Virtual Textures", TransientToolProperty))
	bool bEnableVirtualTextures = true;

	/**
	 * Configure whether Nanite Rendering is enabled or not.
	 * This setting enables or disables Nanite at the Project level.
	 * 
	 * An Editor Restart is required after changing this setting.
	 */
	UPROPERTY(EditAnywhere, Category = "Rendering Settings", meta = (TransientToolProperty))
	bool bEnableNanite = true;

	/**
	 * Configure Shadow Maps method
	 */
	UPROPERTY(EditAnywhere, Category = "Rendering Settings", meta = (TransientToolProperty))
	TEnumAsByte<EShadowMapMethod::Type> ShadowMaps = EShadowMapMethod::Type::VirtualShadowMaps;

	/**
	 * Configure Dynamic GI Method (Lumen enable/disable)
	 */
	UPROPERTY(EditAnywhere, Category = "Rendering Settings", meta = (TransientToolProperty))
	TEnumAsByte<EDynamicGlobalIlluminationMethod::Type> GlobalIllumination = EDynamicGlobalIlluminationMethod::Type::Lumen;

	/**
	 * Configure Reflection Method
	 */
	UPROPERTY(EditAnywhere, Category = "Rendering Settings", meta = (TransientToolProperty))
	TEnumAsByte<EReflectionMethod::Type> Reflections = EReflectionMethod::Type::Lumen;

	UPROPERTY(EditAnywhere, Category = "Rendering Settings")
	FGSActionButtonGroup RenderingPresets;

};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGradientspaceSettingsTool
	: public UInteractiveTool
{
	GENERATED_BODY()
public:
	UGradientspaceSettingsTool();
	virtual void Setup() override;

	UPROPERTY()
	TObjectPtr<UGradientspaceSettingsToolSettings> Settings;

	virtual void ApplyRenderingPreset(FString Name);
	virtual void GradientspaceAction(FString Name);

protected:
	EGSUEToolboxStartupEditorMode PrevStartupMode;
	bool bInitialModesToolbar;
	int InitialUndoBufferSize;
	bool bInitialEnableNanite;
	bool bInitialEnableVirtualTextures;
	void UpdateRestartMessage();
};