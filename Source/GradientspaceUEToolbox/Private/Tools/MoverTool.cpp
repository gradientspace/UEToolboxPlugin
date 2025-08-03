// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/MoverTool.h"
#include "InteractiveToolManager.h"
#include "FrameTypes.h"
#include "LineTypes.h"
#include "Distance/DistLine3Ray3.h"
#include "ToolDataVisualizer.h"
#include "Util/ColorConstants.h"
#include "SceneQueries/SceneSnappingManager.h"
#include "ToolSceneQueriesUtil.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
//#include "BaseBehaviors/KeyAsModifierBehavior.h"
#include "Transforms/QuickAxisTranslater.h"
#include "Intersection/IntersectionUtil.h"

#include "Utility/UEGeometryCollisionScene.h"
#include "Engine/StaticMeshActor.h"

#include "InputBehaviors/GSClickOrDragBehavior.h"

#include "Math/GSQuaternion.h"
#include "Math/GSRay3.h"

#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Engine/Selection.h"

#include "CanvasTypes.h"
#include "CanvasItem.h"
#include "Engine/Engine.h"  // for GEngine->GetSmallFont()
#include "SceneView.h"
#include "LevelEditorViewport.h"		// for toggling editor snapping
#include "Settings/LevelEditorViewportSettings.h"

#include "LandscapeProxy.h"
#include "Landscape.h"

#include "Settings/GSToolSubsystem.h"
#if WITH_EDITOR
#include "EditorIntegration/GSModelingModeSubsystem.h"
#endif


#define LOCTEXT_NAMESPACE "UGSMoverTool"

using namespace UE::Geometry;


static const int32 UGSMoverToolBuilder_ShiftModifierID = 1;
static const int32 UGSMoverToolBuilder_CtrlModifierID = 2;

UInteractiveTool* UGSMoverToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UGSMoverTool* Tool = NewObject<UGSMoverTool>(SceneState.ToolManager);
	Tool->SetTargetWorld(SceneState.World);
	return Tool;
}


UGSMoverTool_Settings::UGSMoverTool_Settings()
{
	GridSteps = FVector(50, 50, 50);
}

UGSMoverTool_EnvironmentSettings::UGSMoverTool_EnvironmentSettings()
{
	AxisLock.bAxisX = false;
	AxisLock.bAxisY = false;
	AxisLock.bAxisZ = false;
}

UGSMoverTool_GrabSettings::UGSMoverTool_GrabSettings()
{
	AxisLock.bAxisX = false;
	AxisLock.bAxisY = false;
	AxisLock.bAxisZ = false;
}

void UGSMoverTool::SetTargetWorld(UWorld* World)
{
	TargetWorld = World;
}

void UGSMoverTool::Setup()
{
	SetToolDisplayName(LOCTEXT("ToolName", "Quick Move"));

	UGSClickOrDragInputBehavior* ClickDragBehavior = NewObject<UGSClickOrDragInputBehavior>(this);
	ClickDragBehavior->Initialize(this, this);
	//ClickDragBehavior->Modifiers.RegisterModifier(UGSMoverToolBuilder_ShiftModifierID, FInputDeviceState::IsShiftKeyDown);
	//ClickDragBehavior->Modifiers.RegisterModifier(UGSMoverToolBuilder_CtrlModifierID, FInputDeviceState::IsCtrlKeyDown);
	ClickDragBehavior->bConsumeEditorCtrlClickDrag = true;
	ClickDragBehavior->SetDragWheelBehavior(this);
	AddInputBehavior(ClickDragBehavior);

	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>(this);
	HoverBehavior->Modifiers.RegisterModifier(UGSMoverToolBuilder_ShiftModifierID, FInputDeviceState::IsShiftKeyDown);
	HoverBehavior->Modifiers.RegisterModifier(UGSMoverToolBuilder_CtrlModifierID, FInputDeviceState::IsCtrlKeyDown);
	HoverBehavior->Initialize(this);
	AddInputBehavior(HoverBehavior);

	Settings = NewObject<UGSMoverTool_Settings>(this);

	ModesEnumButtonsTarget = MakeShared<FGSEnumButtonsTarget>();
	ModesEnumButtonsTarget->GetSelectedItemIdentifier = [this]() { return (int)Settings->Mode_Selected; NotifyOfPropertyChangeByTool(Settings); };
	ModesEnumButtonsTarget->IsItemEnabled = [this](int) { return true; };
	ModesEnumButtonsTarget->IsItemVisible = [this](int) { return true; };
	ModesEnumButtonsTarget->SetSelectedItem = [this](int NewItem) { Settings->Mode_Selected = (EGSMoverToolMode)NewItem; };
	Settings->Mode.AddItem({ TEXT("Vertical"), (int)EGSMoverToolMode::Vertical, LOCTEXT("VerticalPreset", "[1] Vertical"),
		LOCTEXT("VerticalPresetTooltip", "Translate the object vertically along the Z axis   [Ctrl] rotate around Z") });
	Settings->Mode.AddItem({ TEXT("XYPlane"), (int)EGSMoverToolMode::XYPlane, LOCTEXT("XYPlanePreset", "[2] XY Plane"),
		LOCTEXT("XYPlanePresetTooltip", "Translate the object in the XY plane   [Ctrl] Rotate around Z   [Ctrl+Shift] Tilt forward/back") });
	Settings->Mode.AddItem({ TEXT("SmartAxis"), (int)EGSMoverToolMode::SmartAxis, LOCTEXT("SmartAxisPreset", "[3] SmartAxis"),
		LOCTEXT("SmartAxisPresetTooltip", "Translate the object in X, Y, or Z axis based on mouse movement direction") });
	Settings->Mode.AddItem({ TEXT("ScreenSpace"), (int)EGSMoverToolMode::ScreenSpace, LOCTEXT("ScreenSpacePreset", "[4] ScreenSpace"),
		LOCTEXT("ScreenSpacePresetTooltip", "Translate the object in the screen plane  [Shift] Translate forward/back  [Ctrl] Rotate left/right  [Ctrl+Shift] Tilt forward/back") });
	Settings->Mode.AddItem({ TEXT("Environment"), (int)EGSMoverToolMode::Environment, LOCTEXT("EnvironmentPreset", "[5] WorldDrag"),
		LOCTEXT("EnvironmentPresetTooltip", "Translate the object along the Landscape or objects in the level") });
	Settings->Mode.AddItem({ TEXT("Tumble"), (int)EGSMoverToolMode::Tumble, LOCTEXT("TumblePreset", "[6] Tumble"),
		LOCTEXT("TumblePresetTooltip", "Rotate around the object's center") });
	Settings->Mode.AddItem({ TEXT("Point"), (int)EGSMoverToolMode::Point, LOCTEXT("PointPreset", "[7] Point"),
		LOCTEXT("PointPresetTooltip", "Rotate the clicked point towards the cursor  [Ctrl] towards world-hit point  [Shift] view-plane constraint") });
	Settings->Mode.AddItem({ TEXT("Attached"), (int)EGSMoverToolMode::Attached, LOCTEXT("AttachedPreset", "[9] Grab"),
		LOCTEXT("AttachedPresetTooltip", "Grab the object under the cursor when flying, and move it with the camera") });
	Settings->Mode.bShowLabel = false;
	Settings->Mode.bCentered = true;
	Settings->Mode.Target = ModesEnumButtonsTarget;
	Settings->Mode.ButtonSpacingHorz = 4.0; Settings->Mode.ButtonSpacingVert = 4.0;
	Settings->Mode.GroupSpacingAbove = 4.0; Settings->Mode.GroupSpacingBelow = 8.0;
	Settings->Mode.bCalculateFixedButtonWidth = true;
	Settings->Mode.bUseGridLayout = false;
	Settings->WatchProperty(Settings->Mode_Selected, [&](EGSMoverToolMode newMode) { UpdatePropertySetVisibilities(); });


	RotationPivotEnumButtonsTarget = MakeShared<FGSEnumButtonsTarget>();
	RotationPivotEnumButtonsTarget->GetSelectedItemIdentifier = [this]() { return (int)Settings->RotationPivot_Selected; };
	RotationPivotEnumButtonsTarget->IsItemEnabled = [this](int) { return true; };
	RotationPivotEnumButtonsTarget->IsItemVisible = [this](int) { return true; };
	RotationPivotEnumButtonsTarget->SetSelectedItem = [this](int NewItem) { Settings->RotationPivot_Selected = (EGSMoverRotationPivot)NewItem; };
	Settings->RotationPivot.AddItem({ TEXT("Location"), (int)EGSMoverRotationPivot::Location, LOCTEXT("LocationPivotLabel", "Location"),
		LOCTEXT("LocationPivotTooltip", "Rotate around the Location of the target Actor (ie it's Origin / default Pivot point)") });
	Settings->RotationPivot.AddItem({ TEXT("Center"), (int)EGSMoverRotationPivot::Center, LOCTEXT("CenterPivotLabel", "Center"),
		LOCTEXT("CenterPivotTooltip", "Rotate around the Center of the target Actor's Bounding Box") });
	Settings->RotationPivot.AddItem({ TEXT("Base"), (int)EGSMoverRotationPivot::Base, LOCTEXT("BasePivotLabel", "Base"),
		LOCTEXT("BasePivotTooltip", "Rotate around the Base point of the target Actor's Bounding Box") });
	Settings->RotationPivot.Target = RotationPivotEnumButtonsTarget;
	Settings->RotationPivot.bCalculateFixedButtonWidth = true;

	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);

	SmartAxisSettings = NewObject<UGSMoverTool_SmartAxisSettings>(this);
	SmartAxisSettings->RestoreProperties(this);
	AddToolPropertySource(SmartAxisSettings);

	EnvironmentSettings = NewObject<UGSMoverTool_EnvironmentSettings>(this);
	EnvironmentSettings->RestoreProperties(this);
	AddToolPropertySource(EnvironmentSettings);

	GrabSettings = NewObject<UGSMoverTool_GrabSettings>(this);
	GrabSettings->RestoreProperties(this);
	AddToolPropertySource(GrabSettings);
	

	QuickActions = NewObject<UGSMoverTool_QuickActions>(this);
	AddToolPropertySource(QuickActions);


	QuickActionTarget = MakeShared<FGSActionButtonTarget>();
	QuickActionTarget->ExecuteCommand = [this](FString CommandName) { OnQuickActionButtonClicked(CommandName); };
	QuickActionTarget->IsCommandEnabled = [this](FString CommandName) { return OnQuickActionButtonEnabled(CommandName); };
	QuickActions->Actions.Target = QuickActionTarget;
	QuickActions->Actions.AddAction(TEXT("ResetRotation"), LOCTEXT("ResetRotationLabel", "Reset Rotation"),
		LOCTEXT("ResetRotationTooltip", "Reset Rotation Angles on Active Actors to Zero   [Shift+R]"));
	QuickActions->Actions.AddAction(TEXT("SetUpright"), LOCTEXT("SetUprightLabel", "Z Up"),
		LOCTEXT("SetUprightTooltip", "Rotate Local +Z axis to World +Z   [Z]"));
	QuickActions->Actions.AddAction(TEXT("SetUprightNearest"), LOCTEXT("SetUprightNearestLabel", "Nearest Up"),
		LOCTEXT("SetUprightNearestTooltip", "Rotate closest local axis to World +Z   [Shift+Z]"));

	UpdatePropertySetVisibilities();

	Translater = MakeShared<FQuickAxisTranslater>();
	Translater->Initialize();

	USelection::SelectionChangedEvent.AddUObject(this, &UGSMoverTool::OnActorSelectionChanged);

