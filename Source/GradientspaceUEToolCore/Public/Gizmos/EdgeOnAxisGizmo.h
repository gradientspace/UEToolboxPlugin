// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveGizmo.h"
#include "InteractiveGizmoBuilder.h"
#include "InputState.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"

#include "FrameTypes.h"

#include "EdgeOnAxisGizmo.generated.h"


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UEdgeOnAxisGizmoBuilder : public UInteractiveGizmoBuilder
{
	GENERATED_BODY()
public:
	virtual UInteractiveGizmo* BuildGizmo(const FToolBuilderState& SceneState) const override;
};


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UEdgeOnAxisGizmo : public UInteractiveGizmo, public IClickDragBehaviorTarget, public IHoverBehaviorTarget
{
	GENERATED_BODY()

public:
	static const FString GizmoName;

	void InitializeParameters(
		const UE::Geometry::FFrame3d& WorldFrame,
		int FrameAxisIndex,
		FVector3d LineWorldPosA, FVector3d LineWorldPosB,
		double AxisMin, double AxisMax,
		double CurrentAxisValue);

	void SetEnabled(bool bEnabled);


	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDragUpdate, UEdgeOnAxisGizmo*, double);
	
	FOnDragUpdate OnBeginDrag;
	FOnDragUpdate OnUpdateDrag;
	FOnDragUpdate OnEndDrag;


	virtual void Setup() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;
	virtual void Tick(float DeltaTime) override;

	// IClickDragBehaviorTarget implementation
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override;

	// IHoverBehaviorTarget implementation
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;


	void GetLineWorld(FVector3d& A, FVector3d& B) const;

	bool HitTest(FRay3d WorldRay, double& RayT, FVector3d& RayPos, double& SegmentT, FVector3d& SegmentPos) const;

protected:
	bool bEnabled = true;

	UE::Geometry::FFrame3d FrameWorld;
	int AxisIndex;
	FVector2d LineA, LineB;
	double AxisExtents[2];
	
	double CurrentAxisT;

	FViewCameraState CurCameraState;

	bool bInDragInteraction = false;
	bool bHovering = false;
};

