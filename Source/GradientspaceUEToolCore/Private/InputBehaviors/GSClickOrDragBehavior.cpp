// Copyright Gradientspace Corp. All Rights Reserved.
#include "InputBehaviors/GSClickOrDragBehavior.h"

void UGSClickOrDragInputBehavior::Initialize(IClickBehaviorTarget* ClickTargetIn, IClickDragBehaviorTarget* DragTargetIn)
{
	this->ClickTarget = ClickTargetIn;
	this->DragTarget = DragTargetIn;
}

void UGSClickOrDragInputBehavior::SetDragWheelBehavior(IGSMouseWheelDuringDragTarget* WheelTargetIn)
{
	WheelTarget = WheelTargetIn;
}

FInputCaptureRequest UGSClickOrDragInputBehavior::WantsCapture(const FInputDeviceState& Input)
{
	bImmediatelyBeginDragInBeginCapture = false;
	if (IsPressed(Input) && (ModifierCheckFunc == nullptr || ModifierCheckFunc(Input)))
	{
		FInputRayHit ClickHitResult = (ClickTarget) ? ClickTarget->IsHitByClick(GetDeviceRay(Input)) : FInputRayHit();
		if (ClickHitResult.bHit)
		{
			return FInputCaptureRequest::Begin(this, EInputCaptureSide::Any, ClickHitResult.HitDepth);
		}
		else if (bBeginDragIfClickTargetNotHit)
		{
			FInputRayHit DragHitResult = DragTarget->CanBeginClickDragSequence(GetDeviceRay(Input));
			if (DragHitResult.bHit)
			{
				bImmediatelyBeginDragInBeginCapture = true;
				return FInputCaptureRequest::Begin(this, EInputCaptureSide::Any, DragHitResult.HitDepth);
			}
		}
	}

	// optionally consume editor ctrl-click-drag to avoid confusion in the tool
	if ((Input.Mouse.Left.bPressed || Input.Mouse.Middle.bPressed || Input.Mouse.Right.bPressed)
		&& Input.bCtrlKeyDown
		&& bConsumeEditorCtrlClickDrag)
	{
		return FInputCaptureRequest::Begin(this, EInputCaptureSide::Any);
	}

	return FInputCaptureRequest::Ignore();
}


FInputCaptureUpdate UGSClickOrDragInputBehavior::BeginCapture(const FInputDeviceState& Input, EInputCaptureSide eSide)
{
	if ( ClickTarget != nullptr )
	{
		Modifiers.UpdateModifiers(Input, ClickTarget);
	}
	if ( DragTarget != nullptr )
	{
		Modifiers.UpdateModifiers(Input, DragTarget);
	}

	ensure(Input.IsFromDevice(EInputDevices::Mouse));	// todo: handle other devices
	MouseDownPosition2D = Input.Mouse.Position2D;	
	MouseDownRay = Input.Mouse.WorldRay;
	bInDrag = false;

	// If the WantsCapture() hit-test did not hit the target, then start the drag action directly, instead of starting
	// a click and then switching to a drag (could alternately repeat the hit-test here...)
	if (bImmediatelyBeginDragInBeginCapture)
	{
		bInDrag = true;
		if (DragTarget)
			DragTarget->OnClickPress(GetDeviceRay(Input));
	}

	return FInputCaptureUpdate::Begin(this, EInputCaptureSide::Any);
}


FInputCaptureUpdate UGSClickOrDragInputBehavior::UpdateCapture(const FInputDeviceState& Input, const FInputCaptureData& Data)
{
	if ( ClickTarget != nullptr )
	{
		Modifiers.UpdateModifiers(Input, ClickTarget);
	}
	if ( DragTarget != nullptr )
	{
		Modifiers.UpdateModifiers(Input, DragTarget);
	}

	if (bInDrag == false)
	{
		float MouseMovement = static_cast<float>(FVector2D::Distance(Input.Mouse.Position2D, MouseDownPosition2D));
		if (MouseMovement > ClickDistanceThreshold)
		{
			FInputDeviceState StartInput = Input;					// the input state in WantsCapture/BeginCapture
			StartInput.Mouse.Position2D = MouseDownPosition2D;		
			StartInput.Mouse.WorldRay = MouseDownRay;
			if ( DragTarget )
			{
				FInputRayHit DragHitResult = DragTarget->CanBeginClickDragSequence(GetDeviceRay(StartInput));
				if (DragHitResult.bHit)
				{
					bInDrag = true;
					if (DragTarget)
						DragTarget->OnClickPress(GetDeviceRay(Input));
				}
			}
		}
	}

	if (IsReleased(Input))
	{
		if (bInDrag)
		{
			if (DragTarget)
				DragTarget->OnClickRelease(GetDeviceRay(Input));
			bInDrag = false;
		}
		else
		{
			if (ClickTarget && ClickTarget->IsHitByClick(GetDeviceRay(Input)).bHit )
			{
				ClickTarget->OnClicked(GetDeviceRay(Input));
			}
		}

		return FInputCaptureUpdate::End();
	}
	else if ( bInDrag )
	{
		if (DragTarget != nullptr)
			DragTarget->OnClickDrag(GetDeviceRay(Input));

		if (Input.Mouse.WheelDelta != 0 && WheelTarget != nullptr)
			WheelTarget->OnMouseWheelScroll(Input.Mouse.WheelDelta);

		return FInputCaptureUpdate::Continue();
	}

	return FInputCaptureUpdate::Continue();
}


void UGSClickOrDragInputBehavior::ForceEndCapture(const FInputCaptureData& data)
{
	if (bInDrag) {
		if ( DragTarget )
			DragTarget->OnTerminateDragSequence();
		bInDrag = false;
	}
}
