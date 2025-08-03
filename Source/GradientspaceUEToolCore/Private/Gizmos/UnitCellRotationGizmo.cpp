// Copyright Gradientspace Corp. All Rights Reserved.
#include "Gizmos/UnitCellRotationGizmo.h"

#include "InteractiveGizmoManager.h"
#include "BaseBehaviors/SingleClickOrDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "BaseGizmos/GizmoMath.h"
#include "ToolDataVisualizer.h"
#include "ToolSceneQueriesUtil.h"

#include "BoxTypes.h"

using namespace UE::Geometry;
//using namespace GS;


const FString UUnitCellRotationGizmo::GizmoName(TEXT("UnitCellRotationGizmo"));


UInteractiveGizmo* UUnitCellRotationGizmoBuilder::BuildGizmo(const FToolBuilderState& SceneState) const
{
	return NewObject<UUnitCellRotationGizmo>(SceneState.GizmoManager);
}



void UUnitCellRotationGizmo::SetCell(UE::Geometry::FFrame3d Frame, FVector3d Dimensions, FTransformSequence3d& CurrentTransform)
{
	CellFrame = Frame;
	CellDimensions = Dimensions;
	TransformSeq = CurrentTransform;
}

void UUnitCellRotationGizmo::UpdateTransform(FTransformSequence3d& NewTransform)
{
	TransformSeq = NewTransform;
}

void UUnitCellRotationGizmo::SetEnabled(bool bEnabledIn)
{
	bEnabled = bEnabledIn;
}


void UUnitCellRotationGizmo::Setup()
{
	UInteractiveGizmo::Setup();

	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
	// We use modifiers on hover to change the highlighting according to what can be selected  	
	//HoverBehavior->Modifiers.RegisterModifier(ShiftModifierID, FInputDeviceState::IsShiftKeyDown);
	//HoverBehavior->Modifiers.RegisterModifier(CtrlModifierID, FInputDeviceState::IsCtrlKeyDown);
	HoverBehavior->Initialize(this);
	HoverBehavior->SetDefaultPriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY);
	AddInputBehavior(HoverBehavior);

	USingleClickOrDragInputBehavior* ClickOrDragBehavior = NewObject<USingleClickOrDragInputBehavior>();
	ClickOrDragBehavior->Initialize(this, this);
	//ClickOrDragBehavior->Modifiers.RegisterModifier(ShiftModifierID, FInputDeviceState::IsShiftKeyDown);
	//ClickOrDragBehavior->Modifiers.RegisterModifier(CtrlModifierID, FInputDeviceState::IsCtrlKeyDown);
	ClickOrDragBehavior->SetDefaultPriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY);
	AddInputBehavior(ClickOrDragBehavior);

	bInInteraction = false;
}

void UUnitCellRotationGizmo::Render(IToolsContextRenderAPI* RenderAPI)
{
	CurCameraState = RenderAPI->GetCameraState();
	if (!bEnabled) return;
	FVector ForwardDir = CurCameraState.Forward();

	FToolDataVisualizer Draw;
	Draw.BeginFrame(RenderAPI);

	Draw.PushTransform(CellFrame.ToFTransform());

	FAxisAlignedBox3d CellBounds(FVector3d::Zero(), CellDimensions);
	CellBounds.Expand(5);
	FVector3d Extents = CellBounds.Extents();

	//Draw.DrawWireBox((FBox)CellBounds, FLinearColor::Green, 1.0f, true);

	FTransformSequence3d Tmp = TransformSeq;
	Tmp.ClearScales();
	const auto& TransformList = Tmp.GetTransforms();
	FFrame3d BoxFrame;
	for (const FTransformSRT3d& Transform : TransformList)
	{
		BoxFrame.Transform(Transform);
	}
	BoxFrame.Origin = CellBounds.Center();


	for (int Axis = 0; Axis < 3; ++Axis)
	{
		FVector3d GridAxis = FVector3d::Zero();
		GridAxis[Axis] = 1.0;

		FVector4f AxisColor(0, 0, 0);
		AxisColor[Axis] = 1.0;

		FVector3d FacePos = CellBounds.Center();

		FVector3d ExtentsDir(0,0,0);
		ExtentsDir[Axis] = 1.0;
		FVector3d WorldExtentsDir = CellFrame.FromFrameVector(ExtentsDir);

		double AxisSign = (WorldExtentsDir.Dot(ForwardDir) < 0) ? 1.0 : -1.0;

		FacePos[Axis] += AxisSign * Extents[Axis];

		float LineWidth = (bHovering && Axis == ActiveAxisIndex) ? 5.0 : 1.0;
		Draw.DrawCircle(FacePos, GridAxis, CellDimensions.X / 10, 16, ToLinearColor(AxisColor), LineWidth, false);

		CurAxisElements[Axis].WorldPosition = CellFrame.FromFramePoint(FacePos);
		CurAxisElements[Axis].WorldFrame = FFrame3d(CurAxisElements[Axis].WorldPosition, GridAxis);
		CurAxisElements[Axis].BoxAxisIndex = Axis;
	}

	// draw cell axes
	//Draw.DrawLine(BoxFrame.Origin, BoxFrame.FromFramePoint(FVector(Extents.X, 0, 0)), FLinearColor(1, 0, 0), 1.0, false);
	//Draw.DrawLine(BoxFrame.Origin, BoxFrame.FromFramePoint(FVector(0, Extents.Y, 0)), FLinearColor(0, 1, 0), 1.0, false);
	//Draw.DrawLine(BoxFrame.Origin, BoxFrame.FromFramePoint(FVector(0, 0, Extents.Z)), FLinearColor(0, 0, 1), 1.0, false);

	Draw.PopTransform();
	Draw.EndFrame();
}

