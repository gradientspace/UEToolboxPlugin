// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "InputBehaviorSet.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "InteractiveToolQueryInterfaces.h"
#include "Templates/PimplPtr.h"

#include "GridActor/UGSGridMaterialSet.h"
#include "PropertyTypes/ToggleButtonGroup.h"
#include "PropertyTypes/OrderedOptionSet.h"
#include "PropertyTypes/EnumButtonsGroup.h"

#include "BoxTypes.h"
#include "FrameTypes.h"
#include "IntVectorTypes.h"
#include "IntBoxTypes.h"

#include "ModelGrid/ModelGridTypes.h"

#include "Math/GSIntVector2.h"
#include "Tools/ModelGridEditorToolInteraction.h"

#include "ModelGridEditorTool.generated.h"

class UPreviewMesh;
class UGSModelGridPreview;
class UConstructionPlaneMechanic;
class UDragAlignmentMechanic;
class UMaterialInterface;
class UMaterial;
class UMaterialInstanceDynamic;
class UDynamicMeshComponent;
class UTexture2D;
namespace UE::Geometry { class FDynamicMesh3; }
class AGSModelGridActor;
class UUnitCellRotationGizmo;
class UEdgeOnAxisGizmo;
class UBoxRegionClickGizmo;
namespace GS { struct ModelGridCell; }


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UModelGridEditorToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	virtual UModelGridEditorTool* CreateNewToolOfType(const FToolBuilderState& SceneState) const;
};


class FModelGridInternal;

UENUM()
enum class EModelGridEditorToolType
{
	Model = 0,
	Paint = 1
};


UENUM()
enum class EModelGridDrawEditType
{
	Add,
	Replace,
	Erase
};

UENUM()
enum class EModelGridDrawOrientationType
{
	Fixed = 0,
	AlignedToView = 1
};


UENUM()
enum class EModelGridDrawMode
{
	Single UMETA(DisplayName = "Single Block"),
	Connected UMETA(DisplayName = "Connected Area"),
	Brush3D UMETA(DisplayName = "3D Brush Volume"),
	Brush2D UMETA(DisplayName = "2D Brush Volume"),
	FloodFill2D UMETA(DisplayName = "2D Flood Fill"),
	Rect2D UMETA(DisplayName = "2D Rectangle")
};


UENUM()
enum class EModelGridPaintMode
{
	Single UMETA(DisplayName = "Single Block"),
	Brush2D UMETA(DisplayName = "2D Brush Volume"),
	Brush3D UMETA(DisplayName = "3D Brush Volume"),
	ConnectedLayer UMETA(DisplayName = "Fill Connected Area"),
	FillVolume UMETA(DisplayName = "Fill Connected Volume"),
	Rect2D UMETA(DisplayName = "2D Rectangle"),

	SingleFace UMETA(DisplayName = "Block Face"),
};





UENUM()
enum class EModelGridDrawCellType : uint8
{
	Solid = 0,
	Slab = 1,
	Ramp = 2,
	Corner = 3,
	CutCorner = 4,
	Pyramid = 5,
	Peak = 6,
	Cylinder = 7
};


UENUM()
enum class EModelGridEditorCellDimensionType : uint8
{
	/** Dimension increments are based on 1/4, 16 steps total */
	Quarters = 0,
	/** Dimension increments are based on 1/3, 12 steps total, with two extra-small steps available */
	Thirds = 1
};


UENUM()
enum class EModelGridWorkPlane
{
	Auto = 0 UMETA(DisplayName = "None/Auto"),
	X = 1,
	Y = 2,
	Z = 3
};


UENUM()
enum class EModelGridEditSymmetryMode
{
	None = 0,
	X = 1,
	OffsetX = 2,
	Y = 3,
	OffsetY = 4
};


