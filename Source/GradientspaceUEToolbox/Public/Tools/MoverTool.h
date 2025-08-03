// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolQueryInterfaces.h"
#include "PropertyTypes/ActionButtonGroup.h"
#include "FrameTypes.h"
#include "InputBehaviors/GSClickOrDragBehavior.h"
#include "PropertySets/AxisFilterPropertyType.h"
#include "PropertyTypes/EnumButtonsGroup.h"
#include "Utility/CameraUtils.h"

#include "MoverTool.Generated.h"

class UWorld;
namespace UE::Geometry { class FQuickAxisTranslater; }
namespace GS { class FMeshCollisionScene; }
namespace GS { class FGeometryCollisionScene; }

UCLASS(MinimalAPI)
class UGSMoverToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};


UENUM()
enum class EGSMoverToolMode : uint8
{
	/** Translate the object vertically along the Z axis   [+Ctrl] rotate around Z */
	Vertical = 0,
	/** Translate the object in the XY plane   [+Ctrl] rotate around Z  [+Ctrl+Shift] tilt in/out along view direction */
	XYPlane = 1,
	/** Translate the object in X, Y, or Z axis based on mouse movement direction */
	SmartAxis = 2,
	/** Translate the object in the screen plane   [+Ctrl] arcball rotation   [+Ctrl+Shift] rotate around view vector (roll) */
	ScreenSpace = 3,
	/** Translate the object along the Landscape or objects in the level */
	Environment = 4,
	/** Rotate around the object's center */
	Tumble = 5,
	/** Rotate the object to keep the clicked point oriented towards the cursor */
	Point = 6,
	/** "Grab" the object under the cursor when flying, and move it with the camera */
	Attached = 9 UMETA(DisplayName = "Grab"),

	None = 254  UMETA(Hidden)
};


UENUM()
enum class EGSMoverRotationPivot : uint8
{
	Location = 0,
	Center = 1,
	Base = 2
};

UENUM()
enum class EGSMoverToolTextDisplayMode : uint8
{
	/** Show the amount of translation/rotation in the current transformation */
	Delta = 0,
	/** Show World Coordinates  */
	Absolute = 2,
	None = 3
};


UENUM()
enum class EGSMoverToolSnapMode : uint8
{
	/** Use the Editor snapping settings */
	Editor = 0,
	/** Override the Editor snapping settings */
	Override = 1,
	/** Disable snapping */
	Disabled = 2,

	Last = 3  UMETA(Hidden)
};


UCLASS(MinimalAPI)
class UGSMoverTool_Settings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UGSMoverTool_Settings();

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (TransientToolProperty))
	FGSEnumButtonsGroup Mode;
	UPROPERTY()
	EGSMoverToolMode Mode_Selected = EGSMoverToolMode::XYPlane;

	/** Gain scales the transformation amount relative to the mouse movement. Hotkeys [ and ] */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (TransientToolProperty, UIMin = 0.001, UIMax = 1))
	float Gain = 1.0f;

	/** Step Size for WASD/MouseWheel Nudging */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (UIMin = 0.1, UIMax = 10.0, Min = 0))
	float NudgeSize = 1.0f;

	/** Test for collisions between the active object and any other selected objects. Double-click or Ctrl+Alt marquee-select to modify selection. */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (DisplayName = "Collide with Selected"))
	bool bCollisionTest = false;

	/** Object point to rotate around */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (TransientToolProperty))
	FGSEnumButtonsGroup RotationPivot;
	UPROPERTY()
	EGSMoverRotationPivot RotationPivot_Selected = EGSMoverRotationPivot::Location;

	/** Use the Editor snapping settings, or override them. [G] to toggle editor grid snapping. */
	UPROPERTY(EditAnywhere, Category = "Snapping")
	EGSMoverToolSnapMode Snapping = EGSMoverToolSnapMode::Editor;

	/** Grid Snapping Step Size */
	UPROPERTY(EditAnywhere, Category = "Snapping", meta=(EditCondition="Snapping==EGSMoverToolSnapMode::Override"))
	FVector GridSteps;

	/** Snap to points in the World Grid, instead of relative step sizes [Shift+G] */
	UPROPERTY(EditAnywhere, Category = "Snapping")
	bool bToWorldGrid = false;



	/** What kind of text information to show in the viewport */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Settings")
	EGSMoverToolTextDisplayMode ShowText = EGSMoverToolTextDisplayMode::Delta;

	/** 
	 * Number of bounding-box corners of an object that must be visible for it to become active.
	 * This avoids accidentally modifying (eg) huge ground planes, walls behind objects, etc.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Settings", meta=(ClampMin=0, ClampMax=8))
	int VisibilityFiltering = 6;

};


UCLASS(MinimalAPI)
class UGSMoverTool_SmartAxisSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Align the smart gizmo axes to the hit surface normal */
	UPROPERTY(EditAnywhere, Category = "SmartAxis Options")
	bool bAlignToNormal = false;
};


