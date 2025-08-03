// Copyright Gradientspace Corp. All Rights Reserved.
#include "EditorIntegration/GSModelingModeSubsystem.h"

#include "Editor.h"		// for GEditor

#include "InteractiveTool.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolsContext.h"

#include "Misc/EngineVersionComparison.h"
#if UE_VERSION_OLDER_THAN(5,6,0)
#include "EdModeInteractiveToolsContext.h"
#else
#include "Tools/EdModeInteractiveToolsContext.h"
#endif

#include "ModelingToolsEditorMode.h"
#include "ModelingToolsManagerActions.h"
#include "EditorModeManager.h"
#include "Toolkits/BaseToolkit.h"
#include "GradientspaceUEToolboxHotkeys.h"
#include "GradientspaceUEToolboxCommands.h"
#include "Settings/PersistentToolSettings.h"
#include "Settings/GSToolSubsystem.h"

#include "EditorIntegration/GSToolFeedbackWidget.h"
#include "Widgets/SOverlay.h"

#include "LevelEditor.h"
#include "LevelEditorModesActions.h"
#include "ILevelEditor.h"
#include "SLevelViewport.h"
#include "ToolMenus.h"
#include "EditorModes.h"

#include "Toolkits/AssetEditorToolkit.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "UGSModelingModeSubsystem"


UGSModelingModeSubsystem* UGSModelingModeSubsystem::Get()
{
	UGSModelingModeSubsystem* Subsystem = GEditor->GetEditorSubsystem<UGSModelingModeSubsystem>();
	ensure(Subsystem);
	return Subsystem;
}


void UGSModelingModeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UGSToolSubsystem::Get()->SetActiveAPI(this);
}

void UGSModelingModeSubsystem::RegisterToolHotkeyBindings(UInteractiveTool* Tool)
{
	TSharedPtr<FModeToolkit> Toolkit = FindModelingModeToolkit(Tool);
	if (Toolkit.IsValid())
	{
		FGradientspaceModelingToolHotkeys::UpdateToolCommandBinding(Tool, Toolkit->GetToolkitCommands(), false);
	}
}

void UGSModelingModeSubsystem::UnRegisterToolHotkeyBindings(UInteractiveTool* Tool)
{
	TSharedPtr<FModeToolkit> Toolkit = FindModelingModeToolkit(Tool);
	if (Toolkit.IsValid())
	{
		FGradientspaceModelingToolHotkeys::UpdateToolCommandBinding(Tool, Toolkit->GetToolkitCommands(), true);
	}
}





void UGSModelingModeSubsystem::EnableToolFeedbackWidget(UInteractiveTool* Tool)
{
	//GizmoNumericalUIOverlayWidget = SNew(STransformGizmoNumericalUIOverlay)
	//	.DefaultLeftPadding(15)
	//	.DefaultVerticalPadding(75)
	//	.bPositionRelativeToBottom(true);

	if ( ToolFeedbackWidget.IsValid() == false )
		ToolFeedbackWidget = SNew(SGSToolFeedbackWidget);

	TSharedPtr<FModeToolkit> Toolkit = FindModelingModeToolkit(Tool);
	if (Toolkit.IsValid())
	{
		Toolkit->GetToolkitHost()->AddViewportOverlayWidget(ToolFeedbackWidget.ToSharedRef());
		
		//SAssignNew(ToolFeedbackWidget, FGSToolFeedbackWidget);
		//ToolFeedbackWidget = MakeShared<FGSToolFeedbackWidget>();
		//TSharedPtr<SWidget> OverlayWidget = ToolFeedbackWidget->Initialize();
	}
}

