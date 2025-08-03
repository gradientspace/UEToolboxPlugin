// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Tools/GSBaseTexturePaintTool.h"
#include "TransformTypes.h"
#include "IntVectorTypes.h"
#include "FrameTypes.h"
#include "Core/SharedPointer.h"
#include "PropertySets/ColorChannelFilterPropertyType.h"
#include "Image/GSImage.h"

#include "PixelPaintTool.generated.h"

class UWorld;
class UPreviewMesh;
class UMaterial;
class UMaterialInterface;
class UTexture2D;
namespace GS { class PixelPaintStroke; }

UCLASS(MinimalAPI)
class UGSMeshPixelPaintToolBuilder : public UMeshSurfacePointToolBuilder
{
	GENERATED_BODY()
public:
	GRADIENTSPACEUETOOLBOX_API virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;

};


UENUM()
enum class EGSPixelPaintToolMode
{
	Pixel = 0
};


USTRUCT()
struct GRADIENTSPACEUETOOLBOX_API FGSPixelToolSettings
{
	GENERATED_BODY()

	/** Spacing between pixel stamps */
	//UPROPERTY(EditAnywhere, Category = "PixelBrush", meta = (ClampMin = 0, ClampMax = 10000, UIMin = 0, UIMax = 100, EditCondition = bVisible, EditConditionHides, HideEditConditionToggle))
	UPROPERTY()
	int Spacing = 0;

	UPROPERTY()
	bool bVisible = true;
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UPixelPaintToolSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY()
	EGSPixelPaintToolMode ActiveTool = EGSPixelPaintToolMode::Pixel;

	UPROPERTY(EditAnywhere, Category = "Pixel", meta = (ShowOnlyInnerProperties))
	FGSPixelToolSettings PixelToolSettings;

	/** Alpha value used to composite brush stroke with underlying image */
	UPROPERTY(EditAnywhere, Category = "Paint Settings", meta=(DisplayName="Stroke Alpha",ClampMin = 0, ClampMax = 1))
	float Alpha = 1.0f;
	
	/** Enable/Disable modification of specific image channels */
	UPROPERTY(EditAnywhere, Category = "Paint Settings")
	FModelingToolsColorChannelFilter ChannelFilter;
};



UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGSMeshPixelPaintTool : public UGSBaseTextureEditTool
{
	GENERATED_BODY()
public:
	UGSMeshPixelPaintTool();
	virtual void Setup() override;
	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const { return true; }
	virtual bool HasAccept() const { return true; }
	virtual bool CanAccept() const { return true; }

	// UMeshSurfacePointTool API
	virtual bool HitTest(const FRay& Ray, FHitResult& OutHit) override;
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnUpdateDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;

	// IHoverBehaviorTarget API
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;


	virtual void SetActiveToolMode(EGSPixelPaintToolMode NewToolMode);
	virtual void PickColorAtCursor(bool bSecondaryColor);
	virtual void SetActiveColor(FLinearColor NewColor, bool bSetSecondary = false, bool bDisableChangeNotification = true);

protected:
	// UGSBaseTextureEditTool API
	virtual void OnActiveTargetTextureUpdated() override;
	virtual void PreTextureChangeOnUndoRedo() override;

protected:
	UPROPERTY()
	TObjectPtr<UGSTextureToolColorSettings> ColorSettings;

	UPROPERTY()
	TObjectPtr<UPixelPaintToolSettings> Settings;


protected:
	bool bInStroke = false;
	double AccumStrokeTime = 0.0;
	double LastStampTime = 0.0;
	FRay LastWorldRay;
	UE::Geometry::FFrame3d LastHitFrame;
	UE::Geometry::FFrame3d LastStampFrame;
	FVector2f LastHitUVPos = FVector2f(-1, -1);
	UE::Geometry::FVector2i LastHitPixel = UE::Geometry::FVector2i(-1, -1);
	int LastHitTriangleID = -1;
	double AccumPathLength = 0;

	void AddStampFromWorldRay(const FRay& WorldRay, double dt);
	void CancelActiveStroke();

	struct PendingStamp
	{
		FRay3d WorldRay;
		UE::Geometry::FFrame3d LocalFrame;
		int TriangleID;
		UE::Geometry::FVector2i Pixel;
		double dt;
	};
	TArray<PendingStamp> PendingPixelStamps;
	void CompletePendingPaintOps();
	void ApplyPendingStamps_Pixel();

	TSharedPtr<GS::Image4f> ImageBuffer;
	TSharedPtr<GS::PixelPaintStroke> ActiveStrokeInfo;


	EGSPixelPaintToolMode CurrentToolMode = EGSPixelPaintToolMode::Pixel;
	void SetPixelToolMode();

	friend class FPixelPaint_PixelsChange;
	void UndoRedoPixelsChange(const FPixelPaint_PixelsChange& Change, bool bUndo);
};