UENUM()
enum class EModelGridCoordinateDisplay
{
	None,
	ShowXY,
	ShowXYZ
};


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UModelGridEditorToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Primary mode of the Tool. Cycle with [T] hotkey. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	EModelGridEditorToolType EditingMode = EModelGridEditorToolType::Model;

	// for details customization
	TWeakObjectPtr<UModelGridEditorTool> Tool;
};



UCLASS()
class GRADIENTSPACEUETOOLCORE_API UModelGridEditorTool_ModelProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Cell Type/Shape to use for drawing operations */
	UPROPERTY(EditAnywhere, Category = "Model Settings")
	EModelGridDrawCellType CellType = EModelGridDrawCellType::Solid;

	/** Area to fill with the active Cell type. [1] ... [5] keys change mode */
	UPROPERTY(EditAnywhere, Category = "Model Settings")
	EModelGridDrawMode DrawMode = EModelGridDrawMode::Single;

	/** Type of Edit to apply in the fill area */
	UPROPERTY(EditAnywhere, Category = "Model Settings", meta=(DisplayName="EdType"))
	EModelGridDrawEditType EditType = EModelGridDrawEditType::Add;

	/** 2D Work Plane to constrain drawing to, where applicable. [N] cycles options [Shift+N] pick X/Y/Z from cursor */
	UPROPERTY(EditAnywhere, Category = "Model Settings", meta = (DisplayName = "Plane"))
	EModelGridWorkPlane WorkPlane = EModelGridWorkPlane::Auto;

	/** Type of Edit to apply in Drawing Region. [A] cycles options  */
	UPROPERTY(EditAnywhere, Category = "Model Settings", meta = (DisplayName = "Align", EditCondition="DrawMode==EModelGridDrawMode::Single && EditType == EModelGridDrawEditType::Add"))
	EModelGridDrawOrientationType OrientationMode = EModelGridDrawOrientationType::AlignedToView;

	/** 
	 * Enable Symmetric/Mirror Editing & Painting
	 * 
	 * NOTE: Currently symmetry is only properly supported for solid blocks. Other block shapes will not be correctly mirrored.
	 */
	UPROPERTY(EditAnywhere, Category = "Model Settings", meta = ())
	FGSEnumButtonsGroup SymmetryMode;
	
	/** Origin point for symmetric editing [M] set symmetry origin at current cell */
	UPROPERTY(EditAnywhere, Category = "Model Settings", meta = (EditCondition = "SymmetryMode.SelectedIndex != 0"))
	FIntVector2 SymmetryOrigin;


	UPROPERTY(EditAnywhere, Category = "Hit Filtering", meta = (TransientToolProperty))
	FGSToggleButtonGroup HitTargetFilters;

	UPROPERTY()
	bool bHitWorkPlane = true;

	UPROPERTY()
	bool bHitWorld = false;


	// for details customization
	TWeakObjectPtr<UModelGridEditorTool> Tool;
};


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UModelGridEditorTool_PaintProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Paint Settings")
	EModelGridPaintMode PaintMode = EModelGridPaintMode::Single;

	// for details customization
	TWeakObjectPtr<UModelGridEditorTool> Tool;
};



UENUM()
enum class EGSGridEditorTool_FillLayerMode : uint8
{
	All = 0,
	Border = 1,
	Interior = 2
};

UENUM()
enum class EGSGridEditorTool_FillOpMode : uint8
{
	Add = 0,
	Clone = 1
};



UCLASS()
class GRADIENTSPACEUETOOLCORE_API UModelGridEditorTool_FillLayerProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Fill Mode modifies how the target area in the layer will be filled */
	UPROPERTY(EditAnywhere, Category = "Fill Layer Settings", meta = ())
	FGSEnumButtonsGroup FillMode;

	/** Fill Operation controls which cell the target area will be filled with */
	UPROPERTY(EditAnywhere, Category = "Fill Layer Settings", meta = ())
	FGSEnumButtonsGroup FillOperation;

	/** Filter limits which cell types will be included in the layer */
	UPROPERTY(EditAnywhere, Category = "Fill Layer Settings", meta = ())
	FGSEnumButtonsGroup Filter;
};