void UGSModelingModeSubsystem::DisableToolFeedbackWidget(UInteractiveTool* Tool)
{
	TSharedPtr<FModeToolkit> Toolkit = FindModelingModeToolkit(Tool);
	if (Toolkit.IsValid() && Toolkit->IsHosted() && ToolFeedbackWidget.IsValid())
	{
		Toolkit->GetToolkitHost()->RemoveViewportOverlayWidget(ToolFeedbackWidget.ToSharedRef());
		ToolFeedbackWidget.Reset();
	}
	else if (ToolFeedbackWidget.IsValid())
	{
		TSharedPtr<SWidget> ParentWidget = ToolFeedbackWidget->GetParentWidget();
		if (ParentWidget.IsValid() && ParentWidget->GetTypeAsString().Compare(TEXT("SOverlay")) == 0)
		{
			TSharedPtr<SOverlay> ViewportOverlay = StaticCastSharedPtr<SOverlay>(ParentWidget);
			ViewportOverlay->RemoveSlot(ToolFeedbackWidget.ToSharedRef());
			ToolFeedbackWidget.Reset();
		}
	}
}


void UGSModelingModeSubsystem::SetToolFeedbackStrings(const GS::GSErrorSet& ErrorSet)
{
	if (ToolFeedbackWidget.IsValid())
	{
		ToolFeedbackWidget->SetFromErrorSet(ErrorSet);
	}
}






bool UGSModelingModeSubsystem::IsFlightCameraActive()
{
	// todo this can be static and only done once...
	// TODO can probably just use GCurrentLevelEditingViewportClient...

	const TSharedRef<ILevelEditor>& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor").GetFirstLevelEditor().ToSharedRef();
	TSharedPtr<SLevelViewport> ActiveViewport = LevelEditor->GetActiveViewportInterface();
	FEditorViewportClient* UseViewportClient = (ActiveViewport.IsValid()) ? ActiveViewport->GetViewportClient().Get() : nullptr;
	if (UseViewportClient == nullptr)
		UseViewportClient = GCurrentLevelEditingViewportClient;		// hail mary does this work??

	if (!UseViewportClient)
		return false;
	return UseViewportClient->IsFlightCameraActive();
}
bool UGSModelingModeSubsystem::GetIsFlightCameraActive()
{
	if (UGSModelingModeSubsystem* Subsystem = Get())
		return Subsystem->IsFlightCameraActive();
	return false;
}



void UGSModelingModeSubsystem::BeginListeningForModeChanges()
{
	// seems like this is never fired...
	//FEditorDelegates::ChangeEditorMode.AddUObject(this, &UGSModelingModeSubsystem::OnEditorModeChanged);

	//const TSharedRef<ILevelEditor>& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor").GetFirstLevelEditor().ToSharedRef();
	//TSharedPtr<SLevelViewport> ActiveViewport = LevelEditor->GetActiveViewportInterface();
	//FEditorModeTools& ModeTools = LevelEditor->GetEditorModeManager();
	// aaahh the above function just returns GLevelEditorModeTools() !!
	FEditorModeTools& ModeTools = GLevelEditorModeTools();


	if (    (ActiveModeToolsPtr == nullptr) 
		 || (ActiveModeToolsPtr != nullptr && ActiveModeToolsPtr != &ModeTools) )
	{
		// todo can the ModeTools change during an editor session??
		ensure(ActiveModeToolsPtr == nullptr);

		ActiveModeToolsPtr = &ModeTools;
		ModeChangeListenerHandle = ModeTools.OnEditorModeIDChanged().AddUObject(this, &UGSModelingModeSubsystem::OnEditorModeChanged);

		//ActiveModeToolsPtr->ActivateMode()
	}
}




