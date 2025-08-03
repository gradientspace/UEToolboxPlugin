// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/GradientspaceSettingsTool.h"

#include "InteractiveToolManager.h"

#include "HAL/Platform.h"
#include "HAL/PlatformProcess.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/EngineBaseTypes.h"

#include "Settings/EditorLoadingSavingSettings.h"
#include "Engine/RendererSettings.h"
#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"
#include "HAL/IConsoleManager.h"

#ifdef WITH_GS_PLUGIN_MANAGER
#include "GradientspacePluginSubsystem.h"
#endif


#define LOCTEXT_NAMESPACE "GradientspaceSettingsTool"

UInteractiveTool* UGradientspaceSettingsToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UGradientspaceSettingsTool>(SceneState.ToolManager);
}

UGradientspaceSettingsTool::UGradientspaceSettingsTool()
{
	UInteractiveTool::SetToolDisplayName(LOCTEXT("ToolName", "Modify Settings"));
}

void UGradientspaceSettingsTool::Setup()
{
	Settings = NewObject<UGradientspaceSettingsToolSettings>(this);
	AddToolPropertySource(Settings);

	UGradientspaceUEToolboxToolSettings* GradientSettings = GetMutableDefault<UGradientspaceUEToolboxToolSettings>();
	Settings->StartupMode = GradientSettings->StartupEditorMode;
	Settings->WatchProperty(Settings->StartupMode, [GradientSettings, this](EGSUEToolboxStartupEditorMode) {
		GradientSettings->StartupEditorMode = Settings->StartupMode; GradientSettings->SaveConfig(); 
	});

	Settings->bModesToolbar = GradientSettings->bAddModeButtonsToMainToolbar;
	bInitialModesToolbar = Settings->bModesToolbar;
	Settings->WatchProperty(Settings->bModesToolbar, [GradientSettings, this](bool) {
		GradientSettings->bAddModeButtonsToMainToolbar = Settings->bModesToolbar; GradientSettings->SaveConfig();
		UpdateRestartMessage();
	});

	TSharedPtr<FGSActionButtonTarget> GSActionsButtonTarget = MakeShared<FGSActionButtonTarget>();
	GSActionsButtonTarget->ExecuteCommand = [this](FString CommandName) { GradientspaceAction(CommandName); };
	Settings->GSSettingsActions.Target = GSActionsButtonTarget;
	Settings->GSSettingsActions.AddAction("UISettings",
		LOCTEXT("UISettings", "Toolbox Settings"),
		LOCTEXT("UISettingsTooltip", "Open Gradientspace UE Toolbox Settings"));
#ifdef WITH_GS_PLUGIN_MANAGER
	Settings->GSSettingsActions.AddAction("UpdateSettings",
		LOCTEXT("UpdateSettings", "Update Settings"),
		LOCTEXT("UpdateSettingsTooltip", "Open Gradientspace UE Plugin Manager Settings"));
#endif


	GConfig->GetInt(TEXT("Undo"), TEXT("UndoBufferSize"), Settings->UndoBufferSize, GEditorPerProjectIni);
	InitialUndoBufferSize = Settings->UndoBufferSize;
	Settings->WatchProperty(Settings->UndoBufferSize, [this](int) {
		GConfig->SetInt(TEXT("Undo"), TEXT("UndoBufferSize"), Settings->UndoBufferSize, GEditorPerProjectIni);
		GConfig->GetInt(TEXT("Undo"), TEXT("UndoBufferSize"), Settings->UndoBufferSize, GEditorPerProjectIni);
		UpdateRestartMessage();
	});

	UEditorLoadingSavingSettings* LoadingSavingSettings = GetMutableDefault<UEditorLoadingSavingSettings>();
	Settings->bEnableAutosave = LoadingSavingSettings->bAutoSaveEnable;
	Settings->WatchProperty(Settings->bEnableAutosave, [LoadingSavingSettings, this](bool) {
		LoadingSavingSettings->bAutoSaveEnable = Settings->bEnableAutosave; LoadingSavingSettings->SaveConfig();
	});


	URendererSettings* RendererSettings = GetMutableDefault<URendererSettings>();
	auto ModifyRendererProperty = [RendererSettings](FName PropertyName)
	{
		FProperty* Property = RendererSettings->GetClass()->FindPropertyByName(PropertyName);
		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		RendererSettings->PostEditChangeProperty(Event);
		RendererSettings->SaveConfig();
	};


	Settings->bEnableVirtualTextures = RendererSettings->bVirtualTextures;
	bInitialEnableVirtualTextures = Settings->bEnableVirtualTextures;
	Settings->WatchProperty(Settings->bEnableVirtualTextures, [RendererSettings, ModifyRendererProperty, this](bool) {
		RendererSettings->bVirtualTextures = (Settings->bEnableVirtualTextures) ? 1 : 0;
		ModifyRendererProperty(TEXT("bVirtualTextures"));
		Settings->bEnableVirtualTextures = RendererSettings->bVirtualTextures;
		UpdateRestartMessage();
	});

	Settings->bEnableNanite = RendererSettings->bNanite;
	bInitialEnableNanite = Settings->bEnableNanite;
	Settings->WatchProperty(Settings->bEnableNanite, [RendererSettings, ModifyRendererProperty, this](bool) {
		RendererSettings->bNanite = (Settings->bEnableNanite) ? 1 : 0;
		ModifyRendererProperty(TEXT("bNanite"));
		Settings->bEnableNanite = RendererSettings->bNanite;
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"))->Set((int32)RendererSettings->bNanite, EConsoleVariableFlags::ECVF_SetByConsole);
		UpdateRestartMessage();
	});

	Settings->GlobalIllumination = RendererSettings->DynamicGlobalIllumination;
	Settings->WatchProperty(Settings->GlobalIllumination, [RendererSettings, ModifyRendererProperty, this](int) {
		RendererSettings->DynamicGlobalIllumination = Settings->GlobalIllumination;
		ModifyRendererProperty(TEXT("DynamicGlobalIllumination"));
		Settings->GlobalIllumination = RendererSettings->DynamicGlobalIllumination;
		Settings->Reflections = RendererSettings->Reflections;
	});

	Settings->Reflections = RendererSettings->Reflections;
	Settings->WatchProperty(Settings->Reflections, [RendererSettings, ModifyRendererProperty, this](int) {
		RendererSettings->Reflections = Settings->Reflections;
		ModifyRendererProperty(TEXT("Reflections"));
		Settings->Reflections = RendererSettings->Reflections;
		NotifyOfPropertyChangeByTool(Settings);	  // necessary if reflections changed
	});

	Settings->ShadowMaps = RendererSettings->ShadowMapMethod;
	Settings->WatchProperty(Settings->ShadowMaps, [RendererSettings, ModifyRendererProperty, this](int) {
		RendererSettings->ShadowMapMethod = Settings->ShadowMaps;
		ModifyRendererProperty(TEXT("ShadowMapMethod"));
		Settings->ShadowMaps = RendererSettings->ShadowMapMethod;
	});

	TSharedPtr<FGSActionButtonTarget> RenderingPresentsButtonTarget = MakeShared<FGSActionButtonTarget>();
	RenderingPresentsButtonTarget->ExecuteCommand = [this](FString CommandName) { ApplyRenderingPreset(CommandName); };
	Settings->RenderingPresets.Target = RenderingPresentsButtonTarget;
	Settings->RenderingPresets.AddAction("UE4",
		LOCTEXT("UE4Rendering", "UE4"),
		LOCTEXT("UE4RenderingTooltip", "Disable UE5 Rendering features (Nanite/VSM/Lumen/etc)"));
	Settings->RenderingPresets.AddAction("UE5",
		LOCTEXT("UE5Rendering", "UE5"),
		LOCTEXT("UE5RenderingTooltip", "Enable UE5 Rendering features (Nanite/VSM/Lumen/etc)"));

	// ignore any differences due to class defaults
	Settings->SilentUpdateWatched();
}



