// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveGizmo.h"
#include "InteractiveGizmoBuilder.h"
#include "InputState.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"

#include "FrameTypes.h"
#include "BoxTypes.h"

#include "BoxRegionClickGizmo.generated.h"

class UPreviewMesh;

UCLASS()
class GRADIENTSPACEUETOOLCORE_API UBoxRegionClickGizmoBuilder : public UInteractiveGizmoBuilder
{
	GENERATED_BODY()
public:
	virtual UInteractiveGizmo* BuildGizmo(const FToolBuilderState& SceneState) const override;
};


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UBoxRegionClickGizmo : public UInteractiveGizmo, public IClickBehaviorTarget, public IHoverBehaviorTarget
{
	GENERATED_BODY()

public:
	static const FString GizmoName;

	void Initialize(UWorld* TargetWorld);

	void InitializeParameters(
		const UE::Geometry::FFrame3d& WorldFrame,
		const UE::Geometry::FAxisAlignedBox3d& LocalBox);

	void SetEnabled(bool bEnabled);

	enum class EBoxRegionType
	{
		None = 0,
		Face = 1,
		Edge = 2,
		Corner = 3
	};

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnBoxRegionClicked, UBoxRegionClickGizmo*, int RegionType, FVector3d RegionNormals[3]);
	FOnBoxRegionClicked OnBoxRegionClick;

	virtual void Setup() override;
	virtual void Shutdown() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;
	virtual void Tick(float DeltaTime) override;

	// IClickBehaviorTarget implementation
	virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos) override;
	virtual void OnClicked(const FInputDeviceRay& ClickPos) override;

	// IHoverBehaviorTarget implementation
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;

	bool HitTest(FRay3d WorldRay, double& RayT, FVector3d& RayPos, int& HitGroupID) const;

	bool bDrawBoxWireframe = true;

protected:
	bool bEnabled = true;

	UE::Geometry::FFrame3d FrameWorld;
	UE::Geometry::FAxisAlignedBox3d FrameBox;

	UPROPERTY()
	TObjectPtr<UPreviewMesh> PreviewMesh;

	UE::Geometry::FAxisAlignedBox3d CurrentMeshBox;

	int CurrentHitGroup = -1;

	FViewCameraState CurCameraState;

	bool bHovering = false;
};
