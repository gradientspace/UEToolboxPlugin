// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "BaseBehaviors/AnyButtonInputBehavior.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "BaseBehaviors/InputBehaviorModifierStates.h"

#include "GSClickOrDragBehavior.generated.h"


class GRADIENTSPACEUETOOLCORE_API IGSMouseWheelDuringDragTarget
{
public:
	virtual ~IGSMouseWheelDuringDragTarget() {}
	virtual void OnMouseWheelScroll(float WheelDelta) = 0;
};



UCLASS()
class GRADIENTSPACEUETOOLCORE_API UGSClickOrDragInputBehavior : public UAnyButtonInputBehavior
{
	GENERATED_BODY()
public:

	virtual void Initialize(IClickBehaviorTarget* ClickTarget, IClickDragBehaviorTarget* DragTarget);
	virtual void SetDragWheelBehavior(IGSMouseWheelDuringDragTarget* WheelTarget);

	FInputBehaviorModifierStates Modifiers;
	TFunction<bool(const FInputDeviceState&)> ModifierCheckFunc = nullptr;
	UPROPERTY()
	bool bUpdateModifiersDuringDrag = false;


	// UInputBehavior implementation
	virtual FInputCaptureRequest WantsCapture(const FInputDeviceState& Input) override;
	virtual FInputCaptureUpdate BeginCapture(const FInputDeviceState& Input, EInputCaptureSide Side) override;
	virtual FInputCaptureUpdate UpdateCapture(const FInputDeviceState& Input, const FInputCaptureData& Data) override;
	virtual void ForceEndCapture(const FInputCaptureData& Data) override;
	
	/** If true, then if the click-mouse-down does not hit a valid click target (determined by IClickBehaviorTarget::IsHitByClick), then the Drag will be initiated */
	UPROPERTY()
	bool bBeginDragIfClickTargetNotHit = true;
	UPROPERTY()
	float ClickDistanceThreshold = 2.5;

	UPROPERTY()
	bool bConsumeEditorCtrlClickDrag = false;

protected:
	IClickBehaviorTarget* ClickTarget = nullptr;
	IClickDragBehaviorTarget* DragTarget = nullptr;
	IGSMouseWheelDuringDragTarget* WheelTarget = nullptr;

	FVector2D MouseDownPosition2D;
	FRay MouseDownRay;

	bool bInDrag = false;
	bool bImmediatelyBeginDragInBeginCapture = false;


};