void UGSModelingModeSubsystem::AddModeButtonsToMainToolbar()
{
	UGradientspaceUEToolboxToolSettings* Settings = GetMutableDefault<UGradientspaceUEToolboxToolSettings>();
	if (Settings == nullptr || Settings->bAddModeButtonsToMainToolbar == false)
		return;
	if (FLevelEditorModesCommands::IsRegistered() == false)
		return;	// in this case it seems like we are not running the level editor! Happens for project-picker on raw Editor startup, for example.

	TArray<FName> IncludeModeIDButtons;
	if (Settings->ModeButtons.bSelectionMode)
		IncludeModeIDButtons.Add(FBuiltinEditorModes::EM_Default);
	if (Settings->ModeButtons.bLandscapeMode)
		IncludeModeIDButtons.Add(FBuiltinEditorModes::EM_Landscape);
	if (Settings->ModeButtons.bFoliageMode)
		IncludeModeIDButtons.Add(FBuiltinEditorModes::EM_Foliage);
	if (Settings->ModeButtons.bPaintMode)
		IncludeModeIDButtons.Add(TEXT("MeshPaintMode"));
	if (Settings->ModeButtons.bFractureMode)
		IncludeModeIDButtons.Add(TEXT("EM_FractureEditorMode"));
	if (Settings->ModeButtons.bGeometryMode)
		IncludeModeIDButtons.Add(TEXT("EM_Geometry"));
	if (Settings->ModeButtons.bAnimationMode)
		IncludeModeIDButtons.Add(TEXT("EditMode.ControlRig"));
	if (Settings->ModeButtons.bModelingMode)
		IncludeModeIDButtons.Add(TEXT("EM_ModelingToolsEditorMode"));
	if (Settings->ModeButtons.CustomMode1.Len() > 0)
		IncludeModeIDButtons.Add(FName(Settings->ModeButtons.CustomMode1));
	if (Settings->ModeButtons.CustomMode2.Len() > 0)
		IncludeModeIDButtons.Add(FName(Settings->ModeButtons.CustomMode2));
	if (Settings->ModeButtons.CustomMode3.Len() > 0)
		IncludeModeIDButtons.Add(FName(Settings->ModeButtons.CustomMode3));
	if (IncludeModeIDButtons.Num() == 0)
		return;

	// grab some global stuff from the level editor...
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedRef<FUICommandList> CommandBindings = LevelEditorModule.GetGlobalLevelEditorActions();
	const FLevelEditorModesCommands& ModesCommands = LevelEditorModule.GetLevelEditorModesCommands();

	FToolMenuOwnerScoped OwnerScoped(this);		// not sure what this is for exactly, but other UToolMenu extenders do it
	// hardcoded name for the modes toolbar, from FLevelEditorToolBar::RegisterLevelEditorToolBar()
	UToolMenu* ModesToolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.ModesToolBar");

	// maybe should be using AddSectionDynamic like FLevelEditorToolBar::RegisterLevelEditorToolBar() does?

	// add a new section
	FToolMenuSection& NewSection = ModesToolbar->AddSection("GSModes", LOCTEXT("GSModes", "GSModes"));

	for (FName ModeID : IncludeModeIDButtons)
	{
		// this is how the command names are constructed in FLevelEditorModesCommands::RegisterCommands()
		FName EditorModeCommandName = FName(*(FString("EditorMode.") + ModeID.ToString()));
		TSharedPtr<FUICommandInfo> EditorModeCommand =
			FInputBindingManager::Get().FindCommandInContext(ModesCommands.GetContextName(), EditorModeCommandName);
		if (EditorModeCommand.IsValid() == false)
			continue;
		// cannot directly add the FUICommandInfo here because it is not a Button type and InitToolbarButton will ensure :(
		NewSection.AddEntry(FToolMenuEntry::InitToolBarButton(
			EditorModeCommandName,
			FToolUIActionChoice(EditorModeCommand, *CommandBindings),
			EditorModeCommand->GetLabel(), EditorModeCommand->GetDescription(), EditorModeCommand->GetIcon()));
	}
	NewSection.AddSeparator(NAME_None);

	if (Settings->bHideModesDropDown)
		ModesToolbar->RemoveSection("EditorModes");
}



void UGSModelingModeSubsystem::OnEditorModeChanged(const FEditorModeID& ModeID, bool IsEnteringMode)
{
	static const FName ModelingModeID(TEXT("EM_ModelingToolsEditorMode"));

	//UE_LOG(LogTemp, Warning, TEXT("ModeTools: CHANGING TO MODE %s"), *ModeID.ToString());
	if (ModeID == ModelingModeID)
	{
		bModelingModeActive = IsEnteringMode;

		if (IsEnteringMode)
			LastModelingMode = ActiveModeToolsPtr->GetActiveScriptableMode(ModelingModeID);

		if (LastModelingMode.IsValid())
		{
			TWeakPtr<FModeToolkit> ModelingModeToolkit = LastModelingMode->GetToolkit();
			if (ModelingModeToolkit.IsValid())
			{
				if (IsEnteringMode)
				{
					TSharedRef<FUICommandList> ToolkitCommands = ModelingModeToolkit.Pin()->GetToolkitCommands();
					FGradientspaceModelingModeHotkeys::RegisterCommandBindings(ToolkitCommands,
						[this](EGradientspaceModelingModeCommands Command) {
							OnExecuteModelingModeCommand((int)Command);
					});
				}
				else
				{
					FGradientspaceModelingModeHotkeys::UnRegisterCommandBindings(ModelingModeToolkit.Pin()->GetToolkitCommands());
				}
			}
		}

	}
}
void UGSModelingModeSubsystem::OnEditorModeChanged(FEditorModeID ModeID)
{
	//UE_LOG(LogTemp, Warning, TEXT("EditorDelegates: CHANGING TO MODE %s"), *ModeID.ToString());
}