#if WITH_EDITOR
	if (UGSToolSubsystem* Subsystem = UGSToolSubsystem::Get())
		Subsystem->RegisterToolHotkeyBindings(this);
#endif
}

void UGSMoverTool::UpdatePropertySetVisibilities()
{
	SetToolPropertySourceEnabled(SmartAxisSettings, Settings->Mode_Selected == EGSMoverToolMode::SmartAxis);
	SetToolPropertySourceEnabled(EnvironmentSettings, Settings->Mode_Selected == EGSMoverToolMode::Environment);
	SetToolPropertySourceEnabled(GrabSettings, Settings->Mode_Selected == EGSMoverToolMode::Attached);

	if (Settings->Mode_Selected == EGSMoverToolMode::Vertical)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("ToolHotkeysMessage_Vertical", "Click-drag to Translate Vertically  [Ctrl] Rotate around Z  [W/S] Nudge vertically"),
			EToolMessageLevel::UserNotification);
	}
	else if (Settings->Mode_Selected == EGSMoverToolMode::XYPlane)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("ToolHotkeysMessage_XYPlane", "Click-drag to Translate in XY Plane  [Ctrl] Rotate around Z  [Ctrl+Shift] Tilt forward/back"),
			EToolMessageLevel::UserNotification);
	}
	else if (Settings->Mode_Selected == EGSMoverToolMode::SmartAxis)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("ToolHotkeysMessage_SmartAxis", "Click-drag to Translate along best-aligned world axis"),
			EToolMessageLevel::UserNotification);
	}
	else if (Settings->Mode_Selected == EGSMoverToolMode::ScreenSpace)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("ToolHotkeysMessage_ScreenSpace", "Click-drag to Translate in Screen Plane  [Shift] Translate forward/back  [Ctrl] Rotate left/right  [Ctrl+Shift] Tilt forward/back  [W/A/S/D] Nudge translation"),
			EToolMessageLevel::UserNotification);
	}
	else if (Settings->Mode_Selected == EGSMoverToolMode::Environment)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("ToolHotkeysMessage_Environment", "Click-drag to drag on Environment objects  [W/S] Nudge vertically"),
			EToolMessageLevel::UserNotification);
	}
	else if (Settings->Mode_Selected == EGSMoverToolMode::Tumble)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("ToolHotkeysMessage_Tumble", "Rotate around the object's center"),
			EToolMessageLevel::UserNotification);
	}
	else if (Settings->Mode_Selected == EGSMoverToolMode::Point)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("ToolHotkeysMessage_Point", "Rotate the clicked point towards the cursor  [Ctrl] towards world-hit point  [Shift] view-plane constraint"),
			EToolMessageLevel::UserNotification);
	}
	else if (Settings->Mode_Selected == EGSMoverToolMode::Attached)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("ToolHotkeysMessage_Attached", "right-click-drag to move hovered target with camera (with WASD fly control)"),
			EToolMessageLevel::UserNotification);
	}
}


void UGSMoverTool::Shutdown(EToolShutdownType ShutdownType)
{
	USelection::SelectionChangedEvent.RemoveAll(this);

	Settings->SaveProperties(this);
	GrabSettings->SaveProperties(this);
	SmartAxisSettings->SaveProperties(this);
	EnvironmentSettings->SaveProperties(this);


#if WITH_EDITOR
	if (UGSToolSubsystem* Subsystem = UGSToolSubsystem::Get())
		Subsystem->UnRegisterToolHotkeyBindings(this);
#endif
}