UENUM()
enum class EEnvironmentPlacementLocation : uint8
{
	/** Use the bounding-box base-center point as the placement origin */
	BoundsBase = 0,
	/** Use the bounding-box center point as the placement origin */
	BoundsCenter = 1,
	/** Use the initial ray-hit position as the placement origin */
	HitPosition = 2,
	/** Use the initial ray-hit position as the placement origin, maintaining height from the "ground" surface below (if available) */
	HitHeight = 3,
	/** Use the obect pivot as the placement origin */
	ObjectPivot = 4
};

UENUM()
enum class EEnvironmentPlacementType : uint8
{
	/** Place the target object on the Landscape */
	LandscapeOnly = 0,
	/** Place the target object on any World Static objects */
	AnyWorldStatic = 1,
	/** Place the target object on any objects in the level */
	Any = 2
};

UCLASS(MinimalAPI)
class UGSMoverTool_EnvironmentSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UGSMoverTool_EnvironmentSettings();

	/** The Origin point on the target object that will be placed on the environment */
	UPROPERTY(EditAnywhere, Category = "Environment Options")
	EEnvironmentPlacementLocation Location = EEnvironmentPlacementLocation::BoundsBase;

	/** Types of Environment objects that can be hit*/
	UPROPERTY(EditAnywhere, Category = "Environment Options")
	EEnvironmentPlacementType HitType = EEnvironmentPlacementType::Any;

	/** Selectively disable movement along the X/Y/Z axes */
	UPROPERTY(EditAnywhere, Category = "Environment Options")
	FModelingToolsAxisFilter AxisLock;
};



UCLASS(MinimalAPI)
class UGSMoverTool_GrabSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UGSMoverTool_GrabSettings();

	/** Do not rotate the object with the camera, only translate it */
	UPROPERTY(EditAnywhere, Category="Grab")
	bool bLockRotation = false;

	/** Selectively disable movement along the X/Y/Z axes */
	UPROPERTY(EditAnywhere, Category = "Grab")
	FModelingToolsAxisFilter AxisLock;
};


UCLASS(MinimalAPI)
class UGSMoverTool_QuickActions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Quick Actions", meta=(TransientToolProperty))
	FGSActionButtonGroup Actions;
};




UCLASS()
class UGSMoverTool : public UInteractiveTool, 
	public IClickBehaviorTarget,
	public IClickDragBehaviorTarget,
	public IHoverBehaviorTarget, 
	public IGSMouseWheelDuringDragTarget,
	public IInteractiveToolCameraFocusAPI
{
	GENERATED_BODY()
public:
	void SetTargetWorld(UWorld* World);

	virtual void Setup() override;
	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	// IClickBehaviorTarget implementation
	virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos) override;
	virtual void OnClicked(const FInputDeviceRay& ClickPos) override;

	// IClickDragBehaviorTarget implementation
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override;

	// modifiers implementation
	void OnUpdateModifierState(int ModifierID, bool bIsOn);

	// IHoverBehaviorTarget implementation
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& DevicePos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;

	// IGSMouseWheelDuringDragTarget
	virtual void OnMouseWheelScroll(float WheelDelta) override;

	// IInteractiveToolCameraFocusAPI implementation
	virtual bool SupportsWorldSpaceFocusBox() override;
	virtual FBox GetWorldSpaceFocusBox() override;
	virtual bool SupportsWorldSpaceFocusPoint() override;
	virtual bool GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut) override;

public:
	UPROPERTY()
	TObjectPtr<UGSMoverTool_Settings> Settings;

	UPROPERTY()
	TObjectPtr<UGSMoverTool_SmartAxisSettings> SmartAxisSettings;

	UPROPERTY()
	TObjectPtr<UGSMoverTool_EnvironmentSettings> EnvironmentSettings;

	UPROPERTY()
	TObjectPtr<UGSMoverTool_GrabSettings> GrabSettings;

	UPROPERTY()
	TObjectPtr<UGSMoverTool_QuickActions> QuickActions;

	void UpdatePropertySetVisibilities();