void UGSModelingModeSubsystem::OnExecuteModelingModeCommand(int CommandEnum)
{
	if (!bModelingModeActive) return;

	EGradientspaceModelingModeCommands Command = (EGradientspaceModelingModeCommands)CommandEnum;

	if (LastModelingMode.IsValid() && LastModelingMode->GetToolkit().IsValid())
	{
		TWeakPtr<FModeToolkit> ModelingModeToolkit = LastModelingMode->GetToolkit();
		TSharedRef<FUICommandList> ToolkitCommands = ModelingModeToolkit.Pin()->GetToolkitCommands();

		if (Command == EGradientspaceModelingModeCommands::BeginQuickMoveTool)
			ToolkitCommands->ExecuteAction(FGradientspaceUEToolboxCommands::Get().BeginMoverTool.ToSharedRef());
	}
}




static FName GetStartupEditorMode()
{
	FName SettingsModeID = FName();
	if (UGradientspaceUEToolboxToolSettings* Settings = GetMutableDefault<UGradientspaceUEToolboxToolSettings>())
	{
		if (Settings->StartupEditorMode == EGSUEToolboxStartupEditorMode::ModelingMode)
			SettingsModeID = FName(TEXT("EM_ModelingToolsEditorMode"));
		else if (Settings->StartupEditorMode == EGSUEToolboxStartupEditorMode::FractureMode)
			SettingsModeID = FName(TEXT("EM_FractureEditorMode"));

		// this doesn't work for sequencer...
		//else if (Settings->StartupEditorMode == EGSUEToolboxStartupEditorMode::Sequencer)
		//	SettingsModeID = FName(TEXT("EM_SequencerMode"));
	}
	return SettingsModeID;
}



void UGSModelingModeSubsystem::TrySwapToStartupEditorMode()
{
	if (IsRunningCommandlet()) return;
	if (GEditor == nullptr) return;

	FName InitialEditorModeID = GetStartupEditorMode();
	if (InitialEditorModeID.IsNone() == false)
	{
		// list for FEditorDelegates::OnEditorInitialized. This is fired at the end of FUnrealEdMisc::OnInit(), which is also
		// what opens the initial map, which would fire FEditorDelegates::OnMapOpened. 
		EditorInitializedHandle = FEditorDelegates::OnEditorInitialized.AddLambda([](double Duration) {

			// note: for some reason it is not safe to capture 'this' pointer to a subsystem here...if I run with
			// -stompmalloc, the captured [this] pointer will become garbage at some point. It's not clear what is
			// causing this, maybe some weird uobject thing, or maybe editor subsystems get recreated at some point?
			// Anyway just getting the pointer again resolves it...
			//    (it would seem that something would go wrong w/ the handle, but it doesn't seem to...)
			UGSModelingModeSubsystem* CurSubsystem = UGSModelingModeSubsystem::Get();
			CurSubsystem->PendingEditorMode = GetStartupEditorMode();
			CurSubsystem->bSwapToEditorModePending = true;
			FEditorDelegates::OnEditorInitialized.Remove(CurSubsystem->EditorInitializedHandle);
			CurSubsystem->EditorInitializedHandle.Reset();

			// any time a new map is opened, it will reset the editor mode to Selection, so lets switch it back
			// FWorldDelegates has FOnLevelChanged and OnPostWorldInitialization which may happen later? 
			// They get the UWorld, and the UWorld also has OnActorsInitialized...
			FEditorDelegates::OnMapOpened.AddLambda([](const FString& Filename, bool bAsTemplate)
			{
				FName LoadModeID = GetStartupEditorMode();
				if (LoadModeID.IsNone() == false) {
					UGSModelingModeSubsystem* CurSubsystem = UGSModelingModeSubsystem::Get();
					CurSubsystem->PendingEditorMode = LoadModeID;
					CurSubsystem->bSwapToEditorModePending = true;
				}
			});
		});
	}
}