void UGSMoverTool::RegisterActions(FInteractiveToolActionSet& ActionSet)
{
	//ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 100,
	//	TEXT("SnapMode"),
	//	LOCTEXT("SnapMode", "Cycle Snapping Mode"),
	//	LOCTEXT("SnapMode_Tooltip", "Cycle through the available Snapping Modes"),
	//	EModifierKey::None, EKeys::S,
	//	[this]() { Settings->Snapping = (EGSMoverToolSnapMode)(((int)Settings->Snapping + 1) % (int)EGSMoverToolSnapMode::Last); NotifyOfPropertyChangeByTool(Settings); });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 551,
		TEXT("MoveMode_Vertical"),
		LOCTEXT("MoveMode_Vertical", "Set Active Move Mode - Vertical"),
		LOCTEXT("MoveMode_Vertical_Tooltip", "Set the active move mode to Vertical translation"),
		EModifierKey::None, EKeys::One,
		[this]() { Settings->Mode_Selected = EGSMoverToolMode::Vertical; NotifyOfPropertyChangeByTool(Settings); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 552,
		TEXT("MoveMode_XYPlane"),
		LOCTEXT("MoveMode_XYPlane", "Set Active Move Mode - XY Plane"),
		LOCTEXT("MoveMode_XYPlane_Tooltip", "Set the active move mode to XY Plane translation"),
		EModifierKey::None, EKeys::Two,
		[this]() { Settings->Mode_Selected = EGSMoverToolMode::XYPlane; NotifyOfPropertyChangeByTool(Settings); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 553,
		TEXT("MoveMode_SmartAxis"),
		LOCTEXT("MoveMode_SmartAxis", "Set Active Move Mode - SmartAxis"),
		LOCTEXT("MoveMode_SmartAxis_Tooltip", "Set the active move mode to SmartAxis translation"),
		EModifierKey::None, EKeys::Three,
		[this]() { Settings->Mode_Selected = EGSMoverToolMode::SmartAxis; NotifyOfPropertyChangeByTool(Settings); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 554,
		TEXT("MoveMode_ScreenSpace"),
		LOCTEXT("MoveMode_ScreenSpace", "Set Active Move Mode - ScreenSpace"),
		LOCTEXT("MoveMode_ScreenSpace_Tooltip", "Set the active move mode to ScreenSpace translation"),
		EModifierKey::None, EKeys::Four,
		[this]() { Settings->Mode_Selected = EGSMoverToolMode::ScreenSpace; NotifyOfPropertyChangeByTool(Settings); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 555,
		TEXT("MoveMode_Environment"),
		LOCTEXT("MoveMode_Environment", "Set Active Move Mode - Environment"),
		LOCTEXT("MoveMode_Environment_Tooltip", "Set the active move mode to Environment translation"),
		EModifierKey::None, EKeys::Five,
		[this]() { Settings->Mode_Selected = EGSMoverToolMode::Environment; NotifyOfPropertyChangeByTool(Settings); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 556,
		TEXT("MoveMode_Tumble"),
		LOCTEXT("MoveMode_Tumble", "Set Active Move Mode - Tumble"),
		LOCTEXT("MoveMode_Tumble_Tooltip", "Set the active move mode to Tumble rotation"),
		EModifierKey::None, EKeys::Six,
		[this]() { Settings->Mode_Selected = EGSMoverToolMode::Tumble; NotifyOfPropertyChangeByTool(Settings); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 557,
		TEXT("MoveMode_Point"),
		LOCTEXT("MoveMode_Point", "Set Active Move Mode - Point"),
		LOCTEXT("MoveMode_Point_Tooltip", "Set the active move mode to Point rotation"),
		EModifierKey::None, EKeys::Seven,
		[this]() { Settings->Mode_Selected = EGSMoverToolMode::Point; NotifyOfPropertyChangeByTool(Settings); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 559,
		TEXT("MoveMode_Attached"),
		LOCTEXT("MoveMode_Attached", "Set Active Move Mode - Grab"),
		LOCTEXT("MoveMode_Attached_Tooltip", "Set the active move mode to Grab Transformation"),
		EModifierKey::None, EKeys::Nine,
		[this]() { Settings->Mode_Selected = EGSMoverToolMode::Attached; NotifyOfPropertyChangeByTool(Settings); });


	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 600,
		TEXT("ArrowKey_Up"), LOCTEXT("ArrowKey_Up", "Move Up"), LOCTEXT("ArrowKey_Up_Tooltip", "Up Arrow Key"),
		EModifierKey::None, EKeys::W, [this]() { OnArrowKey(EKeys::W); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 601,
		TEXT("ArrowKey_ShiftUp"), LOCTEXT("ArrowKey_Up", "Move Up"), LOCTEXT("ArrowKey_Up_Tooltip", "Up Arrow Key"),
		EModifierKey::Shift, EKeys::W, [this]() { OnArrowKey(EKeys::W); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 602,
		TEXT("ArrowKey_Down"), LOCTEXT("ArrowKey_Down", "Move Down"), LOCTEXT("ArrowKey_Down_Tooltip", "Down Arrow Key"),
		EModifierKey::None, EKeys::S, [this]() { OnArrowKey(EKeys::S); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 603,
		TEXT("ArrowKey_ShiftDown"), LOCTEXT("ArrowKey_Down", "Move Down"), LOCTEXT("ArrowKey_Down_Tooltip", "Down Arrow Key"),
		EModifierKey::Shift, EKeys::S, [this]() { OnArrowKey(EKeys::S); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 604,
		TEXT("ArrowKey_Left"), LOCTEXT("ArrowKey_Left", "Move Left"), LOCTEXT("ArrowKey_Left_Tooltip", "Left Arrow Key"),
		EModifierKey::None, EKeys::A, [this]() { OnArrowKey(EKeys::A); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 605,
		TEXT("ArrowKey_ShiftLeft"), LOCTEXT("ArrowKey_Left", "Move Left"), LOCTEXT("ArrowKey_Left_Tooltip", "Left Arrow Key"),
		EModifierKey::Shift, EKeys::A, [this]() { OnArrowKey(EKeys::A); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 606,
		TEXT("ArrowKey_Right"), LOCTEXT("ArrowKey_Right", "Move Right"), LOCTEXT("ArrowKey_Right_Tooltip", "Right Arrow Key"),
		EModifierKey::None, EKeys::D, [this]() { OnArrowKey(EKeys::D); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 607,
		TEXT("ArrowKey_ShiftRight"), LOCTEXT("ArrowKey_Right", "Move Right"), LOCTEXT("ArrowKey_Right_Tooltip", "Right Arrow Key"),
		EModifierKey::Shift, EKeys::D, [this]() { OnArrowKey(EKeys::D); });


	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 700,
		TEXT("ReduceGain"), LOCTEXT("ReduceGain", "Reduce Gain"), LOCTEXT("Reduce_Gain_Tooltip", "Reduce Gain"),
		EModifierKey::None, EKeys::LeftBracket, [this]() { Settings->Gain = FMath::Max(Settings->Gain - 0.1f, 0.1f); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 701,
		TEXT("IncreaseGain"), LOCTEXT("IncreaseGain", "Increase Gain"), LOCTEXT("Increase_Gain_Tooltip", "Increase Gain"),
		EModifierKey::None, EKeys::RightBracket, [this]() { Settings->Gain = FMath::Min(Settings->Gain + 0.1f, 1.0f); });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 800,
		TEXT("ToggleGridSnapKey"), LOCTEXT("ToggleGridSnapKey", "Toggle Grid Snapping"), LOCTEXT("ToggleGridSnapKey_Tooltip", "Toggle Editor Grid Snapping"),
		EModifierKey::None, EKeys::G, [this]() { OnCycleGridSnapping(); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 801,
		TEXT("ToggleUseWorldGridKey"), LOCTEXT("ToggleUseWorldGridKey", "Snap to World Grid"), LOCTEXT("ToggleUseWorldGridKey_Tooltip", "Toggle World Grid Snapping"),
		EModifierKey::Shift, EKeys::G, [this]() { Settings->bToWorldGrid = !Settings->bToWorldGrid; });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 810,
		TEXT("Upright"), LOCTEXT("UprightHotkey", "Upright Object (+Z Axis)"), LOCTEXT("UprightHotkey_Tooltip", "Apply minimal rotation so that Object +Z is aligned with World +Z"),
		EModifierKey::None, EKeys::Z, [this]() { OnQuickActionButtonClicked("SetUpright"); } );

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 811,
		TEXT("UprightNearest"), LOCTEXT("UprightNearestHotkey", "Upright Object (Nearest Axis)"), LOCTEXT("UprightHotkey_Tooltip", "Apply minimal rotation so that nearest Object Unit Axis is aligned with World +Z"),
		EModifierKey::Shift, EKeys::Z, [this]() { OnQuickActionButtonClicked("SetUprightNearest"); });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 820,
		TEXT("ResetRotation"), LOCTEXT("ResetRotationHotkey", "Reset Rotation"), LOCTEXT("ResetRotationHotkey_Tooltip", "Reset rotation on Object"),
		EModifierKey::Shift, EKeys::R, [this]() { OnQuickActionButtonClicked("ResetRotation"); });
}




void UGSMoverTool::OnTick(float DeltaTime)
{
	if (Settings->Mode_Selected == EGSMoverToolMode::Attached && LastHoveredActor != nullptr)
	{
		//UE_LOG(LogTemp, Warning, TEXT("FLIGHTCAM %s"), Local::IsFlightCameraActive() ? TEXT("ON") : TEXT("OFF"));

		bool bIsFlightCamActive = UGSModelingModeSubsystem::GetIsFlightCameraActive();
		if (bIsFlightCamActive != bWasFlightCameraActive)
		{
			if (bIsFlightCamActive == true)
			{
				GetToolManager()->BeginUndoTransaction(LOCTEXT("StartMove", "Move Actors"));
				LastHoveredActor->Modify();

				ActiveActors.Reset();
				ActiveActors.Add(LastHoveredActor);
				InitialTransforms.SetNum(ActiveActors.Num());
				InitialBounds.SetNum(ActiveActors.Num());
				CurTransforms.SetNum(ActiveActors.Num());
				for (int i = 0; i < ActiveActors.Num(); ++i) {
					InitialTransforms[i] = ActiveActors[i]->GetActorTransform();
					CurTransforms[i] = InitialTransforms[i];

					FVector Origin, Extent;
					ActiveActors[i]->GetActorBounds(false, Origin, Extent, true);
					InitialBounds[i] = FBox(Origin - Extent, Origin + Extent);
				}

				ActiveTransformMode = Settings->Mode_Selected;

				bInTransform = true;
			}
			else
			{
				ActiveTransformMode = EGSMoverToolMode::None;
				bInTransform = false;
				bInCtrlModifierForMode = false;
				bInShiftModifierForMode = false;

				GetToolManager()->EndUndoTransaction();
			}
			bWasFlightCameraActive = bIsFlightCamActive;
		}
		else if (bIsFlightCamActive && bWasFlightCameraActive)
		{
			if (bInTransform)
			{
				FFrame3d NewActorFrame = CurrentViewFrame.FromFrame(LastActorTransformInViewFrame);
				FTransform NewActorTransform = NewActorFrame.ToTransform();
				NewActorTransform.SetScale3D(InitialTransforms[0].GetScale3D());
				if (GrabSettings->bLockRotation)
					NewActorTransform.SetRotation(InitialTransforms[0].GetRotation());

				if (GrabSettings->AxisLock.bAxisX || GrabSettings->AxisLock.bAxisY || GrabSettings->AxisLock.bAxisZ)
				{
					FVector NewTranslation = NewActorTransform.GetTranslation();
					FVector OldTranslation = InitialTransforms[0].GetTranslation();
					NewTranslation.X = (GrabSettings->AxisLock.bAxisX) ? OldTranslation.X : NewTranslation.X;
					NewTranslation.Y = (GrabSettings->AxisLock.bAxisY) ? OldTranslation.Y : NewTranslation.Y;
					NewTranslation.Z = (GrabSettings->AxisLock.bAxisZ) ? OldTranslation.Z : NewTranslation.Z;
					NewActorTransform.SetTranslation(NewTranslation);
				}

				CurTransforms[0] = NewActorTransform;
				ActiveActors[0]->SetActorTransform(CurTransforms[0]);
				LastModifiedActors = ActiveActors;
			}
		}
	}
}

void UGSMoverTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	ViewState = RenderAPI->GetCameraState();
	Translater->UpdateCameraState(ViewState);
	FVector3d CameraRight = ViewState.Right(), CameraUp = ViewState.Up();
	CurrentViewFrame = FFrame3d(ViewState.Position, CameraRight, CameraUp, ViewState.Forward());

	FVector3d EyePos = ViewState.Position;
	auto DepthOffset = [&](const FVector3d& P, double dir = 1.0) { return P + dir * 0.001 * Normalized(P - EyePos); };
	bool bIsRotating = GetIsRotating();

	FToolDataVisualizer Draw;
	Draw.BeginFrame(RenderAPI);

	if (bInTransform) 
	{
		for (int i = 0; i < ActiveActors.Num(); ++i)
		{
			FVector3d WorldPos = InitialTransforms[i].GetLocation();
			FVector3d NewWorldPos = CurTransforms[i].GetLocation();
			FVector3d Delta = (NewWorldPos - WorldPos);
			if (bIsRotating == false)
			{
				if (Delta.GetAbsMax() > FMathf::ZeroTolerance) {
					Draw.DrawLine(DepthOffset(WorldPos), DepthOffset(NewWorldPos), LinearColors::LightGray3f(), 1.0f, false);
					Draw.DrawPoint(NewWorldPos, LinearColors::OrangeRed3f(), 5.0f, false);
					Draw.DrawPoint(WorldPos, LinearColors::VideoRed3f(), 10.0f, false);
				}
				else
					Draw.DrawPoint(WorldPos, LinearColors::VideoRed3f(), 5.0f, false);
			}
			else
			{
				FVector ActorPivotPos = GetRotationOrigin(i);

				bool bDrawLeftRight = (ActiveTransformMode == EGSMoverToolMode::Vertical) 
					|| (ActiveTransformMode == EGSMoverToolMode::XYPlane && bShiftDown == false)
					|| (ActiveTransformMode == EGSMoverToolMode::ScreenSpace && bShiftDown == false);
				bool bDrawUpDown = (ActiveTransformMode == EGSMoverToolMode::XYPlane && bShiftDown == true)
					|| (ActiveTransformMode == EGSMoverToolMode::ScreenSpace && bShiftDown == true);

				if (bDrawLeftRight)
				{
					Draw.DrawLine(ActorPivotPos - 10000 * CameraRight, ActorPivotPos + 10000 * CameraRight, LinearColors::LightGray3f(), 1.0f, false);
					if (LastRotationWorldDelta.X != 0) {
						FVector EndPoint = ActorPivotPos + LastRotationWorldDelta.X * CameraRight;
						Draw.DrawLine(ActorPivotPos, EndPoint, LinearColors::LightGray3f(), 3.0f, false);
						Draw.DrawPoint(EndPoint, LinearColors::OrangeRed3f(), 5.0f, false);
					}
				}
				if (bDrawUpDown)
				{
					Draw.DrawLine(ActorPivotPos - 10000 * CameraUp, ActorPivotPos + 10000 * CameraUp, LinearColors::LightGray3f(), 1.0f, false);
					if (LastRotationWorldDelta.Y != 0) {
						FVector EndPoint = ActorPivotPos + LastRotationWorldDelta.Y * CameraUp;
						Draw.DrawLine(ActorPivotPos, EndPoint, LinearColors::LightGray3f(), 3.0f, false);
						Draw.DrawPoint(EndPoint, LinearColors::OrangeRed3f(), 5.0f, false);
					}
				}

				Draw.DrawPoint(ActorPivotPos, LinearColors::VideoRed3f(), 5.0f, false);
				double CircleRad = ToolSceneQueriesUtil::CalculateDimensionFromVisualAngleD(ViewState, ActorPivotPos, 0.75f);
				Draw.DrawViewFacingCircle(ActorPivotPos, CircleRad, 16, LinearColors::VideoBlack3f(), 1.0f, false);
			}

		}

		if (bIsRotating)
		{
			Draw.DrawViewFacingCircle(CurRotationSphereCenter, CurRotationSphereRadius, 64, FLinearColor::Black, 1.0f, false);
		}

		// darw last world-hit position if it is relevant
		if (  ActiveTransformMode == EGSMoverToolMode::Environment || 
			 (ActiveTransformMode == EGSMoverToolMode::Point && (bInCtrlModifierForMode || bCtrlDown)) )
		{
			Draw.DrawPoint(LastWorldHitPos, LinearColors::VideoBlue3f(), 10.0f, false);
		}

		if (bIsOverlapping)
		{
			Draw.DrawWireBox(OverlapHitBox, LinearColors::VideoRed3f(), 2.0f, true);
		}
	}

	if (LastHoveredActor != nullptr)
	{
		bool bIsMovable = IsActorMovable(LastHoveredActor);
		FLinearColor DrawBoxColor = bIsMovable ? LinearColors::Black3f() : LinearColors::VideoRed3f();
		float Width = (bIsMovable) ? 1.0f : 0.5f;
		Draw.DrawWireBox(LastHoveredActorBox, DrawBoxColor, Width, true);

		if (bIsMovable && !bInTransform) {
			FVector ActorPivotPos = GetRotationOrigin(LastHoveredActor);
			Draw.DrawPoint(ActorPivotPos, LinearColors::VideoRed3f(), 5.0f, false);
			if (bIsRotating)
			{
				double CircleRad = ToolSceneQueriesUtil::CalculateDimensionFromVisualAngleD(ViewState, ActorPivotPos, 0.75f);
				Draw.DrawViewFacingCircle(ActorPivotPos, CircleRad, 16, LinearColors::VideoBlack3f(), 1.0f, false);
			}
		}
	}

	Draw.EndFrame();

	if (Settings->Mode_Selected == EGSMoverToolMode::SmartAxis) {
		if (bInTransform)
			Translater->Render(RenderAPI);
		else
			Translater->PreviewRender(RenderAPI);
	}
}


struct float_string
{
	float_string(double d, int NumDecimals=2, bool bRound=true)
	{
		char digit[32];

		if (bRound)
		{
			// determine number of leading and trailing decimals, rounding to NumDecimals ( from https://stackoverflow.com/questions/277772/avoid-trailing-zeroes-in-printf)
			int sz; double d2;
			d2 = (d >= 0) ? d : -d;				// Allow for negative.
			sz = (d >= 0) ? 0 : 1;
			if (d2 < 1) sz++;					// Add one for each whole digit (0.xx special case).
			while (d2 >= 1) { d2 /= 10.0; sz++; }
			sz += 1 + NumDecimals;				// Adjust for decimal point and fractionals.

			sprintf_s<32>(digit, "%*.*f", sz, NumDecimals, d);
		}
		else
			sprintf_s<32>(digit, "%.10f", d);  // Make the number.

		truncateTrailingZeros(digit, NumDecimals);
		str = FString(ANSI_TO_TCHAR(digit));
	}

	FString str;
	const TCHAR* operator*() { return *str; }

	// from https://stackoverflow.com/questions/277772/avoid-trailing-zeroes-in-printf
	void truncateTrailingZeros(char* s, int n) {
		char* p;
		int count;

		p = strchr(s, '.');         // Find decimal point, if any.
		if (p != NULL) {
			count = n;              // Adjust for more or less decimals.
			while (count >= 0) {    // Maximum decimals allowed.
				count--;
				if (*p == '\0')    // If there's less than desired.
					break;
				p++;               // Next character.
			}

			*p-- = '\0';            // Truncate string.
			while (*p == '0')       // Remove trailing zeros.
				*p-- = '\0';

			if (*p == '.') {        // If all decimals were zeros, remove ".".
				*p = '\0';
			}
		}
	}
};



void UGSMoverTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	const FSceneView* SceneView = RenderAPI->GetSceneView();
	LastViewInfo.Initialize(RenderAPI->GetSceneView());

	if (!bInTransform) return;
	if (Settings->ShowText == EGSMoverToolTextDisplayMode::None) return;

	bool bIsRotating = GetIsRotating();
	if (bIsRotating) return;

	double DPIScale = (double)Canvas->GetDPIScale();
	UFont* UseFont = GEngine->GetSmallFont();
	FViewCameraState CamState = RenderAPI->GetCameraState();
	//FVector3d LocalEyePosition(CurTargetTransform.InverseTransformPosition((FVector3d)CamState.Position));

	FVector2d TextAlignRightOffset(10.0 / DPIScale, -15.0 / DPIScale);

	for (int i = 0; i < ActiveActors.Num(); ++i)
	{
		FVector3d WorldPos = InitialTransforms[i].GetLocation();
		FVector2d InitialPixelPos;
		SceneView->WorldToPixel(WorldPos, InitialPixelPos); InitialPixelPos += TextAlignRightOffset;
		FString CursorString = FString::Printf(TEXT("(%s, %s, %s)"), *float_string(WorldPos.X), *float_string(WorldPos.Y), *float_string(WorldPos.Z));
		Canvas->DrawShadowedString(InitialPixelPos.X / DPIScale, InitialPixelPos.Y / DPIScale, *CursorString, UseFont, FLinearColor::Black);
		Canvas->DrawShadowedString((InitialPixelPos.X-1) / DPIScale, (InitialPixelPos.Y-1) / DPIScale, *CursorString, UseFont, FLinearColor::White);

		FVector3d NewWorldPos = CurTransforms[i].GetLocation();
		FVector3d Delta = NewWorldPos - WorldPos;
		if (Delta.GetAbsMax() > FMathf::ZeroTolerance) {
			FVector2d NewPixelPos;
			SceneView->WorldToPixel(NewWorldPos, NewPixelPos); NewPixelPos += TextAlignRightOffset;

			FVector3d ShowDestVec = (Settings->ShowText == EGSMoverToolTextDisplayMode::Delta) ? Delta : NewWorldPos;

			if (Settings->ShowText == EGSMoverToolTextDisplayMode::Delta && Settings->Mode_Selected == EGSMoverToolMode::ScreenSpace) {
				ShowDestVec = CurrentViewFrame.Rotation.InverseMultiply(Delta);
			}

			CursorString = FString::Printf(TEXT("(%s, %s, %s)"), *float_string(ShowDestVec.X), *float_string(ShowDestVec.Y), *float_string(ShowDestVec.Z));
			Canvas->DrawShadowedString(NewPixelPos.X / DPIScale, NewPixelPos.Y / DPIScale, *CursorString, UseFont, FLinearColor::Black);
			Canvas->DrawShadowedString((NewPixelPos.X - 1) / DPIScale, (NewPixelPos.Y - 1) / DPIScale, *CursorString, UseFont, FLinearColor::White);
		}
	}

}


static bool IsLandscapeActor(const AActor* Actor)
{
	return Actor->IsA(ALandscape::StaticClass())
		|| Actor->IsA(ALandscapeProxy::StaticClass());
}


AActor* UGSMoverTool::FindHitActor(FRay WorldRay, double& Distance, FVector& HitNormal) const
{
	FVector Start = WorldRay.Origin;
	FVector End = WorldRay.PointAt(TNumericLimits<float>::Max());

	// This is pretty dumb...there is no way to ignore hidden objects in LineTrace!
	// So we have to do a trace, and if the hit object is hidden, add it to the ignore list
	// and run the trace again!!

	ECollisionChannel Channels = ECollisionChannel::ECC_Visibility;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;

	bool bDone = false;
	while (!bDone)
	{
		FHitResult OutHit;
		if (TargetWorld->LineTraceSingleByChannel(OutHit, Start, End, Channels, QueryParams))
		{
			AActor* HitActor = OutHit.GetActor();
			if (!HitActor) { bDone = true; continue; }

			bool bIgnoreHit = IsLandscapeActor(HitActor);
			bIgnoreHit = bIgnoreHit || (HitActor->IsHidden() || HitActor->IsHiddenEd());
			//APartitionActor ?

			if (bIgnoreHit) {
				QueryParams.AddIgnoredActor(HitActor);
				continue;
			}

			// got valid hit
			Distance = OutHit.Distance;
			HitNormal = OutHit.Normal;
			return HitActor;
		} else
			bDone = true;
	}

	return nullptr;
}
AActor* UGSMoverTool::FindHitActor(FRay WorldRay, double& Distance) const
{
	FVector Normal;
	return FindHitActor(WorldRay, Distance, Normal);
}


bool UGSMoverTool::FindEnvironmentOrGroundHit(FRay WorldRay, double& Distance, FVector& HitNormal, bool bFallbackToGroundPlane) const
{
	AActor* HitActor = FindEnvironmentHit(WorldRay, Distance, HitNormal);
	if (HitActor != nullptr)
		return true;

	if (bFallbackToGroundPlane)
	{
		FVector HitPoint;
		FFrame3d GroundPlane;
		if (GroundPlane.RayPlaneIntersection(WorldRay.Origin, WorldRay.Direction, 2, HitPoint)) {
			Distance = WorldRay.GetParameter(HitPoint);
			HitNormal = FVector3d::UnitZ();
			return true;
		}
	}

	return false;
}


AActor* UGSMoverTool::FindEnvironmentHit(FRay WorldRay, double& Distance, FVector& HitNormal) const
{
	FVector Start = WorldRay.Origin;
	FVector End = WorldRay.PointAt(TNumericLimits<float>::Max());

	FCollisionQueryParams QueryParams;
	QueryParams.MobilityType = EQueryMobilityType::Static;

	AActor* HitActor = nullptr;
	double MinHitDist = TNumericLimits<double>::Max();
	FVector MinHitNormal = FVector::UnitZ();

	if (EnvironmentSettings->HitType == EEnvironmentPlacementType::LandscapeOnly)
	{
		for (TActorIterator<ALandscape> LandscapeIterator(TargetWorld.Get()); LandscapeIterator; ++LandscapeIterator) {
			ALandscape* Landscape = *LandscapeIterator;
			FHitResult HitResult;
			if (Landscape->ActorLineTraceSingle(HitResult, Start, End, ECollisionChannel::ECC_WorldStatic, QueryParams)) {
				if (HitResult.Distance < MinHitDist) {
					HitActor = Landscape;
					MinHitDist = HitResult.Distance;
					MinHitNormal = HitResult.Normal;
				}
			}
		}
		// also try any landscape proxies (in WP world landscape will be split into proxies?)
		for (TActorIterator<ALandscapeProxy> LandscapeProxyIterator(TargetWorld.Get()); LandscapeProxyIterator; ++LandscapeProxyIterator) {
			ALandscapeProxy* Landscape = *LandscapeProxyIterator;
			FHitResult HitResult;
			if (Landscape->ActorLineTraceSingle(HitResult, Start, End, ECollisionChannel::ECC_WorldStatic, QueryParams)) {
				if (HitResult.Distance < MinHitDist) {
					HitActor = Landscape;
					MinHitDist = HitResult.Distance;
					MinHitNormal = HitResult.Normal;
				}
			}
		}


	}
	else 
	{
		ECollisionChannel UseChannel = ECollisionChannel::ECC_WorldStatic;
		if (EnvironmentSettings->HitType == EEnvironmentPlacementType::Any) {
			UseChannel = ECollisionChannel::ECC_Visibility;
			QueryParams.MobilityType = EQueryMobilityType::Any;		// doesn't seem to work?
		}

		// todo visibility??

		QueryParams.AddIgnoredActors(ActiveActors);
		FHitResult HitResult;
		if (TargetWorld->LineTraceSingleByChannel(HitResult, Start, End, UseChannel, QueryParams)) {
			if (HitResult.Distance < MinHitDist ) {
				HitActor = HitResult.GetActor();
				MinHitDist = HitResult.Distance;
				MinHitNormal = HitResult.Normal;
			}
		}
	}

	if (HitActor != nullptr) {
		Distance = MinHitDist;
		HitNormal = MinHitNormal;
	}
	return HitActor;
}


bool UGSMoverTool::IsActorMovable(const AActor* Actor) const
{
	FVector3d Origin, Extent;
	Actor->GetActorBounds(false, Origin, Extent, true);
	FAxisAlignedBox3d ActorBox(Origin - Extent, Origin + Extent);

	// if we just moved this actor we have to keep it movable
	if (LastModifiedActors.Contains(Actor))
		return true;

	// current criteria: must be able to see box center and at least two corners

	if (LastViewInfo.IsVisible(ActorBox.Center()) == false)
		return false;

	int VisibleCorners = 0;
	for (int i = 0; i < 8; ++i)
	{
		FVector2d c = LastViewInfo.WorldToPixel(ActorBox.GetCorner(i));
		if (LastViewInfo.IsVisible(c))
			VisibleCorners++;
	}
	return (VisibleCorners >= Settings->VisibilityFiltering);
}


FInputRayHit UGSMoverTool::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	return FInputRayHit();
}
void UGSMoverTool::OnClicked(const FInputDeviceRay& ClickPos)
{
}


FInputRayHit UGSMoverTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	double HitDist;

	AActor* HitActor = FindHitActor(PressPos.WorldRay, HitDist);
	if ( HitActor != nullptr && IsActorMovable(HitActor) )
		return FInputRayHit(HitDist);
	return FInputRayHit();
}

void UGSMoverTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	LastHoveredActor = nullptr;
	bInTransform = false;
	GetToolManager()->BeginUndoTransaction(LOCTEXT("StartMove", "Move Actors"));

	ActiveActors.Reset();

	double HitDist;
	AActor* HitActor = FindHitActor(PressPos.WorldRay, HitDist);
	if (HitActor == nullptr) return;

	HitActor->Modify();

	ActiveActors.Add(HitActor);
	InitialTransforms.SetNum(ActiveActors.Num());
	InitialBounds.SetNum(ActiveActors.Num());
	CurTransforms.SetNum(ActiveActors.Num());
	for (int i = 0; i < ActiveActors.Num(); ++i) {
		InitialTransforms[i] = ActiveActors[i]->GetActorTransform();
		CurTransforms[i] = InitialTransforms[i];

		FVector Origin, Extent;
		ActiveActors[i]->GetActorBounds(false, Origin, Extent, true);
		InitialBounds[i] = FBox(Origin - Extent, Origin + Extent);
	}

	ActiveTransformMode = Settings->Mode_Selected;
	bInCtrlModifierForMode = bCtrlDown;
	bInShiftModifierForMode = bShiftDown;

	StartPosRay = PressPos.WorldRay;
	StartHitPos = StartPosRay.PointAt(HitDist);
	StartRelativeOffset = FVector::Zero();
	LastMoveDelta = FVector3d::Zero();
	ActiveTransformNudge = FVector3d::Zero();
	bInTransform = true;
	bIsOverlapping = false;
	InitializeTransformMode();
}

void UGSMoverTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	if (bInTransform)
		UpdateTransformsForMode(DragPos);
}

void UGSMoverTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	GetToolManager()->EndUndoTransaction();

	// no point in doing this because we are doing a full rebuild right now
	//if (bInTransform && CollisionScene.IsValid()) {
	//	CollisionScene->FastUpdateTransformsForActor(ActiveActors[0]);
	//}

	bInTransform = false;
	Translater->Reset();
	ActiveTransformMode = EGSMoverToolMode::None;
}

void UGSMoverTool::OnTerminateDragSequence()
{
	SetCurrentMoveDelta(FVector3d::Zero(), true, true, true);		// restore positions
	GetToolManager()->EndUndoTransaction();
	bInTransform = false;
	ActiveTransformMode = EGSMoverToolMode::None;
}





void UGSMoverTool::UpdateHoveredActor(const FRay& WorldRay)
{
	LastHoveredActor = nullptr;

	// todo only move selected actor? use clutch key?
	double HitDist; FVector HitNormal;
	AActor* HitActor = FindHitActor(WorldRay, HitDist, HitNormal);
	if (HitActor != nullptr) {
		LastHoveredActor = HitActor;
		FVector Center, Extent;
		LastHoveredActorTransform = LastHoveredActor->GetActorTransform();
		LastHoveredActor->GetActorBounds(false, Center, Extent);
		LastHoveredActorBox = FBox(Center - Extent, Center + Extent);
		LastHoverHitPoint = WorldRay.PointAt(HitDist);
		LastHoverHitNormal = HitNormal;
		FFrame3d CurTransform = FFrame3d(HitActor->GetActorTransform());
		LastActorTransformInViewFrame = CurrentViewFrame.ToFrame(CurTransform);
	}
}

FInputRayHit UGSMoverTool::BeginHoverSequenceHitTest(const FInputDeviceRay& DevicePos)
{
	UpdateHoveredActor(DevicePos.WorldRay);
	if (LastHoveredActor != nullptr)
		return FInputRayHit(DevicePos.WorldRay.GetParameter(LastHoverHitPoint));
	return FInputRayHit();
}

void UGSMoverTool::OnBeginHover(const FInputDeviceRay& DevicePos)
{
	UpdateHoveredActor(DevicePos.WorldRay);
}
bool UGSMoverTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	UpdateHoveredActor(DevicePos.WorldRay);

	if ( Settings->Mode_Selected == EGSMoverToolMode::SmartAxis && SmartAxisSettings->bAlignToNormal )
		Translater->SetActiveFrameFromWorldNormal(LastHoverHitPoint, LastHoverHitNormal, true);
	else
		Translater->SetActiveFrameFromWorldAxes(LastHoverHitPoint);

	return true;
}
void UGSMoverTool::OnEndHover()
{
	if (Settings->Mode_Selected != EGSMoverToolMode::Attached)
		LastHoveredActor = nullptr;
}

void UGSMoverTool::OnMouseWheelScroll(float WheelDelta)
{
	float S = Settings->NudgeSize * Settings->Gain;
	if (bInTransform && Settings->Mode_Selected == EGSMoverToolMode::ScreenSpace)
	{
		UpdateCurrentMoveNudge( CurrentViewFrame.Z() * WheelDelta * S );
	}
	else if (bInTransform && Settings->Mode_Selected == EGSMoverToolMode::Environment)
	{
		UpdateCurrentMoveNudge(FVector::UnitZ() * WheelDelta * S);
	}
}


void UGSMoverTool::OnUpdateModifierState(int ModifierID, bool bIsOn)
{
	switch (ModifierID)	{
	case UGSMoverToolBuilder_ShiftModifierID:
		bShiftDown = bIsOn;
		break;
	case UGSMoverToolBuilder_CtrlModifierID:
		bCtrlDown = bIsOn;
		break;
	default:
		break;
	}
}



void UGSMoverTool::InitializeTransformMode()
{
	//if ( (ActiveTransformMode == EGSMoverToolMode::ScreenSpace) || (ActiveTransformMode == EGSMoverToolMode::Tumble) || (ActiveTransformMode == EGSMoverToolMode::Point) )
	if ( GetIsRotating() )
	{
		CurRotationSphereRadius = ToolSceneQueriesUtil::CalculateDimensionFromVisualAngleD(ViewState, StartHitPos, 20.0f);
		CurRotationSphereCenter = StartHitPos + CurRotationSphereRadius * StartPosRay.Direction;
		AccumRotation = FQuaterniond::Identity();
		LastWorldHitPos = FVector3d::Zero();
	}

	if (ActiveTransformMode == EGSMoverToolMode::Environment)
	{
		if (EnvironmentSettings->Location == EEnvironmentPlacementLocation::ObjectPivot) {
			StartRelativeOffset = InitialTransforms[0].GetLocation() - StartHitPos;
		} else if (EnvironmentSettings->Location == EEnvironmentPlacementLocation::BoundsBase) {
			FVector3d PivotPos = InitialBounds[0].GetCenter();
			PivotPos.Z = InitialBounds[0].Min.Z;
			StartRelativeOffset = PivotPos - StartHitPos;
		} else if (EnvironmentSettings->Location == EEnvironmentPlacementLocation::BoundsCenter) {
			StartRelativeOffset = InitialBounds[0].GetCenter() - StartHitPos;
		} else if (EnvironmentSettings->Location == EEnvironmentPlacementLocation::HitHeight) {
			double DistToGround; FVector Normal;
			if ( FindEnvironmentOrGroundHit(FRay(StartHitPos, -FVector::UnitZ()), DistToGround, Normal, (EnvironmentSettings->HitType == EEnvironmentPlacementType::Any) ) )
				StartRelativeOffset = FVector(0, 0, -DistToGround);
		}
	}

	if (Settings->bCollisionTest) {
		UpdateMeshCollisionScene();
	}
}



void UGSMoverTool::UpdateTransformsForMode(const FInputDeviceRay& DragPos)
{
	LastRotationWorldDelta = FVector2d::Zero();

	switch (ActiveTransformMode)
	{
	case EGSMoverToolMode::Vertical: UpdateTransformsForMode_Vertical(DragPos); break;
	case EGSMoverToolMode::XYPlane: UpdateTransformsForMode_XYPlane(DragPos); break;
	case EGSMoverToolMode::ScreenSpace: UpdateTransformsForMode_ScreenSpace(DragPos); break;
	case EGSMoverToolMode::Environment: UpdateTransformsForMode_Environment(DragPos); break;
	case EGSMoverToolMode::SmartAxis: UpdateTransformsForMode_SmartAxis(DragPos); break;
	case EGSMoverToolMode::Tumble: UpdateTransformsForMode_Tumble(DragPos); break;
	case EGSMoverToolMode::Point: UpdateTransformsForMode_Point(DragPos); break;
	}
}


void UGSMoverTool::UpdateTransformsForMode_Vertical(const FInputDeviceRay& DragPos)
{
	if (bInCtrlModifierForMode)
	{
		FFrame3d HitPlane(StartHitPos, ViewState.Forward());
		FVector HitPoint;
		if (HitPlane.RayPlaneIntersection(DragPos.WorldRay.Origin, DragPos.WorldRay.Direction, 2, HitPoint))
		{
			FVector2d Start2D = LastViewInfo.WorldToPixel(StartHitPos);
			FVector2d Cur2D = LastViewInfo.WorldToPixel(HitPoint);
			SetCurrentRotateDelta(FVector3d::UnitZ(), -(Cur2D.X-Start2D.X) / 10);

			FLine3d dx(StartHitPos, ViewState.Right());
			LastRotationWorldDelta.X = dx.Project(HitPoint) - dx.Project(StartHitPos);
		}
	}
	else
	{
		FLine3d VerticalLine(StartHitPos, FVector::UnitZ());
		FDistLine3Ray3d Dist(VerticalLine, DragPos.WorldRay);
		Dist.Get();
		FVector LinePoint = Dist.LineClosestPoint;
		FVector MoveDelta = (LinePoint - StartHitPos);
		SetCurrentMoveDelta(MoveDelta, true, true, false);
	}
}

void UGSMoverTool::UpdateTransformsForMode_XYPlane(const FInputDeviceRay& DragPos)
{
	if (bInCtrlModifierForMode)
	{
		FFrame3d HitPlane(StartHitPos, ViewState.Forward());
		FVector HitPoint;
		if (HitPlane.RayPlaneIntersection(DragPos.WorldRay.Origin, DragPos.WorldRay.Direction, 2, HitPoint))
		{
			FVector2d Start2D = LastViewInfo.WorldToPixel(StartHitPos);
			FVector2d Cur2D = LastViewInfo.WorldToPixel(HitPoint);
			if (bShiftDown) 
			{
				SetCurrentRotateDelta(ViewState.Right(), -(Cur2D.Y - Start2D.Y) / 10);	// tilt in/out of screen
				FLine3d dy(StartHitPos, ViewState.Up());
				LastRotationWorldDelta.Y = dy.Project(HitPoint) - dy.Project(StartHitPos);
			}
			else {
				SetCurrentRotateDelta(FVector3d::UnitZ(), -(Cur2D.X - Start2D.X) / 10);		// vertical rotation
				FLine3d dx(StartHitPos, ViewState.Right());
				LastRotationWorldDelta.X = dx.Project(HitPoint) - dx.Project(StartHitPos);
			}
		}
	} 
	else
	{
		FFrame3d XYPlane(StartHitPos, FVector::UnitZ());
		FVector3d PlaneHitPoint;
		if (!XYPlane.RayPlaneIntersection(DragPos.WorldRay.Origin, DragPos.WorldRay.Direction, 2, PlaneHitPoint))
			return;
		FVector MoveDelta = PlaneHitPoint - StartHitPos;

		SetCurrentMoveDelta(MoveDelta, false, false, true);
	}
}


void UGSMoverTool::UpdateTransformsForMode_ScreenSpace(const FInputDeviceRay& DragPos)
{
	if (bInCtrlModifierForMode)
	{
		FFrame3d HitPlane(StartHitPos, ViewState.Forward());
		FVector HitPoint;
		if (HitPlane.RayPlaneIntersection(DragPos.WorldRay.Origin, DragPos.WorldRay.Direction, 2, HitPoint))
		{
			FVector2d Start2D = LastViewInfo.WorldToPixel(StartHitPos);
			FVector2d Cur2D = LastViewInfo.WorldToPixel(HitPoint);
			if (bShiftDown) 
			{
				SetCurrentRotateDelta(ViewState.Right(), -(Cur2D.Y - Start2D.Y) / 10);	// tilt in/out of screen
				FLine3d dy(StartHitPos, ViewState.Up());
				LastRotationWorldDelta.Y = dy.Project(HitPoint) - dy.Project(StartHitPos);
			}
			else
			{
				SetCurrentRotateDelta(ViewState.Forward(), -(Cur2D.X - Start2D.X) / 10);  // rotate around eye ray
				FLine3d dx(StartHitPos, ViewState.Right());
				LastRotationWorldDelta.X = dx.Project(HitPoint) - dx.Project(StartHitPos);
			}
		}
	}
	else
	{
		FFrame3d ScreenPlane(StartHitPos, CurrentViewFrame.Rotation);
		FVector3d PlaneHitPoint;
		if (!ScreenPlane.RayPlaneIntersection(DragPos.WorldRay.Origin, DragPos.WorldRay.Direction, 2, PlaneHitPoint))
			return;

		FVector MoveDelta = FVector::Zero();
		if (bInShiftModifierForMode)
		{
			double dy = (PlaneHitPoint - StartHitPos).Dot(ScreenPlane.Y());
			MoveDelta = dy * CurrentViewFrame.Z();
		}
		else
		{
			MoveDelta = PlaneHitPoint - StartHitPos;
		}

		SetCurrentMoveDelta(MoveDelta, false, false, false);
	}
}


namespace GS {


struct ConvexIntersection
{
	bool bIntersects = false;
	int NumIntersections = 0;       // 0, 1, or 2
	double MinParameter = 0;
	double MaxParameter = 0;
	
	static ConvexIntersection NoIntersection() { return ConvexIntersection{ false,0,0,0 }; }
};

bool TestRaySphereIntersection(
	const GS::Vector3d& RayOrigin,
	const GS::Vector3d& RayDirection,
	const GS::Vector3d& SphereCenter,
	double SphereRadius)
{
	// adapted from GeometricTools GTEngine file IntrRay3Sphere3.h (boost license - https://www.geometrictools.com)
	Vector3d diff = RayOrigin - SphereCenter;
	double a0 = diff.SquaredLength() - (SphereRadius * SphereRadius);
	if (a0 <= (double)0)	// origin is inside the sphere.
		return true;

	double a1 = RayDirection.Dot(diff);
	if (a1 >= (double)0)
		return false;

	// Intersection occurs when Q(t) has real roots.
	double discr = a1 * a1 - a0;
	return (discr >= (double)0);
}

ConvexIntersection FindLineSphereIntersection(
	const GS::Vector3d& LineOrigin,
	const GS::Vector3d& LineDirection,
	const GS::Vector3d& SphereCenter,
	double SphereRadius)
{
	Vector3d diff = LineOrigin - SphereCenter;
	double a0 = Dot(diff, diff) - SphereRadius * SphereRadius;
	double a1 = Dot(LineDirection, diff);

	// Intersection occurs when Q(t) has real roots.
	double const zero = static_cast<double>(0);
	double discr = a1 * a1 - a0;
	if (discr > zero)
	{
		// The line intersects the sphere in 2 distinct points.
		double root = GS::Sqrt(discr);
		return ConvexIntersection{true, 2, -a1-root, -a1 + root };
	}
	else if (discr == zero)
	{
		// The line is tangent to the sphere, so the intersection is
		// a single point. The parameter[1] value is set, because
		// callers will access the degenerate interval [-a1,-a1].
		return ConvexIntersection{true, 1, -a1, -a1};
	}
	else
	{
		// else:  The line is outside the sphere, no intersection.
		return ConvexIntersection::NoIntersection();
	}
}


ConvexIntersection FindRaySphereIntersection(
	const GS::Vector3d& RayOrigin,
	const GS::Vector3d& RayDirection,
	const GS::Vector3d& SphereCenter,
	double SphereRadius)
{
	ConvexIntersection LineResult = FindLineSphereIntersection(RayOrigin, RayDirection, SphereCenter, SphereRadius);
	if (LineResult.bIntersects)
	{
		// The line containing the ray intersects the sphere; the
		// t-interval is [t0,t1]. The ray intersects the sphere as
		// long as [t0,t1] overlaps the ray t-interval [0,+infinity).
		if (LineResult.MaxParameter < 0)
			return ConvexIntersection::NoIntersection();
		if (LineResult.MinParameter < 0)
			return ConvexIntersection{ true, 1, LineResult.MaxParameter, 0 };
	}
	return LineResult;
}




}  // end namespace GS


void UGSMoverTool::UpdateTransformsForMode_Tumble(const FInputDeviceRay& DragPos)
{
	//if (bInShiftModifierForMode)
	//{
	//	// this is an abomination....horribly janky arcball-with-tumble...
	//	FFrame3d HitPlane(StartHitPos, ViewState.Forward());
	//	FVector PlaneHitPoint;
	//	HitPlane.RayPlaneIntersection(DragPos.WorldRay.Origin, DragPos.WorldRay.Direction, 2, PlaneHitPoint);
	//	if (LastWorldHitPos == FVector3d::Zero()) {
	//		LastWorldHitPos = PlaneHitPoint;
	//		return;
	//	}
	//	double dx = (PlaneHitPoint - LastWorldHitPos).Dot(ViewState.Right());
	//	double dy = (PlaneHitPoint - LastWorldHitPos).Dot(ViewState.Up());
	//	FQuaterniond RotateX(ViewState.Up(), -dx / CurRotationSphereRadius * 30.0f, true);
	//	FQuaterniond RotateY(ViewState.Right(), dy / CurRotationSphereRadius * 30.0f, true);
	//	AccumRotation = AccumRotation * RotateX * RotateY;
	//	SetCurrentRotateDelta(AccumRotation);
	//	LastWorldHitPos = PlaneHitPoint;
	//}
	//else
	//{
		FVector ToEyeDir = Normalized(ViewState.Position - CurRotationSphereCenter);
		FFrame3d HitPlane(CurRotationSphereCenter + 0.5*CurRotationSphereRadius*ToEyeDir, ToEyeDir);
		FVector PlaneHitPoint;
		HitPlane.RayPlaneIntersection(DragPos.WorldRay.Origin, DragPos.WorldRay.Direction, 2, PlaneHitPoint);
		FVector3d Direction = PlaneHitPoint - CurRotationSphereCenter;
		double Distance = Normalize(Direction);
		FQuaterniond FromToRotate(-StartPosRay.Direction, Direction);
		double scale = Distance / CurRotationSphereRadius;
		GS::Vector3d Axis; double Angle;
		((GS::Quaterniond)FromToRotate).ExtractAxisAngleRad(Axis, Angle);
		Angle *= scale;
		FQuaterniond ScaledRotation((FVector)Axis, Angle, false);
		SetCurrentRotateDelta(ScaledRotation);
	//}
}



void UGSMoverTool::UpdateTransformsForMode_Point(const FInputDeviceRay& DragPos)
{
	FFrame3d HitPlane(StartHitPos, ViewState.Forward());
	//FVector3d Origin = InitialTransforms[0].GetLocation();
	FVector3d Origin = GetRotationOrigin(0);

	// if we do this then we basically can only rotate around origin perp to screen...
	if (bInShiftModifierForMode)
		Origin = HitPlane.ToPlane(Origin, 2);

	if (bInCtrlModifierForMode)
	{
		FCollisionQueryParams QueryParams;
		QueryParams.MobilityType = EQueryMobilityType::Any;
		ECollisionChannel UseChannel = ECollisionChannel::ECC_Visibility;
		QueryParams.AddIgnoredActors(ActiveActors);
		FHitResult HitResult;
		if (TargetWorld->LineTraceSingleByChannel(HitResult, DragPos.WorldRay.Origin, DragPos.WorldRay.Origin + 99999 * DragPos.WorldRay.Direction, UseChannel, QueryParams)) {
			FVector3d SourceDir = Normalized(StartHitPos - Origin);
			FVector3d TargetDir = Normalized(HitResult.Location - Origin);
			FQuaterniond FromToRotate(SourceDir, TargetDir);
			SetCurrentRotateDelta(FromToRotate);
			LastWorldHitPos = HitResult.Location;
		}
	}
	else
	{
		FVector HitPoint;
		if (HitPlane.RayPlaneIntersection(DragPos.WorldRay.Origin, DragPos.WorldRay.Direction, 2, HitPoint))
		{
			FVector3d SourceDir = Normalized(StartHitPos - Origin);
			FVector3d TargetDir = Normalized(HitPoint - Origin);
			FQuaterniond FromToRotate(SourceDir, TargetDir);
			SetCurrentRotateDelta(FromToRotate);
			LastWorldHitPos = HitPoint;
		}
	}

}



void UGSMoverTool::UpdateTransformsForMode_SmartAxis(const FInputDeviceRay& DragPos)
{
	//FVector NewHitPosWorld;
	FVector3d SnappedPoint;
	if (Translater->UpdateSnap(DragPos.WorldRay, SnappedPoint, nullptr))
	{
		if (bInCtrlModifierForMode)
		{
			// some kinda rotate...
		}
		else
		{
			FVector MoveDelta = SnappedPoint - StartHitPos;
			SetCurrentMoveDelta(MoveDelta, false, false, false);
		}
	}
}


void UGSMoverTool::UpdateTransformsForMode_Environment(const FInputDeviceRay& DragPos)
{
	double HitDist; FVector HitNormal; 
	bool bEnvironmentHit = FindEnvironmentOrGroundHit(DragPos.WorldRay, HitDist, HitNormal, (EnvironmentSettings->HitType == EEnvironmentPlacementType::Any) );
	if (bEnvironmentHit)
	{
		FVector HitPos = DragPos.WorldRay.PointAt(HitDist);
		LastWorldHitPos = HitPos;
		FVector MoveDelta = HitPos - StartHitPos;
		MoveDelta -= StartRelativeOffset;

		if (EnvironmentSettings->AxisLock.bAxisX)
			MoveDelta.X = 0;
		if (EnvironmentSettings->AxisLock.bAxisY)
			MoveDelta.Y = 0;
		if (EnvironmentSettings->AxisLock.bAxisZ)
			MoveDelta.Z = 0;

		SetCurrentMoveDelta(MoveDelta, false, false, false);
	}
}


void UGSMoverTool::SetCurrentMoveDelta(const FVector3d& WorldDelta, bool bPreserveX, bool bPreserveY, bool bPreserveZ)
{
	LastMoveDelta = WorldDelta * Settings->Gain;

	FToolContextSnappingConfiguration SnapConfig = GetToolManager()->GetContextQueriesAPI()->GetCurrentSnappingSettings();
	bool bEnableSnapping = false;
	if (Settings->Snapping == EGSMoverToolSnapMode::Editor)
		bEnableSnapping = SnapConfig.bEnablePositionGridSnapping;
	else if (Settings->Snapping == EGSMoverToolSnapMode::Override)
		bEnableSnapping = true;
	USceneSnappingManager* SnapManager = (bEnableSnapping) ? USceneSnappingManager::Find(GetToolManager()) : nullptr;

	bool bRelativeSnap = !(Settings->bToWorldGrid);
	if (Settings->Mode_Selected == EGSMoverToolMode::ScreenSpace)	// can only relative snap in some modes
		bRelativeSnap = true;		

	for (int i = 0; i < ActiveActors.Num(); ++i)
	{
		FTransform NewTransform = InitialTransforms[i];
		NewTransform.AddToTranslation(LastMoveDelta);

		FVector3d NewTranslation = NewTransform.GetTranslation();
		FVector3d SnappedPoint = NewTranslation;
		if (SnapManager)
		{
			FSceneSnapQueryRequest Request;
			Request.RequestType = ESceneSnapQueryType::Position;
			Request.TargetTypes = ESceneSnapQueryTargetType::Grid;

			if (bRelativeSnap)
				Request.Position = LastMoveDelta;
			else
				Request.Position = (FVector)NewTransform.GetTranslation();
			if (Settings->Snapping == EGSMoverToolSnapMode::Override) {
				Request.GridSize = Settings->GridSteps;
			}
			TArray<FSceneSnapQueryResult> Results;
			if (SnapManager->ExecuteSceneSnapQuery(Request, Results))
			{
				if (bRelativeSnap)
					SnappedPoint = InitialTransforms[i].GetTranslation() + Results[0].Position;
				else
					SnappedPoint = Results[0].Position;
			};
		}
		NewTranslation.X = (bPreserveX) ? NewTranslation.X : SnappedPoint.X;
		NewTranslation.Y = (bPreserveY) ? NewTranslation.Y : SnappedPoint.Y;
		NewTranslation.Z = (bPreserveZ) ? NewTranslation.Z : SnappedPoint.Z;

		// can test for no movement here? do not bother updating in that case...
		bool bApplyUpdate = true;

		if (bApplyUpdate && Settings->bCollisionTest && CollisionScene.IsValid())
		{
			FVector FinalMoveDelta = NewTranslation - InitialTransforms[i].GetLocation();
			bIsOverlapping = false;
			if ( auto CollisionResult = CollisionScene->TestCollisionWithOtherObjects(ActiveActors[i], FinalMoveDelta) )
			{
				bIsOverlapping = true;
				FVector Origin, Extent;
				CollisionResult.Value.CollidingActor->GetActorBounds(false, Origin, Extent);
				OverlapHitBox = FBox(Origin - Extent, Origin + Extent);
				bApplyUpdate = false;

				// this does not work! we need to solve from the previous good position, this is doing a fixed-step
				// binary search on what could be a very long vector, so the answer is likely worse than our single-step
				// that only updates from the previous position!
				//if (SnapManager)
				//	continue;		// if we are snapping we do not try to solve for exact collision
				//FVector MinMoveDelta;
				//if (SolveForCollisionTime(ActiveActors[i], FinalMoveDelta, 10, MinMoveDelta) == false)
				//	continue;
				//UE_LOG( LogTemp, Warning, TEXT("SOLVE DOT %f"), Dot(Normalized(MinMoveDelta), Normalized(FinalMoveDelta)) );
				//NewTranslation = InitialTransforms[i].GetLocation() + MinMoveDelta;
			}
		}

		if (bApplyUpdate)
		{
			NewTranslation += ActiveTransformNudge;
			NewTransform.SetTranslation(NewTranslation);
			CurTransforms[i] = NewTransform;
			ActiveActors[i]->SetActorTransform(CurTransforms[i]);
		}
	}
	LastModifiedActors = ActiveActors;
}


bool UGSMoverTool::SolveForCollisionTime(AActor* Actor, FVector FullMoveDelta, int MaxSteps, FVector& SolvedMoveDelta) const
{
	SolvedMoveDelta = FVector::Zero();
	double MinT = 0, MaxT = 1;
	double LastNoCollisionT = 0;
	for (int Step = 0; Step < MaxSteps; ++Step)
	{
		double MidT = (MinT + MaxT) * 0.5;
		FVector TestMoveDelta = MidT * FullMoveDelta;
		auto CollisionResult = CollisionScene->TestCollisionWithOtherObjects(Actor, TestMoveDelta);
		//UE_LOG(LogTemp, Warning, TEXT("  INTERVAL [%f,%f] - collision %d  - step length %f"), MinT, MaxT, (bCollision ? 1 : 0), TestMoveDelta.Length() );
		if (CollisionResult)
			MaxT = MidT;
		else
			MinT = MidT;
		if (!CollisionResult)
			LastNoCollisionT = MidT;
	}
	//UE_LOG(LogTemp, Warning, TEXT("LastNoCollisionT %f"), LastNoCollisionT);
	SolvedMoveDelta = LastNoCollisionT * FullMoveDelta;
	return (LastNoCollisionT > 0);

}


bool UGSMoverTool::GetIsRotating() const
{
	switch (Settings->Mode_Selected) {
		case EGSMoverToolMode::Vertical:
		case EGSMoverToolMode::XYPlane:
		case EGSMoverToolMode::ScreenSpace:
			return (bInTransform) ? bInCtrlModifierForMode : bCtrlDown;
		case EGSMoverToolMode::Tumble:
		case EGSMoverToolMode::Point:
			return true;
		case EGSMoverToolMode::SmartAxis:
		case EGSMoverToolMode::Environment:
		case EGSMoverToolMode::Attached:
		default:
			return false;
	}
}



FBox UGSMoverTool::GetActorLocalBounds(AActor* Actor) const
{
	const FBox* Found = LocalActorBoundsCache.Find(Actor);
	if (Found != nullptr)
		return *Found;

	FBox LocalBounds = Actor->CalculateComponentsBoundingBoxInLocalSpace(true);
	LocalActorBoundsCache.Add(Actor, LocalBounds);
	return LocalBounds;
}


FVector UGSMoverTool::GetRotationOrigin(int ActorIndex) const
{
	const FTransform* UseTransform = (ActorIndex >= 0) ? &InitialTransforms[ActorIndex] : &LastHoveredActorTransform;
	FBox LocalBounds = GetActorLocalBounds(ActiveActors[ActorIndex]);

	if (Settings->RotationPivot_Selected == EGSMoverRotationPivot::Location)
		return UseTransform->GetLocation();

	FVector3d RotationOrigin = LocalBounds.GetCenter();
	if (Settings->RotationPivot_Selected == EGSMoverRotationPivot::Base)
		RotationOrigin.Z = LocalBounds.Min.Z;

	return UseTransform->TransformPosition(RotationOrigin);
}


FVector UGSMoverTool::GetRotationOrigin(AActor* Actor) const
{
	FTransform UseTransform = Actor->GetTransform();
	FBox LocalBounds = GetActorLocalBounds(Actor);

	if (Settings->RotationPivot_Selected == EGSMoverRotationPivot::Location)
		return UseTransform.GetLocation();

	FVector3d RotationOrigin = LocalBounds.GetCenter();
	if (Settings->RotationPivot_Selected == EGSMoverRotationPivot::Base)
		RotationOrigin.Z = LocalBounds.Min.Z;

	return UseTransform.TransformPosition(RotationOrigin);
}


void UGSMoverTool::SetCurrentRotateDelta(const FVector3d& WorldAxis, double PixelDistance, const FVector3d* RotationOriginTmp)
{
	double AngleDeg = PixelDistance * Settings->Gain;

	FToolContextSnappingConfiguration SnapConfig = GetToolManager()->GetContextQueriesAPI()->GetCurrentSnappingSettings();
	bool bEnableSnapping = false;
	if (Settings->Snapping == EGSMoverToolSnapMode::Editor)
		bEnableSnapping = SnapConfig.bEnableRotationGridSnapping;
	else if (Settings->Snapping == EGSMoverToolSnapMode::Override)
		bEnableSnapping = true;
	if (bEnableSnapping) {
		FRotator SettingsSnapAngles = SnapConfig.RotationGridAngles;
		AngleDeg = SnapToIncrement(AngleDeg, SettingsSnapAngles.Yaw);
	}

	FQuaterniond RotateDelta(WorldAxis, AngleDeg, true);

	for (int i = 0; i < ActiveActors.Num(); ++i)
	{
		FTransform InitialTransform = InitialTransforms[i];
		FTransform NewTransform = InitialTransform;

		if (Settings->RotationPivot_Selected != EGSMoverRotationPivot::Location)
		{
			FVector RotationOrigin = GetRotationOrigin(i);
			FFrame3d TransformFrame(RotationOrigin);

			// map current frame (minus scale) into local space of transform frame,
			// then rotate that frame, and then map back from new local space
			FFrame3d CurFrame(InitialTransform);
			FFrame3d CurFrameLocal = TransformFrame.ToFrame(CurFrame);
			TransformFrame.Rotate(RotateDelta);
			FFrame3d NewFrame = TransformFrame.FromFrame(CurFrameLocal);

			NewTransform = NewFrame.ToTransform();
			NewTransform.SetScale3D(InitialTransform.GetScale3D());
		}
		else
		{
			NewTransform.SetRotation(
				(FQuat)(RotateDelta * (FQuaterniond)NewTransform.GetRotation()));
		}

		CurTransforms[i] = NewTransform;
		ActiveActors[i]->SetActorTransform(CurTransforms[i]);
	}
	LastModifiedActors = ActiveActors;
}

void UGSMoverTool::SetCurrentRotateDelta(const FQuaterniond& WorldRotation)
{
	for (int i = 0; i < ActiveActors.Num(); ++i)
	{
		FTransform InitialTransform = InitialTransforms[i];
		FTransform NewTransform = InitialTransform;

		if (Settings->RotationPivot_Selected != EGSMoverRotationPivot::Location)
		{
			FVector RotationOrigin = GetRotationOrigin(i);
			FFrame3d TransformFrame(RotationOrigin);

			// map current frame (minus scale) into local space of transform frame,
			// then rotate that frame, and then map back from new local space
			FFrame3d CurFrame(InitialTransform);
			FFrame3d CurFrameLocal = TransformFrame.ToFrame(CurFrame);
			TransformFrame.Rotate(WorldRotation);
			FFrame3d NewFrame = TransformFrame.FromFrame(CurFrameLocal);

			NewTransform = NewFrame.ToTransform();
			NewTransform.SetScale3D(InitialTransform.GetScale3D());
		}
		else
		{
			NewTransform.SetRotation(
				(FQuat)(WorldRotation * (FQuaterniond)NewTransform.GetRotation()));
		}

		CurTransforms[i] = NewTransform;
		ActiveActors[i]->SetActorTransform(CurTransforms[i]);
	}
	LastModifiedActors = ActiveActors;
}

void UGSMoverTool::UpdateCurrentMoveNudge(FVector3d NudgeDelta)
{
	ActiveTransformNudge += NudgeDelta * Settings->Gain;

	for (int i = 0; i < ActiveActors.Num(); ++i)
	{
		FTransform CurTransform = CurTransforms[i];
		CurTransform.AddToTranslation(NudgeDelta);
		CurTransforms[i] = CurTransform;
		ActiveActors[i]->SetActorTransform(CurTransforms[i]);
	}
	LastModifiedActors = ActiveActors;
}


void UGSMoverTool::OnArrowKey(FKey Key)
{
	if (ActiveActors.Num() == 0 || bInTransform == false) return;

	FVector3d Nudge(0, 0, 0);

	if (Settings->Mode_Selected == EGSMoverToolMode::Vertical || Settings->Mode_Selected == EGSMoverToolMode::Environment)
	{
		if (Key == EKeys::W)
			Nudge = FVector3d(0, 0, 1);
		else if (Key == EKeys::S)
			Nudge = FVector3d(0, 0, -1);
	}
	else if (Settings->Mode_Selected == EGSMoverToolMode::ScreenSpace)
	{
		if (Key == EKeys::W)
			Nudge = (bShiftDown) ? CurrentViewFrame.Z() : CurrentViewFrame.Y();
		else if (Key == EKeys::S)
			Nudge = (bShiftDown) ? -CurrentViewFrame.Z() : -CurrentViewFrame.Y();
		else if (Key == EKeys::A)
			Nudge = -CurrentViewFrame.X();
		else if (Key == EKeys::D)
			Nudge = CurrentViewFrame.X();
	}


	if (Nudge.SquaredLength() > 0) 
		UpdateCurrentMoveNudge(Settings->NudgeSize * Nudge * Settings->Gain);

}

void UGSMoverTool::OnCycleGridSnapping()
{
	ULevelEditorViewportSettings* EditorViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	if (EditorViewportSettings)
		EditorViewportSettings->GridEnabled = (EditorViewportSettings->GridEnabled == 1) ? 0 : 1;
}



bool UGSMoverTool::SupportsWorldSpaceFocusBox()
{
	return (bInTransform || LastHoveredActor != nullptr);
}

FBox UGSMoverTool::GetWorldSpaceFocusBox()
{
	if (bInTransform)
	{
		FVector Center, Extent;
		ActiveActors[0]->GetActorBounds(false, Center, Extent);
		FBox Result(Center - Extent, Center + Extent);
		for (int i = 1; i < ActiveActors.Num(); ++i)
		{
			ActiveActors[i]->GetActorBounds(false, Center, Extent);
			Result += FBox(Center - Extent, Center + Extent);
		}
		return Result;
	}
	else if (LastHoveredActor != nullptr)
		return LastHoveredActorBox;
	return FBox();
}

bool UGSMoverTool::SupportsWorldSpaceFocusPoint()
{
	return false;
}

bool UGSMoverTool::GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut)
{
	return false;
}