UENUM()
enum class EModelGridEditorBrushShape : uint8
{
	/** Brush is (approximately) circular/spherical */
	Round = 0,
	/** Brush is square/cube shaped */
	Square = 1
};


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UModelGridEditorTool_BrushProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Radius/Extent of the brush */
	UPROPERTY(EditAnywhere, Category = "Brush Settings", meta = (UIMin = 0, UIMax = 10))
	int BrushRadius = 2;

	/** Shape of the brush stamp */
	UPROPERTY(EditAnywhere, Category = "Brush Settings")
	EModelGridEditorBrushShape BrushShape = EModelGridEditorBrushShape::Round;

	// for details customization
	TWeakObjectPtr<UModelGridEditorTool> Tool;
};



USTRUCT()
struct GRADIENTSPACEUETOOLCORE_API FGridMaterialListItem
{
	GENERATED_BODY()

	//UPROPERTY()
	uint32_t InternalMaterialID = 0;

	UPROPERTY(VisibleAnywhere, Category="Material")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(VisibleAnywhere, Category = "Material")
	FString Name;

	// index into internal list inside Tool
	int32 ToolItemIndex = -1;
};





UCLASS()
class GRADIENTSPACEUETOOLCORE_API UModelGridEditorTool_ColorProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Material/Color", meta = (TransientToolProperty, UIMin = 0))
	int SelectedMaterialIndex = 0;

	UPROPERTY(VisibleAnywhere, Category = "Material/Color", meta = (TransientToolProperty, EditCondition = "bShowMaterialPicker==true", EditConditionHides))
	TArray<FGridMaterialListItem> AvailableMaterials;

	// this is for slate ListView / details customization
	TArray<TSharedPtr<FGridMaterialListItem>> AvailableMaterials_RefList;

	UPROPERTY()
	bool bShowMaterialPicker = false;


	UPROPERTY(EditAnywhere, Category = "Material/Color", meta = (DisplayName = "Primary Color", EditCondition="SelectedMaterialIndex==0"))
	FColor PaintColor = FColor::White;

	UPROPERTY(EditAnywhere, Category = "Material/Color", meta = (DisplayName = "Secondary Color", EditCondition = "SelectedMaterialIndex==0"))
	FColor EraseColor = FColor::White;

	UPROPERTY(EditAnywhere, Category = "Material/Color", meta = (EditCondition = "SelectedMaterialIndex==0"))
	bool bRandomize = false;

	UPROPERTY(EditAnywhere, Category = "Material/Color", meta = (UIMin = 0, UIMax = 180, EditCondition = "SelectedMaterialIndex==0 && bRandomize == true", EditConditionHides))
	float HueRange = 15;

	UPROPERTY(EditAnywhere, Category = "Material/Color", meta = (UIMin = 0, UIMax = 0.5, EditCondition = "SelectedMaterialIndex==0 && bRandomize == true", EditConditionHides))
	float SaturationRange = 0.1;

	UPROPERTY(EditAnywhere, Category = "Material/Color", meta = (UIMin = 0, UIMax = 0.5, EditCondition = "SelectedMaterialIndex==0 && bRandomize == true", EditConditionHides))
	float ValueRange = 0.1;

	// for details customization
	TWeakObjectPtr<UModelGridEditorTool> Tool;
};