void UGSModelingModeSubsystem::Tick(float DeltaTime)
{
	//  this doesn't work because we can't get the right CommandList. We need the LoadToolPaletteCommandList that
	//  lives inside the ToolkitBuilder inside the Toolkit :(
	//static bool bSelectModelingTabNextFrame = false;
	//if (bSelectModelingTabNextFrame)
	//{
	//	UEdMode* ModelingMode = GLevelEditorModeTools().GetActiveScriptableMode(FName(TEXT("EM_ModelingToolsEditorMode")));
	//	if (ModelingMode && ModelingMode->GetToolkit().IsValid()) {
	//		TSharedPtr<FModeToolkit> Toolkit = ModelingMode->GetToolkit().Pin();
	//		const FModelingToolsManagerCommands& Commands = FModelingToolsManagerCommands::Get();
	//		const TSharedPtr<FUICommandList> CommandList = Toolkit->GetToolkitCommands();
	//		CommandList->ExecuteAction(Commands.LoadDeformTools.ToSharedRef());
	//	}
	//	bSelectModelingTabNextFrame = false;
	//}

	if (bSwapToEditorModePending)
	{
		// wait some ticks before we try to switch modes  (a bit paranoid...is there something else we should wait for?)
		static int SwapToModeCountTicks = 0;
		if (SwapToModeCountTicks++ == 10)
		{
			FEditorModeTools& ModeTools = GLevelEditorModeTools();
			ModeTools.ActivateMode(PendingEditorMode);
			bSwapToEditorModePending = false;
			SwapToModeCountTicks = 0;

			//if (PendingEditorMode.ToString().Contains("Modeling"))
			//	bSelectModelingTabNextFrame = true;
		}
	}

	static int EditorSetupCountTicks = 0;
	if (bInitialEditorSetupCompleted == false && EditorSetupCountTicks++ == 10)
	{
		AddModeButtonsToMainToolbar();
		bInitialEditorSetupCompleted = true;
	}

}
bool UGSModelingModeSubsystem::IsTickable() const 
{
	return true;
}
TStatId UGSModelingModeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSModelingModeSubsystem, STATGROUP_Tickables);
}




TSharedPtr<FModeToolkit> UGSModelingModeSubsystem::FindModelingModeToolkit(UInteractiveTool* Tool)
{
	// possibly can do this via LastModelingMode now...

	UInteractiveToolManager* ToolManager = Tool->GetToolManager();
	if (!ToolManager) return TSharedPtr<FModeToolkit>();
	UInteractiveToolsContext* ToolsContext = Cast<UInteractiveToolsContext>(ToolManager->GetOuter());
	if (!ToolsContext) return TSharedPtr<FModeToolkit>();
	UEditorInteractiveToolsContext* EditorToolsContext = Cast<UEditorInteractiveToolsContext>(ToolsContext);
	if (!EditorToolsContext) return TSharedPtr<FModeToolkit>();
	FEditorModeTools* ModeTools = EditorToolsContext->GetParentEditorModeManager();
	if (!ModeTools) return TSharedPtr<FModeToolkit>();

	// sanity check this...
	ensure(ModeTools == ActiveModeToolsPtr);

	FName ModelingModeID(TEXT("EM_ModelingToolsEditorMode"));
	UEdMode* ModelingMode = ModeTools->GetActiveScriptableMode(ModelingModeID);
	if (!ModelingMode) return TSharedPtr<FModeToolkit>();

	TSharedPtr<FModeToolkit> Toolkit = ModelingMode->GetToolkit().Pin();
	return Toolkit.IsValid() ? Toolkit : TSharedPtr<FModeToolkit>();
}



#undef LOCTEXT_NAMESPACE
