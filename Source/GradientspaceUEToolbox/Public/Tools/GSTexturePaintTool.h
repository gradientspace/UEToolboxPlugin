// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Tools/GSBaseTexturePaintTool.h"
#include "TransformTypes.h"
#include "IntVectorTypes.h"
#include "FrameTypes.h"
#include "BoxTypes.h"
#include "Core/SharedPointer.h"
#include "PropertySets/ColorChannelFilterPropertyType.h"
#include "PropertyTypes/GSColorPalette.h"
#include "PropertyTypes/ToggleButtonGroup.h"

#include "Assets/UGSTextureImageStamp.h"		// need this for enums, should fix

#include "GSTexturePaintTool.generated.h"

class UWorld;
class UMaterial;
class UMaterialInterface;
class UTexture2D;
class UGSImageStamp;
class UGSImageStampAsset;
class UBrushStampIndicator;

class FSurfaceTexelCache;
class FSurfacePaintStroke;
namespace GS { class SubGridMask2; }
namespace GS { class SurfaceTexelColorCache; }




UCLASS(MinimalAPI)
class UGSMeshTexturePaintToolBuilder : public UMeshSurfacePointToolBuilder
{
	GENERATED_BODY()
public:
	GRADIENTSPACEUETOOLBOX_API virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;

};








UENUM()
enum class ETexturePaintBrushAreaType : uint8
{
	/** Brush paints any texels inside a sphere around the cursor */
	Connected,
	/** Brush paints any texels geometrically connected to the triangle under the cursor */
	Volumetric
};

UENUM()
enum class EGSTexturePaintToolMode
{
	Brush = 0
};

USTRUCT()
struct GRADIENTSPACEUETOOLBOX_API FGSTexturePaintBrushSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Brush", meta=(EditCondition = bVisible, EditConditionHides, HideEditConditionToggle))
	ETexturePaintBrushAreaType AreaType = ETexturePaintBrushAreaType::Volumetric;

	// this introduces a dependency on MeshModelingToolsetExp (!)
	//UPROPERTY(EditAnywhere, Category = "Brush", meta=(EditCondition = bVisible, EditConditionHides, HideEditConditionToggle))
	//FBrushToolRadius BrushSize;

	UPROPERTY(EditAnywhere, Category = "Brush", meta=(EditCondition = bVisible, EditConditionHides, HideEditConditionToggle, ClampMin = 0, ClampMax = 1))
	double BrushSize = 0.25;

	/** percentage width of the falloff band at the outer edge of the brush  */
	UPROPERTY(EditAnywhere, Category = "Brush", meta = (ClampMin = 0, ClampMax = 100, EditCondition = bVisible, EditConditionHides, HideEditConditionToggle))
	int Falloff = 50;

	/** Speed that brush stamps accumulate during a single stroke */
	UPROPERTY(EditAnywhere, Category = "Brush", meta = (ClampMin = 0, ClampMax = 100, EditCondition = bVisible, EditConditionHides, HideEditConditionToggle))
	int Flow = 100;

	/** Amount of drag applied during brush stroke, to produce smoother results */
	UPROPERTY(EditAnywhere, Category = "Brush", meta = (ClampMin = 0, ClampMax = 100, EditCondition = bVisible, EditConditionHides, HideEditConditionToggle))
	int Lazyness = 25;

	/** Spacing between brush stroke stamps, as a percentage of the BrushSize (0% = continuous flow, 100% = adjacent circles) */
	UPROPERTY(EditAnywhere, Category = "Brush", meta = (ClampMin = 0, ClampMax = 10000, UIMin = 0, UIMax = 100, EditCondition = bVisible, EditConditionHides, HideEditConditionToggle))
	int Spacing = 10;

	UPROPERTY()
	bool bVisible = true;
};