void UGradientspaceSettingsTool::GradientspaceAction(FString CommandName)
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (CommandName == TEXT("UISettings")) {
		if (SettingsModule)
			SettingsModule->ShowViewer("Project", "Plugins", "Gradientspace UE Toolbox");
	}
	else if (CommandName == TEXT("UpdateSettings")) {
		if (SettingsModule)
			SettingsModule->ShowViewer("Project", "Plugins", "Gradientspace Plugin Manager");
	}
}

void UGradientspaceSettingsTool::ApplyRenderingPreset(FString Name)
{
	URendererSettings* RendererSettings = GetMutableDefault<URendererSettings>();
	auto ModifyRendererProperty = [RendererSettings](FName PropertyName)
	{
		FProperty* Property = RendererSettings->GetClass()->FindPropertyByName(PropertyName);
		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		RendererSettings->PostEditChangeProperty(Event);
	};

	if (Name == TEXT("UE4"))
	{
		RendererSettings->DynamicGlobalIllumination = EDynamicGlobalIlluminationMethod::Type::None;
		RendererSettings->Reflections = EReflectionMethod::Type::None;
		RendererSettings->ShadowMapMethod = EShadowMapMethod::Type::ShadowMaps;
		RendererSettings->bNanite = false;
	}
	else
	{
		RendererSettings->ShadowMapMethod = EShadowMapMethod::Type::VirtualShadowMaps;
		RendererSettings->Reflections = EReflectionMethod::Type::Lumen;
		RendererSettings->DynamicGlobalIllumination = EDynamicGlobalIlluminationMethod::Type::Lumen;
		RendererSettings->bNanite = true;
	}
	ModifyRendererProperty(TEXT("ShadowMapMethod"));
	ModifyRendererProperty(TEXT("bNanite"));
	ModifyRendererProperty(TEXT("Reflections"));
	ModifyRendererProperty(TEXT("DynamicGlobalIllumination"));
	RendererSettings->SaveConfig();

	IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"))->Set((int32)RendererSettings->bNanite, EConsoleVariableFlags::ECVF_SetByConsole);

	Settings->GlobalIllumination = RendererSettings->DynamicGlobalIllumination;
	Settings->Reflections = RendererSettings->Reflections;
	Settings->ShadowMaps = RendererSettings->ShadowMapMethod;
	Settings->bEnableNanite = RendererSettings->bNanite;
	NotifyOfPropertyChangeByTool(Settings);
}

void UGradientspaceSettingsTool::UpdateRestartMessage()
{
	bool bRestartRequired =
		(Settings->bModesToolbar != bInitialModesToolbar)
		|| (Settings->UndoBufferSize != InitialUndoBufferSize)
		|| (Settings->bEnableNanite != bInitialEnableNanite)
		|| (Settings->bEnableVirtualTextures != bInitialEnableVirtualTextures);

	if (bRestartRequired)
		GetToolManager()->DisplayMessage(LOCTEXT("RestartRequired", "Editor must be restarted for applied settings changes to take effect."), EToolMessageLevel::UserError);
	else
		GetToolManager()->DisplayMessage(FText::GetEmpty(), EToolMessageLevel::UserError);
}


#undef LOCTEXT_NAMESPACE