void UGSMoverTool::UpdateMeshCollisionScene()
{
	TArray<AActor*> CollisionActors;
	CollisionActors.Add(ActiveActors[0]);

	USelection* SelectedActors = GEditor->GetSelectedActors();
	for (int i = 0; i < SelectedActors->Num(); ++i) {
		AActor* Actor = Cast<AActor>(SelectedActors->GetSelectedObject(i));
		if (Actor && Actor->FindComponentByClass<UMeshComponent>() != nullptr)
			CollisionActors.AddUnique(Actor);
	}
	CollisionActors.Sort();

	// TODO make this work again..

	if (CurrentCollisionSceneActors == CollisionActors)
	{
		check(CollisionScene.IsValid());
		CollisionScene->UpdateAllTransforms();
		return;
	}

	// ok actors changed, have to rebuild scene
	CollisionScene.Reset();
	CurrentCollisionSceneActors.Reset();

	if (CollisionActors.Num() == 1) 
		return;		// only one thing to collide against

	CollisionScene = MakeShared<GS::FGeometryCollisionScene>();
	CollisionScene->AddActors(CollisionActors);
	CollisionScene->UpdateBuild();
	CurrentCollisionSceneActors = CollisionActors;
}


void UGSMoverTool::OnActorSelectionChanged(UObject* ChangedObject)
{
	LastModifiedActors.Reset();
	//UE_LOG(LogTemp, Warning, TEXT("SELECTION CHANGED"));
}