void UUnitCellRotationGizmo::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
}

void UUnitCellRotationGizmo::Tick(float DeltaTime)
{
}


bool UUnitCellRotationGizmo::HitTest(FRay3d WorldRay, double& RayT, FVector3d& RayPos, int& HitAxisIndex) const
{
	if (!bEnabled) return false;

	HitAxisIndex = -1;
	double NearestDist = TNumericLimits<double>::Max();
	for (int j = 0; j < 3; ++j)
	{
		FVector HitPoint;
		if (CurAxisElements[j].WorldFrame.RayPlaneIntersection(WorldRay.Origin, WorldRay.Direction, j, HitPoint))
		{
			if (Distance(HitPoint, CurAxisElements[j].WorldPosition) < (CellDimensions.X / 10) * 1.1)
			{
				double Dist = WorldRay.Dist(HitPoint);
				if (Dist < NearestDist)
				{
					NearestDist = Dist;
					HitAxisIndex = CurAxisElements[j].BoxAxisIndex;
				}
			}
		}
	}
	if (HitAxisIndex != -1)
	{
		RayT = NearestDist;
		RayPos = WorldRay.PointAt(RayT);
		return true;
	}
	return false;
}


FInputRayHit UUnitCellRotationGizmo::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	if (!bEnabled) FInputRayHit();

	int HitAxisIndex = -1; FVector3d HitPos; double HitPosRayT;
	if (HitTest(ClickPos.WorldRay, HitPosRayT, HitPos, HitAxisIndex))
	{
		return FInputRayHit(HitPosRayT);
	}
	return FInputRayHit();
}

void UUnitCellRotationGizmo::OnClicked(const FInputDeviceRay& ClickPos)
{
	int HitAxisIndex = -1; FVector3d HitPos; double HitPosRayT;
	if (HitTest(ClickPos.WorldRay, HitPosRayT, HitPos, HitAxisIndex))
	{
		if (HitAxisIndex >= 0)
		{
			OnAxisClicked.Broadcast(HitAxisIndex);
		}
	}
}

// IClickDragBehaviorTarget implementation
FInputRayHit UUnitCellRotationGizmo::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	return FInputRayHit();
}

void UUnitCellRotationGizmo::OnClickPress(const FInputDeviceRay& PressPos)
{
}

void UUnitCellRotationGizmo::OnClickDrag(const FInputDeviceRay& DragPos)
{
}

void UUnitCellRotationGizmo::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
}

void UUnitCellRotationGizmo::OnTerminateDragSequence()
{
}

// IHoverBehaviorTarget implementation
FInputRayHit UUnitCellRotationGizmo::BeginHoverSequenceHitTest(const FInputDeviceRay& DevicePos)
{
	if (!bEnabled) FInputRayHit();

	int HitAxisIndex = -1; FVector3d HitPos; double HitPosRayT;
	if (HitTest(DevicePos.WorldRay, HitPosRayT, HitPos, HitAxisIndex))
	{
		return FInputRayHit(HitPosRayT);
	}
	return FInputRayHit();
}

void UUnitCellRotationGizmo::OnBeginHover(const FInputDeviceRay& DevicePos)
{
	bHovering = true;
	ActiveAxisIndex = -1;
}

bool UUnitCellRotationGizmo::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	int HitAxisIndex = -1; FVector3d HitPos; double HitPosRayT;
	if (HitTest(DevicePos.WorldRay, HitPosRayT, HitPos, HitAxisIndex))
	{
		ActiveAxisIndex = HitAxisIndex;
	}

	return true;
}

void UUnitCellRotationGizmo::OnEndHover()
{
	bHovering = false;
	ActiveAxisIndex = -1;
}