UCLASS()
class GRADIENTSPACEUETOOLCORE_API UModelGridStandardRSTCellProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Cell Parameters", meta = (TransientToolProperty))
	FGSEnumButtonsGroup Direction;
	UPROPERTY(meta = (TransientToolProperty))
	int Direction_SelectedIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Cell Parameters", meta = (TransientToolProperty))
	FGSOrderedOptionSet Rotation;

	UPROPERTY(EditAnywhere, Category = "Cell Parameters", meta = (TransientToolProperty))
	FGSEnumButtonsGroup DimensionType;
	UPROPERTY(meta = (TransientToolProperty))
	int DimensionType_SelectedIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Cell Parameters", meta = (TransientToolProperty))
	FGSOrderedOptionSet DimensionX;

	UPROPERTY(EditAnywhere, Category = "Cell Parameters", meta = (TransientToolProperty))
	FGSOrderedOptionSet DimensionY;

	UPROPERTY(EditAnywhere, Category = "Cell Parameters", meta = (TransientToolProperty))
	FGSOrderedOptionSet DimensionZ;

	UPROPERTY(EditAnywhere, Category = "Cell Parameters", meta = (TransientToolProperty))
	FGSOrderedOptionSet TranslateX;

	UPROPERTY(EditAnywhere, Category = "Cell Parameters", meta = (TransientToolProperty))
	FGSOrderedOptionSet TranslateY;

	UPROPERTY(EditAnywhere, Category = "Cell Parameters", meta = (TransientToolProperty))
	FGSOrderedOptionSet TranslateZ;

	UPROPERTY(EditAnywhere, Category = "Cell Parameters", meta = (TransientToolProperty))
	FGSToggleButtonGroup Flips;
	UPROPERTY(meta=(TransientToolProperty))
	bool bFlipX = false;
	UPROPERTY(meta = (TransientToolProperty))
	bool bFlipY = false;
	UPROPERTY(meta = (TransientToolProperty))
	bool bFlipZ = false;

	void InitializeFromCell(const GS::ModelGridCell& Cell);
	void UpdateCell(GS::ModelGridCell& CellToUpdate);

	// for details customization
	TWeakObjectPtr<UModelGridEditorTool> Tool;
};



UCLASS()
class GRADIENTSPACEUETOOLCORE_API UModelGridEditorTool_MiscProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Transforms")
	bool bShowGizmo = false;

	UPROPERTY(EditAnywhere, Category = "Transforms")
	EModelGridCoordinateDisplay ShowCoords = EModelGridCoordinateDisplay::ShowXY;

	UPROPERTY(EditAnywhere, Category = "Transforms")
	bool bShowCellPreview = true;


	UPROPERTY()
	bool bInvert = false;

	UPROPERTY()
	int SubDLevel = 0;


	// for details customization
	TWeakObjectPtr<UModelGridEditorTool> Tool;
};