protected:
	TWeakObjectPtr<UWorld> TargetWorld;
	FViewCameraState ViewState;
	UE::Geometry::FFrame3d CurrentViewFrame;
	GS::FViewProjectionInfo LastViewInfo;


	bool bCtrlDown = false;
	bool bShiftDown = false;

	TSharedPtr<FGSEnumButtonsTarget> ModesEnumButtonsTarget;
	TSharedPtr<FGSEnumButtonsTarget> RotationPivotEnumButtonsTarget;

	AActor* FindHitActor(FRay WorldRay, double& Distance) const;
	AActor* FindHitActor(FRay WorldRay, double& Distance, FVector& HitNormal) const;
	AActor* FindEnvironmentHit(FRay WorldRay, double& Distance, FVector& HitNormal) const;
	bool FindEnvironmentOrGroundHit(FRay WorldRay, double& Distance, FVector& HitNormal, bool bFallbackToGroundPlane) const;
	bool IsActorMovable(const AActor* Actor) const;

	TArray<AActor*> ActiveActors;
	TArray<FBox> InitialBounds;
	TArray<FTransform> InitialTransforms;
	TArray<FTransform> CurTransforms;
	FRay StartPosRay;
	FVector StartHitPos;
	FVector StartRelativeOffset;
	bool bInTransform = false;
	EGSMoverToolMode ActiveTransformMode = EGSMoverToolMode::None;
	bool bInCtrlModifierForMode = false;
	bool bInShiftModifierForMode = false;
	FVector3d LastMoveDelta;
	FVector3d ActiveTransformNudge;

	// these are only for visualization
	FVector2d LastRotationWorldDelta;

	FVector3d CurRotationSphereCenter;
	double CurRotationSphereRadius;
	UE::Geometry::FQuaterniond AccumRotation;
	FVector3d LastWorldHitPos;

	bool bIsOverlapping = false;
	FBox OverlapHitBox;
	bool SolveForCollisionTime(AActor* Actor, FVector FullMoveDelta, int MaxSteps, FVector& SolvedMoveDelta) const;

	void InitializeTransformMode();

	void UpdateTransformsForMode(const FInputDeviceRay& DragPos);
	void UpdateTransformsForMode_Vertical(const FInputDeviceRay& DragPos);
	void UpdateTransformsForMode_XYPlane(const FInputDeviceRay& DragPos);
	void UpdateTransformsForMode_ScreenSpace(const FInputDeviceRay& DragPos);
	void UpdateTransformsForMode_Environment(const FInputDeviceRay& DragPos);
	void UpdateTransformsForMode_SmartAxis(const FInputDeviceRay& DragPos);
	void UpdateTransformsForMode_Tumble(const FInputDeviceRay& DragPos);
	void UpdateTransformsForMode_Point(const FInputDeviceRay& DragPos);
	void SetCurrentMoveDelta(const FVector3d& WorldDelta, bool bPreserveX, bool bPreserveY, bool bPreserveZ);
	void SetCurrentRotateDelta(const FVector3d& WorldAxis, double ScreenDistance, const FVector3d* RotationOrigin = nullptr);
	void SetCurrentRotateDelta(const UE::Geometry::FQuaterniond& WorldRotation);
	void UpdateCurrentMoveNudge(FVector3d NudgeDelta);

	void OnArrowKey(FKey Key);
	void OnCycleGridSnapping();

	TSharedPtr<UE::Geometry::FQuickAxisTranslater> Translater;

	// flight-cam-attached mode
	bool bWasFlightCameraActive = false;

	AActor* LastHoveredActor = nullptr;
	FTransform LastHoveredActorTransform;
	FBox LastHoveredActorBox;
	FVector3d LastHoverHitPoint;
	FVector3d LastHoverHitNormal;
	UE::Geometry::FFrame3d LastActorTransformInViewFrame;
	void UpdateHoveredActor(const FRay& WorldRay);

	mutable TMap<AActor*, FBox> LocalActorBoundsCache;
	FBox GetActorLocalBounds(AActor* Actor) const;

	TArray<AActor*> CurrentCollisionSceneActors;
	//TSharedPtr<GS::FMeshCollisionScene> CollisionScene;
	TSharedPtr<GS::FGeometryCollisionScene> CollisionScene;
	void UpdateMeshCollisionScene();

	TArray<AActor*> LastModifiedActors;

	// connected to USelection
	void OnActorSelectionChanged(UObject* ChangedObject);

	bool GetIsRotating() const;
	FVector GetRotationOrigin(int ActorIndex) const;
	FVector GetRotationOrigin(AActor* Actor) const;

	TSharedPtr<FGSActionButtonTarget> QuickActionTarget;
	virtual bool OnQuickActionButtonEnabled(FString CommandName);
	virtual void OnQuickActionButtonClicked(FString CommandName);
	virtual void ApplyRotationUpdateToActors(FText UndoString, TFunctionRef<FTransform(AActor*,FTransform)> UpdateFunc);
};