UCLASS()
class GRADIENTSPACEUETOOLBOX_API UTexturePaintToolSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	//UPROPERTY(EditAnywhere, Category="Tool")
	UPROPERTY()
	EGSTexturePaintToolMode ActiveTool = EGSTexturePaintToolMode::Brush;

	UPROPERTY(EditAnywhere, Category = "Brush", meta = (ShowOnlyInnerProperties))
	FGSTexturePaintBrushSettings BrushToolSettings;

	/**
	 * Current Stamp texture applied during each stamp along the brush stroke
	 */
	UPROPERTY(EditAnywhere, Category = "Stamp Settings")
	TObjectPtr<UGSImageStampAsset> BrushStamp;

	/** Rotation angle applied to the Stamp */
	UPROPERTY(EditAnywhere, Category = "Stamp Settings", meta = (ClampMin = 0, ClampMax = 360))
	int Rotation = 0;

	/**
	 * Configure behavior of the brush stamp
	 */
	UPROPERTY(EditAnywhere, Category = "Stamp Settings", meta = (TransientToolProperty))
	FGSToggleButtonGroup StampToggles;

	UPROPERTY()
	bool bIgnoreFalloff = false;
	UPROPERTY()
	bool bAlignToStroke = true;


	/** Alpha value used to composite entire brush stroke with underlying image */
	UPROPERTY(EditAnywhere, Category = "Paint Settings", meta=(DisplayName="Stroke Alpha",ClampMin = 0, ClampMax = 1))
	float Alpha = 1.0f;
	
	/** Enable/Disable modification of specific image channels */
	UPROPERTY(EditAnywhere, Category = "Paint Settings")
	FModelingToolsColorChannelFilter ChannelMask;

	/**
	 * Limit painted area inside the brush stamp based on mesh attributes
	 */
	UPROPERTY(EditAnywhere, Category = "Paint Settings", meta = (TransientToolProperty))
	FGSToggleButtonGroup RegionFilters;

	UPROPERTY()
	bool bRestrictToUVIsland = false;
	UPROPERTY()
	bool bRestrictToGroup = false;
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UTexturePaintToolLayerSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "OverLayer", meta = (TransientToolProperty))
	FGSActionButtonGroup LayerActions;

	/**
	 * Alpha blending value for the current overlay paint layer
	 */
	UPROPERTY(EditAnywhere, Category = "OverLayer", meta = (DisplayName = "Layer Alpha", ClampMin = 0, ClampMax = 1))
	float LayerAlpha = 1.0f;
};



UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGSMeshTexturePaintTool : public UGSBaseTextureEditTool
{
	GENERATED_BODY()
public:
	UGSMeshTexturePaintTool();
	virtual void Setup() override;
	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const { return true; }
	virtual bool HasAccept() const { return true; }
	virtual bool CanAccept() const { return true; }

	// IClickDragBehaviorTarget API, override to save 2D positions
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;

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


	virtual void SetActiveToolMode(EGSTexturePaintToolMode NewToolMode);
	virtual void PickColorAtCursor(bool bSecondaryColor);
	virtual void SetActiveColor(FLinearColor NewColor, bool bSetSecondary = false, bool bDisableChangeNotification = true);

	virtual double GetBrushSizeWorld() const;
	virtual double GetBrushSizeLocal() const;

protected:
	// UGSBaseTextureEditTool API
	virtual void PreActiveTargetTextureUpdate(bool bInTransaction) override;
	virtual void OnActiveTargetTextureUpdated() override;
	virtual void PreTextureChangeOnUndoRedo() override;

	//! this is used for texture visualization
	virtual UE::Geometry::FIndex4i GetActiveChannelViewMask() const override;

protected:
	UPROPERTY()
	TObjectPtr<UGSTextureToolColorSettings> ColorSettings;

	UPROPERTY()
	TObjectPtr<UTexturePaintToolSettings> Settings;

	UPROPERTY()
	TObjectPtr<UTexturePaintToolLayerSettings> LayerSettings;

protected:
	bool bInStroke = false;
	double AccumStrokeTime = 0.0;
	double LastStampTime = 0.0;
	FRay CurWorldRay;
	FVector2d CurWorldRayCursorPos;
	FRay LastStampWorldRay;
	FVector2d LastStampWorldRayCursorPos;
	UE::Geometry::FFrame3d LastHitFrame;
	UE::Geometry::FFrame3d LastStampFrame;
	FVector2f LastHitUVPos = FVector2f(-1,-1);
	UE::Geometry::FVector2i LastHitPixel = UE::Geometry::FVector2i(-1,-1);
	int LastHitTriangleID = -1;
	double AccumPathLength = 0;

	void AddStampFromWorldRay(const FVector2d& CursorPos, const FRay& WorldRay, double dt);
	void CancelActiveStroke();

	struct PendingStamp
	{
		FRay3d WorldRay;
		FVector2d CursorPos;
		UE::Geometry::FFrame3d LocalFrame;
		int TriangleID;
		UE::Geometry::FVector2i Pixel;
		double dt;
	};
	TArray<PendingStamp> PendingPixelStamps;
	TArray<PendingStamp> AccumStrokeStamps;
	PendingStamp LastAppliedStamp;
	void CompletePendingPaintOps();
	void ApplyPendingStamps_Brush();
	
	TSharedPtr<FSurfaceTexelCache> SurfaceCache;
	TSharedPtr<GS::SurfaceTexelColorCache> ColorCache;
	TSharedPtr<FSurfacePaintStroke> ActiveStrokeInfo;
	TSharedPtr<GS::SubGridMask2> ImageUpdateTracker;

	// temporary buffers used to compute connected brush ROI
	TArray<int32> StampStartROI;
	TArray<int32> StampTempROIBuffer;
	TSet<int32> StampTriangleROI;

	PendingStamp PendingInitialStamp;
	TArray<PendingStamp> OrientedStampQueue;
	bool bIsOrientedStroke = false;
	int PendingOrientedStrokeState = -1;
	void PushPendingOrientedStrokesToQueue(bool bIsEndStroke);

	// temporary painting layer that is composited with base layer
	TSharedPtr<GS::SurfaceTexelColorCache> LayerColorCache;
	bool bPaintLayerEnabled = false;
	void EnableTemporaryPaintLayer(bool bEmitChange);
	void CancelTemporaryPaintLayer(bool bEmitChange);
	void CommitTemporaryPaintLayer(bool bEmitChange);

	// wrappers for base vs layer color-cache
	TSharedPtr<GS::SurfaceTexelColorCache>& GetActiveColorCache();
	FLinearColor CalcCompositedLayerColor(int TexelPointID, FLinearColor LayerColor, double LayerAlpha) const;
	void RebuildBufferFromColorCache(bool bPushFullUpdate);
	void RebuildBufferFromLayerColorCache(bool bPushFullUpdate, double LayerAlpha);

	EGSTexturePaintToolMode CurrentToolMode = EGSTexturePaintToolMode::Brush;
	void SetBrushToolMode();

	friend class FTexturePaint_BrushColorChange;
	void UndoRedoBrushStroke(const FTexturePaint_BrushColorChange& Change, bool bUndo);
	friend class FTexturePaint_TempLayerChange;
	void UndoRedoLayerChange(const FTexturePaint_TempLayerChange& Change, bool bUndo);


	UPROPERTY()
	TObjectPtr<UBrushStampIndicator> BrushIndicator;

	UE::Geometry::FFrame3d IndicatorWorldFrame;

	TInterval<double> BrushWorldSizeRange;


	void OnStampTextureModified();
	bool bHaveStampTexture = false;
	UE::Geometry::FVector2i StampImageDims;
	TArray<FColor> StampImageBuffer4b;
	bool bStampIsSRGB = false;
	EGSImageStampColorMode StampColorMode = EGSImageStampColorMode::RGBAColor;
	EGSImageStampChannel StampAlphaChannel = EGSImageStampChannel::Alpha;
};