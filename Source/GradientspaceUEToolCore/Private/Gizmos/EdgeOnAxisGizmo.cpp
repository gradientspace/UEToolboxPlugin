// Copyright Gradientspace Corp. All Rights Reserved.
#include "Gizmos/EdgeOnAxisGizmo.h"

#include "InteractiveGizmoManager.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "BaseGizmos/GizmoMath.h"
#include "ToolDataVisualizer.h"
#include "ToolSceneQueriesUtil.h"

#include "SegmentTypes.h"
#include "BoxTypes.h"
#include "Distance/DistRay3Segment3.h"

#include "Engine/Font.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"  // for GEngine->GetSmallFont()
#include "SceneView.h"

using namespace UE::Geometry;

const FString UEdgeOnAxisGizmo::GizmoName(TEXT("EdgeOnAxisGizmo"));


UInteractiveGizmo* UEdgeOnAxisGizmoBuilder::BuildGizmo(const FToolBuilderState& SceneState) const
{
	return NewObject<UEdgeOnAxisGizmo>(SceneState.GizmoManager);
}

void UEdgeOnAxisGizmo::Setup()
{
	UInteractiveGizmo::Setup();

	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
	//HoverBehavior->Modifiers.RegisterModifier(ShiftModifierID, FInputDeviceState::IsShiftKeyDown);
	//HoverBehavior->Modifiers.RegisterModifier(CtrlModifierID, FInputDeviceState::IsCtrlKeyDown);
	HoverBehavior->Initialize(this);
	HoverBehavior->SetDefaultPriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY);
	AddInputBehavior(HoverBehavior);

	UClickDragInputBehavior* ClickDragBehavior = NewObject<UClickDragInputBehavior>();
	ClickDragBehavior->Initialize(this);
	//ClickDragBehavior->Modifiers.RegisterModifier(ShiftModifierID, FInputDeviceState::IsShiftKeyDown);
	//ClickDragBehavior->Modifiers.RegisterModifier(CtrlModifierID, FInputDeviceState::IsCtrlKeyDown);
	ClickDragBehavior->SetDefaultPriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY);
	AddInputBehavior(ClickDragBehavior);

	bInDragInteraction = false;
}



void UEdgeOnAxisGizmo::InitializeParameters(
	const UE::Geometry::FFrame3d& WorldFrame,
	int FrameAxisIndex,
	FVector3d LineWorldPosA, FVector3d LineWorldPosB,
	double AxisMin, double AxisMax,
	double CurrentAxisValue)
{
	FrameWorld = WorldFrame;
	AxisIndex = FMath::Clamp(FrameAxisIndex, 0, 2);

	LineA = FrameWorld.ToPlaneUV(LineWorldPosA, FrameAxisIndex);
	LineB = FrameWorld.ToPlaneUV(LineWorldPosB, FrameAxisIndex);
	AxisExtents[0] = AxisMin;
	AxisExtents[1] = AxisMax;

	CurrentAxisT = FMath::Clamp(CurrentAxisValue, AxisExtents[0], AxisExtents[1]);
}

void UEdgeOnAxisGizmo::SetEnabled(bool bEnabledIn)
{
	bEnabled = bEnabledIn;
}


void UEdgeOnAxisGizmo::GetLineWorld(FVector3d& A, FVector3d& B) const
{
	FVector3d AxisDir = FrameWorld.GetAxis(AxisIndex);
	FVector3d PerpA = FrameWorld.GetAxis((AxisIndex + 1) % 3);
	FVector3d PerpB = FrameWorld.GetAxis((AxisIndex + 2) % 3);

	A = FrameWorld.FromPlaneUV(LineA) + CurrentAxisT * AxisDir;
	B = FrameWorld.FromPlaneUV(LineB) + CurrentAxisT * AxisDir;
}


void UEdgeOnAxisGizmo::Render(IToolsContextRenderAPI* RenderAPI)
{
	CurCameraState = RenderAPI->GetCameraState();
	if (!bEnabled) return;

	FToolDataVisualizer Draw;
	Draw.BeginFrame(RenderAPI);

	FVector3d A, B;
	GetLineWorld(A, B);
	if (bHovering || bInDragInteraction)
	{
		Draw.DrawLine(A, B, FLinearColor::Yellow, 6.0, false);
	}
	else
	{
		Draw.DrawLine(A, B, FLinearColor::Red, 3.0, false);
	}

	if (bInDragInteraction)
	{
		FVector3d Axis = FrameWorld.GetAxis(AxisIndex);
		Draw.DrawLine(
			FrameWorld.Origin + AxisExtents[0] * Axis,
			FrameWorld.Origin + AxisExtents[1] * Axis,
			FLinearColor::White, 0.5, false);
		Draw.DrawPoint(FrameWorld.Origin + CurrentAxisT * Axis, FLinearColor::White, 5.0, false);
	}

	// debug drawing...
	//Draw.DrawPoint(FrameWorld.Origin, FLinearColor::Black, 10.0f, false);
	//FVector3d DebugAxis = FrameWorld.GetAxis(AxisIndex);
	//Draw.DrawPoint(FrameWorld.Origin + DebugAxis*AxisExtents[1], FLinearColor::Green, 12.0f, false);

	//Draw.PushTransform(CellFrame.ToFTransform());
	//Draw.PopTransform();

	Draw.EndFrame();
}