UCLASS()
class GRADIENTSPACEUETOOLCORE_API UModelGridEditorTool : 
	public UInteractiveTool, 
	public IInteractiveToolNestedAcceptCancelAPI,
	public IClickBehaviorTarget,
	public IClickDragBehaviorTarget, 
	public IHoverBehaviorTarget,
	public IMouseWheelBehaviorTarget,
	public IInteractiveToolCameraFocusAPI
{
	GENERATED_BODY()

public:
	UModelGridEditorTool();
	virtual void Setup() override;
	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const;
	virtual bool HasAccept() const;
	virtual bool CanAccept() const;

	virtual void OnBeginDrag(const FRay& Ray, const FVector2d& ScreenPos, bool bIsSingleClickInteraction);
	virtual void OnUpdateDrag(const FRay& Ray, const FVector2d& ScreenPos);
	virtual void OnEndDrag(const FRay& Ray, const FVector2d& ScreenPos);

	// IClickBehaviorTarget implementation
	virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos) override;
	virtual void OnClicked(const FInputDeviceRay& ClickPos) override;
	bool bInSimulatingClickViaDrag = false;		// this is gross...will be set to true for duration of OnClicked

	// IClickDragBehaviorTarget implementation
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override;
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override;


	// IHoverBehaviorTarget implementation
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;

	// IMouseWheelBehaviorTarget implementation
	virtual FInputRayHit ShouldRespondToMouseWheel(const FInputDeviceRay& CurrentPos) override;
	virtual void OnMouseWheelScrollUp(const FInputDeviceRay& CurrentPos) override;
	virtual void OnMouseWheelScrollDown(const FInputDeviceRay& CurrentPos) override;

	// IInteractiveToolCameraFocusAPI implementation
	virtual bool SupportsWorldSpaceFocusBox() override;
	virtual FBox GetWorldSpaceFocusBox() override;
	virtual bool SupportsWorldSpaceFocusPoint() override;
	virtual bool GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut) override;

	// IInteractiveToolNestedAcceptCancelAPI
	virtual bool SupportsNestedCancelCommand() override { return true; }
	virtual bool CanCurrentlyNestedCancel() override;
	virtual bool ExecuteNestedCancelCommand() override;
	virtual bool SupportsNestedAcceptCommand() override { return false; }
	virtual bool CanCurrentlyNestedAccept() override { return false; }
	virtual bool ExecuteNestedAcceptCommand() override { return false; }


	//virtual void SetShiftToggle(bool bShiftDown);
	virtual bool GetShiftToggle() const { return bShiftToggle; }

	//virtual void SetCtrlToggle(bool bCtrlDown);
	virtual bool GetCtrlToggle() const { return bCtrlToggle; }

	void SetTargetWorld(UWorld* World);
	UWorld* GetTargetWorld();

	void SetExistingActor(AGSModelGridActor* Actor);


	// IGSToggleButtonTarget
	virtual bool GetToggleButtonValue(FString ToggleName);
	virtual void OnToggleButtonToggled(FString ToggleName, bool bNewValue);


	void SetActiveEditingMode(EModelGridEditorToolType NewMode);
	EModelGridEditorToolType GetActiveEditingMode() const { return Settings->EditingMode; }
	DECLARE_MULTICAST_DELEGATE_OneParam(OnActiveEditingModeModifiedEvent, EModelGridEditorToolType);
	OnActiveEditingModeModifiedEvent OnActiveEditingModeModified;

	void SetActiveDrawEditType(EModelGridDrawEditType NewType);
	EModelGridDrawEditType GetActiveDrawEditType() const { return ModelSettings->EditType; }

	void SetActiveWorkPlaneMode(EModelGridWorkPlane NewPlane);
	EModelGridWorkPlane GetActiveWorkPlaneMode() const { return ModelSettings->WorkPlane; }

	void SetActiveDrawOrientationMode(EModelGridDrawOrientationType NewOrientationMode);
	EModelGridDrawOrientationType GetActiveDrawOrientationMode() const { return ModelSettings->OrientationMode; }
	


	// API for configuring tool
	struct FCellTypeItem
	{
		EModelGridDrawCellType CellType;
		FText Name;
		FString StyleName;
	};
	void GetKnownCellTypes(TArray<FCellTypeItem>& CellTypesOut);
	void SetActiveCellType(EModelGridDrawCellType NewType);
	EModelGridDrawCellType GetActiveCellType() const { return ModelSettings->CellType; }

	DECLARE_MULTICAST_DELEGATE_OneParam(OnActiveCellTypeModifiedEvent, EModelGridDrawCellType);
	OnActiveCellTypeModifiedEvent OnActiveCellTypeModified;


	struct FDrawModeItem
	{
		EModelGridDrawMode DrawMode;
		FText Name;
		FString StyleName;
	};
	void GetKnownDrawModes(TArray<FDrawModeItem>& DrawModesOut);
	void SetActiveDrawMode(EModelGridDrawMode DrawMode);
	EModelGridDrawMode GetActiveDrawMode() const { return ModelSettings->DrawMode; }

	DECLARE_MULTICAST_DELEGATE_OneParam(OnActiveDrawModeModifiedEvent, EModelGridDrawMode);
	OnActiveDrawModeModifiedEvent OnActiveDrawModeModified;


	struct FPaintModeItem
	{
		EModelGridPaintMode PaintMode;
		FText Name;
		FString StyleName;
	};
	void GetKnownPaintModes(TArray<FPaintModeItem>& PaintModesOut);
	void SetActivePaintMode(EModelGridPaintMode PaintMode);
	EModelGridPaintMode GetActivePaintMode() const { return PaintSettings->PaintMode; }

	DECLARE_MULTICAST_DELEGATE_OneParam(OnActivePaintModeModifiedEvent, EModelGridPaintMode);
	OnActivePaintModeModifiedEvent OnActivePaintModeModified;


	void SetActiveSelectedMaterialByItem(FGridMaterialListItem Item);
	//! ListIndex is FGridMaterialListItem.ToolItemIndex
	void SetActiveSelectedMaterialByIndex(int32 ListIndex);	