bool UGSMoverTool::OnQuickActionButtonEnabled(FString CommandName)
{
	if (LastModifiedActors.Num() > 0)
		return true;

	USelection* SelectedActors = GEditor->GetSelectedActors();
	return (SelectedActors->Num() > 0);
}

void UGSMoverTool::ApplyRotationUpdateToActors(FText UndoString, TFunctionRef<FTransform(AActor*, FTransform)> UpdateFunc)
{
	TArray<AActor*> UseActors = LastModifiedActors;
	if (UseActors.Num() == 0)
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();
		for (int i = 0; i < SelectedActors->Num(); ++i) {
			AActor* Actor = Cast<AActor>(SelectedActors->GetSelectedObject(i));
			UseActors.Add(Actor);
		}
	}
	if (UseActors.Num() == 0) return;

	GetToolManager()->BeginUndoTransaction(UndoString);
	for (AActor* Actor : UseActors)
	{
		Actor->Modify();

		FTransform InitialTransform = Actor->GetActorTransform();
		FTransform NewTransform = InitialTransform;

		if (Settings->RotationPivot_Selected != EGSMoverRotationPivot::Location)
		{
			// construct oriented frame at origin point
			FVector RotationOrigin = GetRotationOrigin(Actor);
			FFrame3d OrientedOriginFrame(RotationOrigin, (FQuaterniond)InitialTransform.GetRotation());

			// send this frame through update, as if it were the actor location (this should only touch rotation, should update API)
			FTransform NewOriginTransform = UpdateFunc(Actor, OrientedOriginFrame.ToFTransform());
			FFrame3d NewOrientedOriginFrame(NewOriginTransform);

			// map the origin transform through these two frames
			FFrame3d InitialWorldFrame(InitialTransform);
			FFrame3d InitialLocalFrame = OrientedOriginFrame.ToFrame(InitialWorldFrame);
			FFrame3d NewWorldFrame = NewOrientedOriginFrame.FromFrame(InitialLocalFrame);

			// update the scale
			NewTransform = NewWorldFrame.ToTransform();
			NewTransform.SetScale3D(InitialTransform.GetScale3D());
		}
		else
		{
			NewTransform = UpdateFunc(Actor, InitialTransform);
		}

		Actor->SetActorTransform(NewTransform);
	}
	GetToolManager()->EndUndoTransaction();
}

