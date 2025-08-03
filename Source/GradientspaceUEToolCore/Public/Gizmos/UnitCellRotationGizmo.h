// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveGizmo.h"
#include "InteractiveGizmoBuilder.h"
#include "InputState.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"

#include "FrameTypes.h"
#include "TransformSequence.h"

#include "UnitCellRotationGizmo.generated.h"


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UUnitCellRotationGizmoBuilder : public UInteractiveGizmoBuilder
{
	GENERATED_BODY()
public:
	virtual UInteractiveGizmo* BuildGizmo(const FToolBuilderState& SceneState) const override;
};


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UUnitCellRotationGizmo : public UInteractiveGizmo, public IClickBehaviorTarget, public IClickDragBehaviorTarget, public IHoverBehaviorTarget
{
	GENERATED_BODY()

public:
	static const FString GizmoName;

	virtual void Setup() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;
	virtual void Tick(float DeltaTime) override;

	void SetEnabled(bool bEnabled);
	void SetCell(UE::Geometry::FFrame3d Frame, FVector3d Dimensions, UE::Geometry::FTransformSequence3d& CurrentTransform);
	void UpdateTransform(UE::Geometry::FTransformSequence3d& NewTransform);


	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUnitCellRotationAxisClicked, int Axis);
	FOnUnitCellRotationAxisClicked OnAxisClicked;

	// delegates for rotation x/y/z delta


	// IClickBehaviorTarget implementation
	virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos) override;
	virtual void OnClicked(const FInputDeviceRay& ClickPos) override;

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

	bool HitTest(FRay3d WorldRay, double& RayT, FVector3d& RayPos, int& HitAxisIndex) const;

protected:
	bool bEnabled = true;

	UE::Geometry::FFrame3d CellFrame;
	FVector3d CellDimensions;
	UE::Geometry::FTransformSequence3d TransformSeq;

	FViewCameraState CurCameraState;

	struct FElementInfo
	{
		UE::Geometry::FFrame3d WorldFrame;
		FVector3d WorldPosition;
		int BoxAxisIndex;
	};

	FElementInfo CurAxisElements[3];

	int ActiveAxisIndex = -1;
	bool bInInteraction = false;
	bool bHovering = false;
};