public:

	UPROPERTY()
	TObjectPtr<UModelGridEditorToolProperties> Settings = nullptr;

	UPROPERTY()
	TObjectPtr<UModelGridEditorTool_ModelProperties> ModelSettings = nullptr;

	UPROPERTY()
	TObjectPtr<UModelGridEditorTool_PaintProperties> PaintSettings = nullptr;

	UPROPERTY()
	TObjectPtr<UModelGridEditorTool_BrushProperties> BrushSettings = nullptr;

	UPROPERTY()
	TObjectPtr<UModelGridEditorTool_ColorProperties> ColorSettings = nullptr;

	UPROPERTY()
	TObjectPtr<UModelGridEditorTool_FillLayerProperties> FillLayerSettings = nullptr;
	TSharedPtr<FGSEnumButtonsTarget> FillLayerModeTarget;
	TSharedPtr<FGSEnumButtonsTarget> FillLayerOpTarget;


	UPROPERTY()
	TObjectPtr<UModelGridEditorTool_MiscProperties> MiscSettings = nullptr;

	UPROPERTY()
	TObjectPtr<UModelGridStandardRSTCellProperties> CellSettings_StandardRST = nullptr;


protected:
	UPROPERTY()
	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	UPROPERTY()
	TWeakObjectPtr<AGSModelGridActor> ExistingActor = nullptr;

	UE::Geometry::FVector3i CellSettingsUI_CurrentIndex = UE::Geometry::FVector3i::MaxVector();
	void OnCellSettingsUIChanged();

	void UpdateUI();
	
	FViewCameraState ViewState;

	FRay LastWorldRay;
	bool bShiftToggle = false;
	bool bCtrlToggle = false;
	bool GetWantSelectExisting() const;
	bool InModifyExistingOperationMode() const;

	FVector HoverPositionLocal;		// used to draw hover point/normal...should be done by interaction
	UE::Geometry::FVector3i ActiveHoverCellIndex;
	UE::Geometry::FFrame3d ActiveHoverPlane;
	UE::Geometry::FVector3i ActiveHoverCellFaceNormal;		// this will always be a unit-axis
	bool bHovering = false;
	bool bHaveValidHoverCell = false;						// only used to decide whether or not to draw HUD coords...

	void PickAtCursor(bool bErasePick);
	void CycleDrawPlane();
	void CyclePlacementOrientation();
	void PickDrawPlaneAtCursor();
	void SetSymmetryOriginAtCursor();

	bool bHaveActiveEditCell = false;
	UE::Geometry::FVector3i ActiveEditKey;

	UPROPERTY()
	TObjectPtr<UGSModelGridPreview> PreviewGeometry;

	void OnColumnsModified(const TArray<GS::Vector2i>& Columns);

	UPROPERTY()
	TObjectPtr<UConstructionPlaneMechanic> PlaneMechanic = nullptr;
	UPROPERTY()
	TObjectPtr<UDragAlignmentMechanic> DragAlignmentMechanic = nullptr;
	void UpdateGridFrame(const UE::Geometry::FFrame3d& Frame);

	bool bPreviewMeshDirty = false;
	int GridTimestamp = -1;
	void ValidatePreviewMesh();

	UPROPERTY()
	TObjectPtr<UGSGridMaterialSet> GridMaterialSet;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ActiveMaterial = nullptr;

	TArray<TObjectPtr<UMaterialInterface>> ActiveMaterials;

	TPimplPtr<FModelGridInternal> Internal;
	void UpdateCollider();

	// todo probably need to augment this w/ direction/cellface info, for face painting
	// and view-dependent placement...
	struct DrawUpdatePos
	{
		GS::Vector3i CellIndex;
		GS::Vector3d LocalPosition;
		GS::Vector3d LocalNormal;
	};
	TArray<DrawUpdatePos> DrawUpdateQueue;
	void TryAppendPendingDrawCell(GS::Vector3i CellIndex, GS::Vector3d LocalPosition, GS::Vector3d LocalNormal);
	void ProcessPendingDrawUpdates();

	void UpdateModelingParametersFromSettings(const FRay& WorldRay);
	void UpdatePaintParametersFromSettings();

	UPROPERTY()
	TObjectPtr<UPreviewMesh> DrawPreviewMesh;
	void UpdateDrawPreviewMesh();
	std::vector<GS::Vector3i> DrawPreviewCells;
	void UpdateDrawPreviewVisualization();

	friend class FModelGrid_GridDeltaChange;
	void ForceUpdateOnUndoRedo();
	void BeginGridChange();
	void EndGridChange();


	UPROPERTY()
	TObjectPtr<UUnitCellRotationGizmo> UnitCellRotationGizmo = nullptr;
	void OnRotationAxisClicked(int Axis) { OnRotationAxisClicked(Axis, 1); }
	void OnRotationAxisClicked(int Axis, int Steps = 1);
	void OnRotationHotkey(bool bReverse);

	UPROPERTY()
	TObjectPtr<UEdgeOnAxisGizmo> EdgeGizmoA = nullptr;
	UPROPERTY()
	TObjectPtr<UEdgeOnAxisGizmo> EdgeGizmoB = nullptr;
	void OnEdgeGizmoBegin(UEdgeOnAxisGizmo* Gizmo, double CurrentValue);
	void OnEdgeGizmoUpdate(UEdgeOnAxisGizmo* Gizmo, double CurrentValue);
	void OnEdgeGizmoEnd(UEdgeOnAxisGizmo* Gizmo, double CurrentValue);

	UPROPERTY()
	TObjectPtr<UBoxRegionClickGizmo> BoxPartsClickGizmo = nullptr;
	void OnBoxPartsClick(UBoxRegionClickGizmo* Gizmo, int RegionType, FVector3d RegionNormals[3]);

	void UpdateGizmosForActiveEditingCell();
	void UpdateLiveGizmosOnExternalChange();
	void ConfigureCellRotationGizmo(const GS::ModelGridCell& EditCell);
	void ConfigureCellDimensionGizmos(const GS::ModelGridCell& EditCell);
	void ConfigureBoxPartsClickGizmo(const GS::ModelGridCell& EditCell);

	TSharedPtr<FGSEnumButtonsTarget> DirectionsEnumTarget;
	TSharedPtr<FGSEnumButtonsTarget> DimensionTypeEnumTarget;
	void OnSelectedDimensionModeChanged();


	TSharedPtr<ModelGridPaintCellsInteraction> PaintCellsInteraction;
	TSharedPtr<ModelGridEraseCellsInteraction> EraseCellsInteraction;
	TSharedPtr<ModelGridReplaceCellsInteraction> ReplaceCellsInteraction;
	TSharedPtr<ModelGridAppendCellsInteraction> AppendCellsInteraction;
	TSharedPtr<ModelGridSelectCellInteraction> SelectCellInteraction;
	TSharedPtr<ModelGridEditorToolInteraction> ActiveInteraction;
	void InitializeInteractions();
	void UpdateActiveInteraction();

	friend class ToolStateAPIImplementation;
	TSharedPtr<IModelGridEditToolStateAPI> ToolStateAPI;

	int NestedCancelCount = 0;


public:
	static EModelGridDrawCellType GridTypeToToolType(GS::EModelGridCellType CellType, bool& bValid);
	static GS::EModelGridCellType ToolTypeToGridType(EModelGridDrawCellType CellType);
};