void UGSMoverTool::OnQuickActionButtonClicked(FString CommandName)
{
	if (CommandName == TEXT("ResetRotation"))
	{
		ApplyRotationUpdateToActors(LOCTEXT("ResetRotationTransaction", "Clear Rotation"), [&](AActor* Actor, FTransform CurTransform)
		{
			CurTransform.SetRotation(FQuat::Identity);
			return CurTransform;
		});
	}
	else if (CommandName == TEXT("SetUpright"))
	{
		ApplyRotationUpdateToActors(LOCTEXT("SetUprightTransaction", "Set Upright"), [&](AActor* Actor, FTransform CurTransform)
		{
			FQuaterniond CurOrientation = (FQuaterniond)CurTransform.GetRotation();
			FQuaterniond Rotation(CurOrientation.AxisZ(), FVector::UnitZ());
			FQuaterniond NewOrientation = Rotation * CurOrientation;
			CurTransform.SetRotation((FQuat)NewOrientation);
			return CurTransform;
		});
	}

	else if (CommandName == TEXT("SetUprightNearest"))
	{
		ApplyRotationUpdateToActors(LOCTEXT("SetUprightNearestTransaction", "Set Upright"), [&](AActor* Actor, FTransform CurTransform)
		{
			FQuaterniond CurOrientation = (FQuaterniond)CurTransform.GetRotation();
			FFrame3d TempFrame(FVector3d::Zero(), CurOrientation);
			FVector3d UseAxis = FVector3d::UnitZ(); double MaxDot = -9999;
			for (int k = 0; k < 3; ++k) {
				FVector3d Axis = TempFrame.GetAxis(k);
				double dot = Axis.Dot(FVector3d::UnitZ());
				if (dot > MaxDot) { UseAxis = Axis; MaxDot = dot; }
				Axis = -Axis;
				dot = Axis.Dot(FVector3d::UnitZ());
				if (dot > MaxDot) { UseAxis = Axis; MaxDot = dot; }
			}

			FQuaterniond Rotation(UseAxis, FVector::UnitZ());
			FQuaterniond NewOrientation = Rotation * CurOrientation;
			CurTransform.SetRotation((FQuat)NewOrientation);
			return CurTransform;
		});
	}
}


#undef LOCTEXT_NAMESPACE