void UEdgeOnAxisGizmo::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	if (!bEnabled) return;
	if (!bInDragInteraction) return;

	double DPIScale = Canvas->GetDPIScale();
	UFont* UseFont = GEngine->GetTinyFont(); // GEngine->GetSmallFont();
	FViewCameraState CamState = RenderAPI->GetCameraState();
	const FSceneView* SceneView = RenderAPI->GetSceneView();

	FString CursorString = FString::Printf(TEXT("%.2f"), CurrentAxisT);
	FVector3d Axis = FrameWorld.GetAxis(AxisIndex);
	FVector2d LabelPixelPos;
	SceneView->WorldToPixel(FrameWorld.Origin + CurrentAxisT * Axis, LabelPixelPos);
	LabelPixelPos.X += 25 / DPIScale;
	LabelPixelPos.Y += 25 / DPIScale;

	Canvas->DrawShadowedString(LabelPixelPos.X / DPIScale, LabelPixelPos.Y / DPIScale, *CursorString, UseFont, FLinearColor::White);
}

void UEdgeOnAxisGizmo::Tick(float DeltaTime)
{
}



bool UEdgeOnAxisGizmo::HitTest(FRay3d WorldRay, double& RayT, FVector3d& RayPos, double& SegmentT, FVector3d& SegmentPos) const
{
	if (!bEnabled) return false;

	FVector3d A, B;
	GetLineWorld(A, B);
	FSegment3d Segment(A, B);
	FDistRay3Segment3d Dist(WorldRay, Segment);
	double DistSqr = Dist.GetSquared();
	RayPos = Dist.RayClosestPoint;
	SegmentPos = Dist.SegmentClosestPoint;
	RayT = Dist.RayParameter;
	SegmentT = Dist.SegmentParameter;
	if (ToolSceneQueriesUtil::PointSnapQuery(CurCameraState, RayPos, SegmentPos))
	{
		return true;
	}
	return false;
}

FInputRayHit UEdgeOnAxisGizmo::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FVector3d RayPos, SegmentPos; double RayT, SegmentT;
	if (HitTest(PressPos.WorldRay, RayT, RayPos, SegmentT, SegmentPos))
	{
		return FInputRayHit(RayT);
	}

	return FInputRayHit();
}

void UEdgeOnAxisGizmo::OnClickPress(const FInputDeviceRay& PressPos)
{
	bInDragInteraction = true;
	OnBeginDrag.Broadcast(this, CurrentAxisT);
}

void UEdgeOnAxisGizmo::OnClickDrag(const FInputDeviceRay& DragPos)
{
	FVector3d A, B;
	GetLineWorld(A, B);
	FVector3d LineDir(Normalized(B - A));
	FVector3d Axis = FrameWorld.GetAxis(AxisIndex);
	FVector3d PlaneNormal = Cross(LineDir, Axis);
	FFrame3d WorldPlane(FrameWorld.Origin, PlaneNormal);
	FVector PlaneHitPos;
	if (WorldPlane.RayPlaneIntersection(DragPos.WorldRay.Origin, DragPos.WorldRay.Direction, 2, PlaneHitPos))
	{
		double HitPosAxisT = (PlaneHitPos - FrameWorld.Origin).Dot(Axis);
		CurrentAxisT = FMath::Clamp(HitPosAxisT, AxisExtents[0], AxisExtents[1]);
	}

	OnUpdateDrag.Broadcast(this, CurrentAxisT);
}

void UEdgeOnAxisGizmo::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	bInDragInteraction = false;
	OnEndDrag.Broadcast(this, CurrentAxisT);
}

void UEdgeOnAxisGizmo::OnTerminateDragSequence()
{
	bInDragInteraction = false;
	OnEndDrag.Broadcast(this, CurrentAxisT);
}

FInputRayHit UEdgeOnAxisGizmo::BeginHoverSequenceHitTest(const FInputDeviceRay& DevicePos)
{
	FVector3d RayPos, SegmentPos; double RayT, SegmentT;
	if (HitTest(DevicePos.WorldRay, RayT, RayPos, SegmentT, SegmentPos))
	{
		return FInputRayHit(RayT);
	}
	return FInputRayHit();
}

void UEdgeOnAxisGizmo::OnBeginHover(const FInputDeviceRay& DevicePos) 
{
	bHovering = true;
}

bool UEdgeOnAxisGizmo::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	FVector3d RayPos, SegmentPos; double RayT, SegmentT;
	if (HitTest(DevicePos.WorldRay, RayT, RayPos, SegmentT, SegmentPos))
	{
		return true;
	}
	return false;
}

void UEdgeOnAxisGizmo::OnEndHover()
{
	bHovering = false;
}
