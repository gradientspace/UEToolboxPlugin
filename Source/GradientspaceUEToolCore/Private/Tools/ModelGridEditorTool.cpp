// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/ModelGridEditorTool.h"
#include "InteractiveToolManager.h"
#include "ModelingObjectsCreationAPI.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseWheelBehavior.h"
#include "InputBehaviors/GSClickOrDragBehavior.h"
#include "ToolSetupUtil.h"
#include "ToolDataVisualizer.h"
#include "Selection/ToolSelectionUtil.h"
#include "PreviewMesh.h"

#include "Mechanics/ConstructionPlaneMechanic.h"
#include "Mechanics/DragAlignmentMechanic.h"
#include "BaseGizmos/CombinedTransformGizmo.h"

#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Util/IndexUtil.h"
#include "Utility/GSUEMathUtil.h"
#include "Core/DynamicMeshGenericAPI.h"

#include "GridActor/ModelGridActor.h"
#include "Color/GSColor3b.h"
#include "ModelGrid/ModelGridCell.h"
#include "ModelGrid/ModelGridUtil.h"
#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridEditor.h"
#include "ModelGrid/ModelGridEditMachine.h"
#include "ModelGrid/ModelGridMeshCache.h"
#include "ModelGrid/ModelGridCollision.h"
#include "ModelGrid/ModelGridSerializer.h"
#include "GridActor/GSModelGridPreview.h"
#include "Math/GSVectorMath.h"

#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "InteractiveGizmoManager.h"
#include "Gizmos/UnitCellRotationGizmo.h"
#include "Gizmos/EdgeOnAxisGizmo.h"
#include "Gizmos/BoxRegionClickGizmo.h"

#include "Engine/Font.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"  // for GEngine->GetSmallFont()
#include "SceneView.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"		// FActorLabelUtilities
#endif

#include <numeric>

#define LOCTEXT_NAMESPACE "UModelGridEditorTool"


using namespace UE::Geometry;
using namespace GS;


static const int UModelGridEditorTool_ShiftModifier = 1;
static const int UModelGridEditorTool_CtrlModifier = 2;


class FModelGrid_GridDeltaChange : public FToolCommandChange
{
public:
	std::unique_ptr<GS::ModelGridDeltaChange> GridChange;
	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;
	virtual FString ToString() const override { return TEXT("FModelGrid_GridDeltaChange"); }

	virtual ~FModelGrid_GridDeltaChange() {
		if (GridChange)
			GS::ModelGridDeltaChange::DeleteChangeFromExternalDLL(GridChange.release());
	}
};



bool UModelGridEditorToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	// require initial actor
	return SceneState.SelectedActors.Num() == 1 &&
		(Cast<AGSModelGridActor>(SceneState.SelectedActors[0]) != nullptr);
}

UInteractiveTool* UModelGridEditorToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UModelGridEditorTool* Tool = CreateNewToolOfType(SceneState);
	Tool->SetTargetWorld(SceneState.World);
	if (SceneState.SelectedActors.Num() == 1)
	{
		AGSModelGridActor* Actor = Cast<AGSModelGridActor>(SceneState.SelectedActors[0]);
		if (Actor)
		{
			Tool->SetExistingActor(Actor);
		}
	}

	return Tool;
}

UModelGridEditorTool* UModelGridEditorToolBuilder::CreateNewToolOfType(const FToolBuilderState& SceneState) const
{
	return NewObject<UModelGridEditorTool>(SceneState.ToolManager);
}



// todo: work on getting rid of this class...
class FModelGridInternal
{
public:
	ModelGrid Builder;
	GS::SharedPtr<FReferenceSetMaterialMap> GridMaterialMap;

	Frame3d GridFrame;
	
	ModelGridEditMachine EditSM;
	RandomizeColorModifier ColorModifier_Randomize;

	ModelGridMeshCache MeshCache;
	FDynamicMesh3BuilderFactory MeshBuilderFactory;

	ModelGridCollider Collider;

	AxisBox3i PendingMeshUpdateRegion;
	AxisBox3i PendingColliderUpdateRegion;

	FModelGridInternal()
	{
	}

	void Initialize()
	{
		EditSM.SetCurrentGrid(Builder);
		
		MeshBuilderFactory.bEnableMaterials = true;
		MeshBuilderFactory.bEnableUVs = true;
		MeshCache.Initialize(Builder.GetCellDimensions(), &MeshBuilderFactory);
		if ( GridMaterialMap )
			MeshCache.SetMaterialMap(GridMaterialMap);
		MeshCache.bIncludeAllBlockBorderFaces = true;

		PendingMeshUpdateRegion = Builder.GetModifiedRegionBounds(0);

		Collider.Initialize(Builder);
		PendingColliderUpdateRegion = Builder.GetModifiedRegionBounds(0);
	}

	void AccumulateMeshUpdateRegion()
	{
		GridChangeInfo DirtyRegion = EditSM.GetIncrementalChange(true);
		if (DirtyRegion.bModified)
		{
			if (DirtyRegion.ModifiedRegion != AxisBox3i::Empty())
			{
				PendingMeshUpdateRegion.Contain(DirtyRegion.ModifiedRegion.Min);
				PendingMeshUpdateRegion.Contain(DirtyRegion.ModifiedRegion.Max);

				PendingColliderUpdateRegion.Contain(DirtyRegion.ModifiedRegion.Min);
				PendingColliderUpdateRegion.Contain(DirtyRegion.ModifiedRegion.Max);
			}
		}
	}

	FVector GetHitFaceNormalForCell(FRay Ray, bool bIsWorldRay, Vector3i CellKey)
	{
		Ray3d LocalRay = (bIsWorldRay) ?
			(Ray3d)((FFrame3d)GridFrame).ToFrame(Ray) : (Ray3d)Ray;

		double RayParam; Vector3d BoxHitPos; Vector3d HitFaceNormal;
		bool bHit = Builder.ComputeCellBoxIntersection(LocalRay, CellKey, RayParam, BoxHitPos, HitFaceNormal);
		ensure(bHit);
		return HitFaceNormal;
	}

};



static FVector3d GetWorkPlaneNormal(EModelGridWorkPlane WorkPlane, const FFrame3d& GridFrame, const FVector3d& RayDirection, bool bLocal)
{
	int AxisIndex = (int)WorkPlane - 1;
	FVector3d PlaneNormal(0, 0, 0);
	PlaneNormal[AxisIndex] = 1;
	PlaneNormal = GridFrame.FromFrameVector(PlaneNormal);
	if (PlaneNormal.Dot(RayDirection) > 0)
		PlaneNormal = -PlaneNormal;
	if (bLocal)
		PlaneNormal = GridFrame.ToFrameVector(PlaneNormal);
	return PlaneNormal;
}


UModelGridEditorTool::UModelGridEditorTool()
{
	SetToolDisplayName(LOCTEXT("ToolName", "Model Grid Editor"));
}

void UModelGridEditorTool::Setup()
{
	UInteractiveTool::Setup();

	bShiftToggle = false;
	bCtrlToggle = false;

	// add input behaviors
	//UClickDragInputBehavior* DragBehavior = NewObject<UClickDragInputBehavior>();
	UGSClickOrDragInputBehavior* DragBehavior = NewObject<UGSClickOrDragInputBehavior>();
	DragBehavior->Modifiers.RegisterModifier(UModelGridEditorTool_ShiftModifier, FInputDeviceState::IsShiftKeyDown);
	DragBehavior->Modifiers.RegisterModifier(UModelGridEditorTool_CtrlModifier, FInputDeviceState::IsCtrlKeyDown);
	DragBehavior->SetDefaultPriority(FInputCapturePriority::DEFAULT_TOOL_PRIORITY);
	DragBehavior->Initialize(this, this);
	DragBehavior->bConsumeEditorCtrlClickDrag = true;
	AddInputBehavior(DragBehavior);

	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
	HoverBehavior->Modifiers.RegisterModifier(UModelGridEditorTool_ShiftModifier, FInputDeviceState::IsShiftKeyDown);
	HoverBehavior->Modifiers.RegisterModifier(UModelGridEditorTool_CtrlModifier, FInputDeviceState::IsCtrlKeyDown);
	HoverBehavior->SetDefaultPriority(FInputCapturePriority::DEFAULT_TOOL_PRIORITY);
	HoverBehavior->Initialize(this);
	AddInputBehavior(HoverBehavior);

	//UMouseWheelInputBehavior* WheelBehavior = NewObject<UMouseWheelInputBehavior>();
	//WheelBehavior->ModifierCheckFunc = [](const FInputDeviceState& state) { return state.bShiftKeyDown; };
	//WheelBehavior->Initialize(this);
	//AddInputBehavior(WheelBehavior);

	Internal = MakePimpl<FModelGridInternal>();

	PreviewGeometry = NewObject<UGSModelGridPreview>(this);
	PreviewGeometry->Initialize(this, GetTargetWorld());

	if (UMaterial* GridMaterial = LoadObject<UMaterial>(nullptr, TEXT("/GradientspaceUEToolbox/Materials/M_GridEditMaterial")))	{
		ActiveMaterial = UMaterialInstanceDynamic::Create(GridMaterial, this);
	}

	GridMaterialSet = NewObject<UGSGridMaterialSet>(this);
	AGSModelGridActor* InitFromActor = ExistingActor.Get();
	if (InitFromActor)
	{
		UGSModelGrid* ModelGridContainer = InitFromActor->GetGrid();
		ModelGridContainer->ProcessGrid([&](const ModelGrid& Grid)
		{
			// this ia temporary test of serialization and should be removed...
			MemorySerializer Serializer;
			Serializer.BeginWrite();
			ModelGridSerializer::Serialize(Grid, Serializer);
			Serializer.BeginRead();
			ModelGridSerializer::Restore(Internal->Builder, Serializer);
			//Internal->Builder = Grid;
		});
		InitFromActor->GetRootComponent()->SetVisibility(false, true);
		Internal->GridFrame = (Frame3d)FFrame3d(InitFromActor->GetTransform());


		UGSGridMaterialSet* MaterialSet = InitFromActor->GetGridComponent()->GetGridMaterials();
		if (MaterialSet)
			GridMaterialSet->CopyFrom(MaterialSet);
	}
	else
	{
		FVector3d CellDimensions(50, 50, 50);
		Internal->Builder.Initialize(CellDimensions);
		Internal->GridFrame = Frame3d::Identity();
	}

	// use our vertexcolor mat instead of the default
	GridMaterialSet->SetDefaultMaterial(ActiveMaterial);
	Internal->GridMaterialMap = MakeSharedPtr<FReferenceSetMaterialMap>();
	UGSGridMaterialSet::BuildMaterialMapForSet(GridMaterialSet, *Internal->GridMaterialMap);

	Internal->Initialize();
	Internal->EditSM.OnGridModifiedCallback = [this]() {
		bPreviewMeshDirty = true;
		GridTimestamp++;
	};

	ActiveMaterials.Reset();
	for (auto Material : Internal->GridMaterialMap->MaterialList)
		ActiveMaterials.Add(Material);
	PreviewGeometry->SetMaterials(ActiveMaterials);

	ActiveMaterial->SetVectorParameterValue(TEXT("CellDimensions"), Internal->Builder.CellSize());

	PlaneMechanic = NewObject<UConstructionPlaneMechanic>(this);
	PlaneMechanic->Setup(this);
	PlaneMechanic->CanUpdatePlaneFunc = [this]() { return true; };
	PlaneMechanic->OnPlaneChanged.AddLambda([this]() { UpdateGridFrame(PlaneMechanic->Plane); });
	PlaneMechanic->Initialize(GetTargetWorld(), (FFrame3d)Internal->GridFrame);
	PlaneMechanic->PlaneTransformGizmo->bUseContextCoordinateSystem = false;
	PlaneMechanic->PlaneTransformGizmo->CurrentCoordinateSystem = EToolContextCoordinateSystem::Local;
	PlaneMechanic->PlaneTransformGizmo->SetIsNonUniformScaleAllowedFunction( []() { return false; } );
	PlaneMechanic->PlaneTransformGizmo->SetVisibility(false);

	DragAlignmentMechanic = NewObject<UDragAlignmentMechanic>(this);
	DragAlignmentMechanic->Setup(this);
	DragAlignmentMechanic->AddToGizmo(PlaneMechanic->PlaneTransformGizmo);

	DrawPreviewMesh = NewObject<UPreviewMesh>(this);
	DrawPreviewMesh->CreateInWorld(GetTargetWorld(), FTransform());
	DrawPreviewMesh->SetVisible(false);
	DrawPreviewMesh->SetMaterial(ToolSetupUtil::GetDefaultEditVolumeMaterial());

	Settings = NewObject<UModelGridEditorToolProperties>(this);
	Settings->Tool = this;
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);
	Settings->WatchProperty(Settings->EditingMode, [&](EModelGridEditorToolType NewMode) { SetActiveEditingMode(NewMode); });
	
	ModelSettings = NewObject<UModelGridEditorTool_ModelProperties>(this);
	ModelSettings->Tool = this;
	ModelSettings->RestoreProperties(this);
	AddToolPropertySource(ModelSettings);

	TSharedPtr<FGSEnumButtonsTarget> SymmetryTarget = ModelSettings->SymmetryMode.SimpleInitialize();
	ModelSettings->SymmetryMode.AddItem({ TEXT("SymmetryMode_None"), (int)EModelGridEditSymmetryMode::None, LOCTEXT("SymmetryMode_None", "No Symmetry"),
		LOCTEXT("SymmetryMode_None_Tooltip", "No symmetric edits will be applied") });
	ModelSettings->SymmetryMode.AddItem({ TEXT("SymmetryMode_X"), (int)EModelGridEditSymmetryMode::X, LOCTEXT("SymmetryMode_X", "X"),
		LOCTEXT("SymmetryMode_X_Tooltip", "Mirror cells along the X axis") });
	ModelSettings->SymmetryMode.AddItem({ TEXT("SymmetryMode_OffsetX"), (int)EModelGridEditSymmetryMode::OffsetX, LOCTEXT("SymmetryMode_OffsetX", "X (Offset)"),
		LOCTEXT("SymmetryMode_OffsetX_Tooltip", "Mirror cells along the X axis (origin row is symmetric with itself)") });
	ModelSettings->SymmetryMode.AddItem({ TEXT("SymmetryMode_Y"), (int)EModelGridEditSymmetryMode::Y, LOCTEXT("SymmetryMode_Y", "Y"),
		LOCTEXT("SymmetryMode_Y_Tooltip", "Mirror cells along the Y axis") });
	ModelSettings->SymmetryMode.AddItem({ TEXT("SymmetryMode_OffsetY"), (int)EModelGridEditSymmetryMode::OffsetY, LOCTEXT("SymmetryMode_OffsetY", "Y (Offset)"),
		LOCTEXT("SymmetryMode_OffsetY_Tooltip", "Mirror cells along the Y axis (origin row is symmetric with itself)") });
	ModelSettings->SymmetryMode.ValidateAfterSetup();
	ModelSettings->SymmetryMode.bShowLabel = false;

	PaintSettings = NewObject<UModelGridEditorTool_PaintProperties>(this);
	PaintSettings->Tool = this;
	PaintSettings->RestoreProperties(this);
	AddToolPropertySource(PaintSettings);

	BrushSettings = NewObject<UModelGridEditorTool_BrushProperties>(this);
	BrushSettings->Tool = this;
	BrushSettings->RestoreProperties(this);
	AddToolPropertySource(BrushSettings);

	ColorSettings = NewObject<UModelGridEditorTool_ColorProperties>(this);
	ColorSettings->Tool = this;
	ColorSettings->RestoreProperties(this);

	for (int k = 0; k < Internal->GridMaterialMap->InternalIDList.Num(); ++k)
	{
		FGridMaterialListItem Item;
		Item.InternalMaterialID = Internal->GridMaterialMap->InternalIDList[k];
		Item.Material = Internal->GridMaterialMap->MaterialList[k];
		Item.Name = Internal->GridMaterialMap->NameList[k];
		Item.ToolItemIndex = k;
		ColorSettings->AvailableMaterials.Add(Item);
		ColorSettings->AvailableMaterials_RefList.Add(MakeShared<FGridMaterialListItem>(Item));
	};
	ColorSettings->SelectedMaterialIndex = 0;
	ColorSettings->bShowMaterialPicker = (ColorSettings->AvailableMaterials.Num() > 1);

	AddToolPropertySource(ColorSettings);

	FillLayerSettings = NewObject<UModelGridEditorTool_FillLayerProperties>(this);
	FillLayerSettings->RestoreProperties(this);
	AddToolPropertySource(FillLayerSettings);

	FillLayerModeTarget = FillLayerSettings->FillMode.SimpleInitialize();
	FillLayerSettings->FillMode.AddItem({ TEXT("FillLayerMode_All"), (int)EGSGridEditorTool_FillLayerMode::All, LOCTEXT("FillLayerMode_All", "All"),
		LOCTEXT("FillLayerMode_All_Tooltip", "Fill entire layer") });
	FillLayerSettings->FillMode.AddItem({ TEXT("FillLayerMode_Border"), (int)EGSGridEditorTool_FillLayerMode::Border, LOCTEXT("FillLayerMode_Border", "Border"),
		LOCTEXT("FillLayerMode_Border_Tooltip", "Fill border cells of layer") });
	FillLayerSettings->FillMode.AddItem({ TEXT("FillLayerMode_Interior"), (int)EGSGridEditorTool_FillLayerMode::Interior, LOCTEXT("FillLayerMode_Interior", "Interior"),
		LOCTEXT("FillLayerMode_Interior_Tooltip", "Fill interior (non-border) cells of layer") });
	FillLayerSettings->FillMode.ValidateAfterSetup();

	FillLayerOpTarget = FillLayerSettings->FillOperation.SimpleInitialize();
	FillLayerSettings->FillOperation.AddItem({ TEXT("FillLayerOpMode_Add"), (int)EGSGridEditorTool_FillOpMode::Add, LOCTEXT("FillLayerOpMode_Add", "Active"),
		LOCTEXT("FillLayerOpMode_Add_Tooltip", "Fill the layer with the active cell type / parameters") });
	FillLayerSettings->FillOperation.AddItem({ TEXT("FillLayerOpMode_Clone"), (int)EGSGridEditorTool_FillOpMode::Clone, LOCTEXT("FillLayerOpMode_Clone", "Clone Initial"),
		LOCTEXT("FillLayerOpMode_Clone_Tooltip", "Fill the layer by cloning cells from the initial source layer") });
	FillLayerSettings->FillOperation.ValidateAfterSetup();

	TSharedPtr<FGSEnumButtonsTarget> FillLayerFilterTarget = FillLayerSettings->Filter.SimpleInitialize();
	FillLayerSettings->Filter.AddItem({ TEXT("FillLayerFilter_None"), (int)ModelGridEditMachine::ERegionFillFilter::NoFilter, LOCTEXT("FillLayerFilter_None", "None"),
		LOCTEXT("FillLayerFilter_None_Tooltip", "Fill all cells in the layer, ie filtering is disabled") });
	FillLayerSettings->Filter.AddItem({ TEXT("FillLayerFilter_OnlySolid"), (int)ModelGridEditMachine::ERegionFillFilter::OnlySolidCells, LOCTEXT("FillLayerFilter_OnlySolid", "Only Solid"),
		LOCTEXT("FillLayerFilter_OnlyFilled_Tooltip", "Only fill cells that are Solid in the source layer") });
	FillLayerSettings->Filter.ValidateAfterSetup();


	// construct property set for standard RST-transform cell type

	CellSettings_StandardRST = NewObject<UModelGridStandardRSTCellProperties>(this);
	CellSettings_StandardRST->Tool = this;

	DirectionsEnumTarget = MakeShared<FGSEnumButtonsTarget>();
	DirectionsEnumTarget->GetSelectedItemIdentifier = [this]() { return (int)CellSettings_StandardRST->Direction_SelectedIndex; };
	DirectionsEnumTarget->IsItemEnabled = [this](int) { return true; };
	DirectionsEnumTarget->IsItemVisible = [this](int) { return true; };
	DirectionsEnumTarget->SetSelectedItem = [this](int NewItem) { CellSettings_StandardRST->Direction_SelectedIndex = NewItem; };
	CellSettings_StandardRST->Direction.AddItems({ TEXT("+Z"), TEXT("+Y"), TEXT("+X"), TEXT("-X"), TEXT("-Y"), TEXT("-Z") });
	CellSettings_StandardRST->Direction.Target = DirectionsEnumTarget;

	TSharedPtr<FGSToggleButtonTarget> FlipsTarget = MakeShared<FGSToggleButtonTarget>();
	FlipsTarget->GetToggleValue = [this](FString ToggleName) { return this->GetToggleButtonValue(ToggleName); };
	FlipsTarget->SetToggleValue = [this](FString ToggleName, bool bNewValue) { this->OnToggleButtonToggled(ToggleName, bNewValue); };
	CellSettings_StandardRST->Flips.Target = FlipsTarget;
	CellSettings_StandardRST->Flips.AddToggle("RSTCell.FlipX",
		LOCTEXT("FlipX", "Flip X"),
		LOCTEXT("FlipXTooltip", "Flip in the X axis direction"));
	CellSettings_StandardRST->Flips.AddToggle("RSTCell.FlipY",
		LOCTEXT("FlipY", "Flip Y"),
		LOCTEXT("FlipYTooltip", "Flip in the Y axis direction"));
	CellSettings_StandardRST->Flips.AddToggle("RSTCell.FlipZ",
		LOCTEXT("FlipZ", "Flip Z"),
		LOCTEXT("FlipZTooltip", "Flip in the Z axis direction"));

	DimensionTypeEnumTarget = MakeShared<FGSEnumButtonsTarget>();
	DimensionTypeEnumTarget->GetSelectedItemIdentifier = [this]() { return (int)CellSettings_StandardRST->DimensionType_SelectedIndex; };
	DimensionTypeEnumTarget->IsItemEnabled = [this](int) { return true; };
	DimensionTypeEnumTarget->IsItemVisible = [this](int) { return true; };
	DimensionTypeEnumTarget->SetSelectedItem = [this](int NewItem) { CellSettings_StandardRST->DimensionType_SelectedIndex = NewItem; };
	CellSettings_StandardRST->DimensionType.AddItem({ TEXT("Quarters"), (int)EModelGridEditorCellDimensionType::Quarters, LOCTEXT("DimensionType_Quarters_Label", "Quarters"),
		LOCTEXT("DimensionType_Quarters_Tooltip", "Dimension increments are based on 1/4, 16 steps total") });
	CellSettings_StandardRST->DimensionType.AddItem({ TEXT("Thirds"), (int)EModelGridEditorCellDimensionType::Thirds, LOCTEXT("DimensionType_Thirds_Label", "Thirds"),
		LOCTEXT("DimensionType_Thirds_Tooltip", "Dimension increments are based on 1/3, 12 steps total, with two extra-small steps available") });
	CellSettings_StandardRST->DimensionType.Target = DimensionTypeEnumTarget;

	CellSettings_StandardRST->Rotation.AddOptions({ TEXT("0"), TEXT("90"), TEXT("180"), TEXT("270") });

	AddToolPropertySource(CellSettings_StandardRST);
	SetToolPropertySourceEnabled(CellSettings_StandardRST, false);
	CellSettings_StandardRST->WatchProperty(CellSettings_StandardRST->Direction_SelectedIndex, [&](int) { OnCellSettingsUIChanged(); });
	CellSettings_StandardRST->WatchProperty(CellSettings_StandardRST->Rotation.SelectedIndex, [&](int) { OnCellSettingsUIChanged(); });
	CellSettings_StandardRST->WatchProperty(CellSettings_StandardRST->DimensionType_SelectedIndex, [&](int) { OnSelectedDimensionModeChanged(); OnCellSettingsUIChanged(); });
	OnSelectedDimensionModeChanged();		// update dimension string lists
	CellSettings_StandardRST->WatchProperty(CellSettings_StandardRST->DimensionX.SelectedIndex, [&](int) { OnCellSettingsUIChanged(); });
	CellSettings_StandardRST->WatchProperty(CellSettings_StandardRST->DimensionY.SelectedIndex, [&](int) { OnCellSettingsUIChanged(); });
	CellSettings_StandardRST->WatchProperty(CellSettings_StandardRST->DimensionZ.SelectedIndex, [&](int) { OnCellSettingsUIChanged(); });
	CellSettings_StandardRST->WatchProperty(CellSettings_StandardRST->TranslateX.SelectedIndex, [&](int) { OnCellSettingsUIChanged(); });
	CellSettings_StandardRST->WatchProperty(CellSettings_StandardRST->TranslateY.SelectedIndex, [&](int) { OnCellSettingsUIChanged(); });
	CellSettings_StandardRST->WatchProperty(CellSettings_StandardRST->TranslateZ.SelectedIndex, [&](int) { OnCellSettingsUIChanged(); });
	CellSettings_StandardRST->WatchProperty(CellSettings_StandardRST->bFlipX, [&](bool) { OnCellSettingsUIChanged(); });
	CellSettings_StandardRST->WatchProperty(CellSettings_StandardRST->bFlipY, [&](bool) { OnCellSettingsUIChanged(); });
	CellSettings_StandardRST->WatchProperty(CellSettings_StandardRST->bFlipZ, [&](bool) { OnCellSettingsUIChanged(); });

	MiscSettings = NewObject<UModelGridEditorTool_MiscProperties>(this);
	MiscSettings->Tool = this;
	MiscSettings->RestoreProperties(this);
	AddToolPropertySource(MiscSettings);
	MiscSettings->WatchProperty(MiscSettings->bInvert, [&](bool) { bPreviewMeshDirty = true; });
	MiscSettings->WatchProperty(MiscSettings->SubDLevel, [&](int) { bPreviewMeshDirty = true; });

	bPreviewMeshDirty = true;
	GridTimestamp = 0;
	ValidatePreviewMesh();
	PreviewGeometry->BeginCollisionUpdate();
	UpdateCollider();


	// add box rotation gizmo
	GetToolManager()->GetPairedGizmoManager()->RegisterGizmoType(UUnitCellRotationGizmo::GizmoName, NewObject<UUnitCellRotationGizmoBuilder>());
	UnitCellRotationGizmo = GetToolManager()->GetPairedGizmoManager()->CreateGizmo<UUnitCellRotationGizmo>(UUnitCellRotationGizmo::GizmoName, TEXT("UnitCellRotationGizmo"), this);
	UnitCellRotationGizmo->OnAxisClicked.AddUObject(this, &UModelGridEditorTool::OnRotationAxisClicked);
	UnitCellRotationGizmo->SetEnabled(false);

	// add box rotation gizmo
	GetToolManager()->GetPairedGizmoManager()->RegisterGizmoType(UEdgeOnAxisGizmo::GizmoName, NewObject<UEdgeOnAxisGizmoBuilder>());
	EdgeGizmoA = GetToolManager()->GetPairedGizmoManager()->CreateGizmo<UEdgeOnAxisGizmo>(UEdgeOnAxisGizmo::GizmoName, TEXT("EdgeGizmoA"), this);
	EdgeGizmoA->SetEnabled(false);
	EdgeGizmoA->OnBeginDrag.AddUObject(this, &UModelGridEditorTool::OnEdgeGizmoBegin);
	EdgeGizmoA->OnUpdateDrag.AddUObject(this, &UModelGridEditorTool::OnEdgeGizmoUpdate);
	EdgeGizmoA->OnEndDrag.AddUObject(this, &UModelGridEditorTool::OnEdgeGizmoEnd);

	EdgeGizmoB = GetToolManager()->GetPairedGizmoManager()->CreateGizmo<UEdgeOnAxisGizmo>(UEdgeOnAxisGizmo::GizmoName, TEXT("EdgeGizmoB"), this);
	EdgeGizmoB->SetEnabled(false);
	EdgeGizmoB->OnBeginDrag.AddUObject(this, &UModelGridEditorTool::OnEdgeGizmoBegin);
	EdgeGizmoB->OnUpdateDrag.AddUObject(this, &UModelGridEditorTool::OnEdgeGizmoUpdate);
	EdgeGizmoB->OnEndDrag.AddUObject(this, &UModelGridEditorTool::OnEdgeGizmoEnd);

	GetToolManager()->GetPairedGizmoManager()->RegisterGizmoType(UBoxRegionClickGizmo::GizmoName, NewObject<UBoxRegionClickGizmoBuilder>());
	BoxPartsClickGizmo = GetToolManager()->GetPairedGizmoManager()->CreateGizmo<UBoxRegionClickGizmo>(UBoxRegionClickGizmo::GizmoName, TEXT("BoxPartsGizmo"), this);
	BoxPartsClickGizmo->Initialize(GetTargetWorld());
	BoxPartsClickGizmo->SetEnabled(false);
	BoxPartsClickGizmo->OnBoxRegionClick.AddUObject(this, &UModelGridEditorTool::OnBoxPartsClick);

	// init random...
	Internal->ColorModifier_Randomize.RandomHelper = GS::RandomStream( (uint32)(FDateTime::Now().GetTicks() & 0xFFFFFFFF) );

	TSharedPtr<FGSToggleButtonTarget> FiltersTarget = MakeShared<FGSToggleButtonTarget>();
	FiltersTarget->GetToggleValue = [this](FString ToggleName) { return this->GetToggleButtonValue(ToggleName); };
	FiltersTarget->SetToggleValue = [this](FString ToggleName, bool bNewValue) { this->OnToggleButtonToggled(ToggleName, bNewValue); };
	ModelSettings->HitTargetFilters.Target = FiltersTarget;
	ModelSettings->HitTargetFilters.AddToggle("Model.HitWorkPlane",
		LOCTEXT("HitWorkPlane", "Hit Ground Plane"),
		LOCTEXT("HitWorkPlaneTooltip", "Allow drawing rays to hit the \"ground plane\" of the grid, at z=0"));
	ModelSettings->HitTargetFilters.AddToggle("Model.HitWorld",
		LOCTEXT("HitWorld", "Hit World"),
		LOCTEXT("HitWorldTooltip", "Allow drawing rays to hit the objects in the world"));


	InitializeInteractions();

	UpdateActiveInteraction();
	UpdateUI();
}


void UModelGridEditorTool::OnSelectedDimensionModeChanged()
{
	TArray<FString> TransformLabels;
	for (int i = 0; i < ModelGridCellData_StandardRST::MaxTranslate; ++i)
		TransformLabels.Add(FString::Printf(TEXT("%f"), (double)i / (double)(ModelGridCellData_StandardRST::MaxTranslate + 1)));
	CellSettings_StandardRST->TranslateX.AddOptions(TransformLabels);
	CellSettings_StandardRST->TranslateY.AddOptions(TransformLabels);
	CellSettings_StandardRST->TranslateZ.AddOptions(TransformLabels);

	if (CellSettings_StandardRST->DimensionType_SelectedIndex == (int)EModelGridEditorCellDimensionType::Quarters)
	{
		TArray<FString> DimensionLabels_Quarters = {
			TEXT(".0625   1/16"), 
			TEXT(".125    1/8 "), 
			TEXT(".1875   3/16"), 
			TEXT(".25     1/4 "),
			TEXT(".3125   5/16"), 
			TEXT(".375    3/8 "), 
			TEXT(".4375   7/16"), 
			TEXT(".5      1/2 "),
			TEXT(".5625   9/16"), 
			TEXT(".625    5/8 "), 
			TEXT(".6875  11/16"), 
			TEXT(".75     3/4 "),
			TEXT(".8125  13/16"), 
			TEXT(".875    7/8 "), 
			TEXT(".9375  15/16"), 
			TEXT("1.0") };
		CellSettings_StandardRST->DimensionX.ReplaceOptions(DimensionLabels_Quarters);
		CellSettings_StandardRST->DimensionY.ReplaceOptions(DimensionLabels_Quarters);
		CellSettings_StandardRST->DimensionZ.ReplaceOptions(DimensionLabels_Quarters);
	}
	else
	{
		TArray<FString> DimensionLabels_Thirds = {
			TEXT(".0125   1/80"), 
			TEXT(".025    1/40"),  
			TEXT(".0833   1/12"),  
			TEXT(".1666   1/6 "),
			TEXT(".025    1/4 "),  
			TEXT(".3333   1/3 "), 
			TEXT(".4166   5/12"),  
			TEXT(".5      1/2 "),
			TEXT(".5833   7/12"), 
			TEXT(".6666   2/3 "), 
			TEXT(".75     3/4 "),  
			TEXT(".8333   5/6 "),
			TEXT(".9166  11/12"), 
			TEXT(".975   39/40"),  
			TEXT(".9875  79/80"),  
			TEXT("1.0"), };
		CellSettings_StandardRST->DimensionX.ReplaceOptions(DimensionLabels_Thirds);
		CellSettings_StandardRST->DimensionY.ReplaceOptions(DimensionLabels_Thirds);
		CellSettings_StandardRST->DimensionZ.ReplaceOptions(DimensionLabels_Thirds);
	}


	NotifyOfPropertyChangeByTool(CellSettings_StandardRST);
}

void UModelGridEditorTool::RegisterActions(FInteractiveToolActionSet& ActionSet)
{
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 1,
		TEXT("CycleToolMode"),
		LOCTEXT("CycleToolModeUIName", "Model/Paint"),
		LOCTEXT("CycleToolModeTooltip", "Swap between Model and Paint modes"),
		EModifierKey::None, EKeys::T,
		[this]() { 
			SetActiveEditingMode( (Settings->EditingMode == EModelGridEditorToolType::Model) ? EModelGridEditorToolType::Paint : EModelGridEditorToolType::Model );
		});

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 500,
		TEXT("PickColorUnderCursor"),
		LOCTEXT("PickColorUnderCursor", "Pick Color"),
		LOCTEXT("PickColorUnderCursorTooltip", "Switch the Color to the color of the block currently under the cursor"),
		EModifierKey::Shift, EKeys::G,
		[this]() { PickAtCursor(false); });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 501,
		TEXT("PickEraseColorUnderCursor"),
		LOCTEXT("PickEraseColorUnderCursor", "Pick Erase Color"),
		LOCTEXT("PickEraseColorUnderCursorTooltip", "Switch the Erase Color to the color of the block currently under the cursor"),
		EModifierKey::Control | EModifierKey::Shift, EKeys::G,
		[this]() { PickAtCursor(true); });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 510,
		TEXT("CycleDrawPlane"),
		LOCTEXT("CycleDrawPlane", "Cycle Draw Plane"),
		LOCTEXT("CycleDrawPlaneTooltip", "Cycle through the available draw planes"),
		EModifierKey::None, EKeys::N,
		[this]() { CycleDrawPlane(); });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 511,
		TEXT("PickDrawPlaneUnderCursor"),
		LOCTEXT("PickDrawPlaneUnderCursor", "Pick Draw Plane"),
		LOCTEXT("PickDrawPlaneUnderCursorTooltip", "Set the Drawing Plane based on the block currently under the cursor"),
		EModifierKey::Shift, EKeys::N,
		[this]() { PickDrawPlaneAtCursor(); });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 515,
		TEXT("CycleOrientationMode"),
		LOCTEXT("CycleOrientationMode", "Cycle Placement Orientation Mode"),
		LOCTEXT("CycleOrientationModeTooltip", "Cycle through the available placement orientation modes"),
		EModifierKey::None, EKeys::A,
		[this]() { CyclePlacementOrientation(); });


	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 516,
		TEXT("PickMirrorOrigin"),
		LOCTEXT("PickMirrorOrigin", "Set Symmetry Origin"),
		LOCTEXT("PickMirrorOriginTooltip", "Set the Symmetry/Mirror Origin Location using the currently-hovered cell"),
		EModifierKey::None, EKeys::M,
		[this]() { SetSymmetryOriginAtCursor(); });


	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 530,
		TEXT("CycleRotationZ"),
		LOCTEXT("CycleRotationZ", "Rotate around Z"),
		LOCTEXT("CycleRotationZTooltip", "Cycle through the available Z rotations"),
		EModifierKey::None, EKeys::R,
		[this]() { OnRotationHotkey(false); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 531,
		TEXT("CycleRotationZReverse"),
		LOCTEXT("CycleRotationZReverse", "Rotate around -Z"),
		LOCTEXT("CycleRotationZReverseTooltip", "Cycle through the available Z rotations (reversed)"),
		EModifierKey::Shift, EKeys::R,
		[this]() { OnRotationHotkey(true); });


	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 540,
		TEXT("ResetToSolidCell"),
		LOCTEXT("ResetToSolidCell", "Solid Cell Type"),
		LOCTEXT("ResetToSolidCellTooltip", "Reset to the Solid cell type"),
		EModifierKey::None, EKeys::Q,
		[this]() { SetActiveCellType(EModelGridDrawCellType::Solid); });


	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 550,
		TEXT("SetTool_Single"),
		LOCTEXT("SetTool_Single", "Set Active Tool - Single Block"),
		LOCTEXT("SetTool_Single_Tooltip", "Set the active tool to the Single Block tool"),
		EModifierKey::None, EKeys::One,
		[this]() { (Settings->EditingMode == EModelGridEditorToolType::Paint) ? SetActivePaintMode(EModelGridPaintMode::Single) : SetActiveDrawMode(EModelGridDrawMode::Single); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 551,
		TEXT("SetTool_Brush2D"),
		LOCTEXT("SetTool_Brush2D", "Set Active Tool - 2D Brush"),
		LOCTEXT("SetTool_Brush2D_Tooltip", "Set the active tool to the 2D Brush tool"),
		EModifierKey::None, EKeys::Two,
		[this]() { (Settings->EditingMode == EModelGridEditorToolType::Paint) ? SetActivePaintMode(EModelGridPaintMode::Brush2D) : SetActiveDrawMode(EModelGridDrawMode::Brush2D); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 552,
		TEXT("SetTool_Brush3D"),
		LOCTEXT("SetTool_Brush3D", "Set Active Tool - 3D Brush"),
		LOCTEXT("SetTool_Brush3D_Tooltip", "Set the active tool to the 3D Brush tool"),
		EModifierKey::None, EKeys::Three,
		[this]() { (Settings->EditingMode == EModelGridEditorToolType::Paint) ? SetActivePaintMode(EModelGridPaintMode::Brush3D) : SetActiveDrawMode(EModelGridDrawMode::Brush3D); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 553,
		TEXT("SetTool_FillLayer"),
		LOCTEXT("SetTool_FillLayer", "Set Active Tool - Fill Layer"),
		LOCTEXT("SetTool_FillLayer_Tooltip", "Set the active tool to the Fill Layer tool"),
		EModifierKey::None, EKeys::Four,
		[this]() { (Settings->EditingMode == EModelGridEditorToolType::Paint) ? SetActivePaintMode(EModelGridPaintMode::ConnectedLayer) : SetActiveDrawMode(EModelGridDrawMode::Connected); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 554,
		TEXT("SetTool_Rect2D"),
		LOCTEXT("SetTool_Rect2D", "Set Active Tool - Rectangle"),
		LOCTEXT("SetTool_Rect2D_Tooltip", "Set the active tool to the Rectangle tool"),
		EModifierKey::None, EKeys::Five,
		[this]() { (Settings->EditingMode == EModelGridEditorToolType::Paint) ? SetActivePaintMode(EModelGridPaintMode::Rect2D) : SetActiveDrawMode(EModelGridDrawMode::Rect2D); });

}

bool UModelGridEditorTool::GetToggleButtonValue(FString ToggleName)
{
	if (ToggleName == TEXT("Model.HitWorkPlane"))
		return ModelSettings->bHitWorkPlane;
	else if (ToggleName == TEXT("Model.HitWorld"))
		return ModelSettings->bHitWorld;
	else if (ToggleName == TEXT("RSTCell.FlipX"))
		return CellSettings_StandardRST->bFlipX;
	else if (ToggleName == TEXT("RSTCell.FlipY"))
		return CellSettings_StandardRST->bFlipY;
	else if (ToggleName == TEXT("RSTCell.FlipZ"))
		return CellSettings_StandardRST->bFlipZ;

	return false;
}

void UModelGridEditorTool::OnToggleButtonToggled(FString ToggleName, bool bNewValue)
{
	if (ToggleName == TEXT("Model.HitWorkPlane"))
		ModelSettings->bHitWorkPlane = bNewValue;
	else if (ToggleName == TEXT("Model.HitWorld"))
		ModelSettings->bHitWorld = bNewValue;
	else if (ToggleName == TEXT("RSTCell.FlipX"))
		CellSettings_StandardRST->bFlipX = bNewValue;
	else if (ToggleName == TEXT("RSTCell.FlipY"))
		CellSettings_StandardRST->bFlipY = bNewValue;
	else if (ToggleName == TEXT("RSTCell.FlipZ"))
		CellSettings_StandardRST->bFlipZ = bNewValue;
}



void UModelGridEditorTool::Shutdown(EToolShutdownType ShutdownType)
{
	Settings->SaveProperties(this);
	ModelSettings->SaveProperties(this);
	PaintSettings->SaveProperties(this);
	BrushSettings->SaveProperties(this);
	ColorSettings->SaveProperties(this);
	FillLayerSettings->SaveProperties(this);
	MiscSettings->SaveProperties(this);

	DrawPreviewMesh->Disconnect();
	DrawPreviewMesh = nullptr;

	DragAlignmentMechanic->Shutdown();
	DragAlignmentMechanic = nullptr;
	PlaneMechanic->Shutdown();
	PlaneMechanic = nullptr;

	UInteractiveGizmoManager* GizmoManager = GetToolManager()->GetPairedGizmoManager();
	GizmoManager->DestroyAllGizmosByOwner(this);
	GizmoManager->DeregisterGizmoType(UUnitCellRotationGizmo::GizmoName);
	GizmoManager->DeregisterGizmoType(UEdgeOnAxisGizmo::GizmoName);
	GizmoManager->DeregisterGizmoType(UBoxRegionClickGizmo::GizmoName);

	if (PreviewGeometry)
	{
		PreviewGeometry->Shutdown();

		AGSModelGridActor* UpdateActor = ExistingActor.Get();
		if (UpdateActor)
		{
			UpdateActor->GetRootComponent()->SetVisibility(true, true);
		}

		if (ShutdownType == EToolShutdownType::Accept)
		{
			if (UpdateActor != nullptr)
			{
				GetToolManager()->BeginUndoTransaction(LOCTEXT("ShutdownEdit", "Update Grid"));

				// make sure transactions are enabled on target grid object
				UGSModelGrid* GridWrapper = UpdateActor->GetGrid();
				GridWrapper->SetEnableTransactions(true);

				// do we need to mark asset dirty?? make it transactional??
				
				UpdateActor->Modify();
				GridWrapper->Modify();

				GridWrapper->EditGrid([&](ModelGrid& EditGrid)
				{
					EditGrid = Internal->Builder;
				});

				UpdateActor->SetActorTransform( ((FFrame3d)Internal->GridFrame).ToFTransform() );

#if WITH_EDITOR
				GridWrapper->PostEditChange();
				UpdateActor->PostEditChange();
#endif
				
				ToolSelectionUtil::SetNewActorSelection(GetToolManager(), UpdateActor);

				GetToolManager()->EndUndoTransaction();
			}
			else
			{
				GetToolManager()->BeginUndoTransaction(LOCTEXT("ShutdownEdit", "New Grid Actor"));

				FActorSpawnParameters SpawnInfo;
				AGSModelGridActor* NewActor = GetTargetWorld()->SpawnActor<AGSModelGridActor>(SpawnInfo);
				if (NewActor)
				{
					NewActor->GetGrid()->EditGrid([&](ModelGrid& EditGrid)
					{
						EditGrid = Internal->Builder;
					});

					NewActor->SetActorTransform( ((FFrame3d)Internal->GridFrame).ToFTransform() );

#if WITH_EDITOR
					FActorLabelUtilities::SetActorLabelUnique(NewActor, TEXT("Grid"));
					NewActor->PostEditChange();
#endif

					ToolSelectionUtil::SetNewActorSelection(GetToolManager(), NewActor);
				}

				GetToolManager()->EndUndoTransaction();
			}
		}
	}

}


bool UModelGridEditorTool::HasCancel() const
{
	return true;
}

bool UModelGridEditorTool::HasAccept() const
{
	return true;
}

bool UModelGridEditorTool::CanAccept() const
{
	return ( Internal->Builder.GetModifiedRegionBounds(0).IsValid());
}



void UModelGridEditorTool::UpdateUI()
{
	bool bBrushSettingsVisible = false;
	bool bFillLayerSettingsVisible = false;
	if (Settings->EditingMode == EModelGridEditorToolType::Model)
	{
		SetToolPropertySourceEnabled(ModelSettings, true);
		SetToolPropertySourceEnabled(PaintSettings, false);
		bBrushSettingsVisible = (ModelSettings->DrawMode == EModelGridDrawMode::Brush2D || ModelSettings->DrawMode == EModelGridDrawMode::Brush3D);
		bFillLayerSettingsVisible = (ModelSettings->DrawMode == EModelGridDrawMode::Connected);
	}
	else
	{
		SetToolPropertySourceEnabled(ModelSettings, false);
		SetToolPropertySourceEnabled(PaintSettings, true);
		bBrushSettingsVisible = (PaintSettings->PaintMode == EModelGridPaintMode::Brush2D || PaintSettings->PaintMode == EModelGridPaintMode::Brush3D);
		bFillLayerSettingsVisible = (PaintSettings->PaintMode == EModelGridPaintMode::ConnectedLayer);
	}

	SetToolPropertySourceEnabled(BrushSettings, bBrushSettingsVisible);
	SetToolPropertySourceEnabled(FillLayerSettings, bFillLayerSettingsVisible);

	if (Settings->EditingMode == EModelGridEditorToolType::Model)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("ToolHotkeysMessage", "[T] Model/Paint  [Ctrl] Erase  [Shift] Select  [1-5] Tool  [Q] SolidCell  [Shift+G] Pick Cell   [Shift+N] Pick WorkPlane [N] Cycle WorkPlane"),
			EToolMessageLevel::UserNotification);
	}
	else
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("ToolHotkeysMessage", "[T] Model/Paint  [Ctrl] Erase  [1-5] Tool  [Shift+G] Pick Color  [Ctrl+Shift+G] Pick Secondary  [Shift+N] Pick WorkPlane [N] Cycle WorkPlane"),
			EToolMessageLevel::UserNotification);
	}
}



void UModelGridEditorTool::SetActiveEditingMode(EModelGridEditorToolType NewMode)
{
	if (Settings->EditingMode != NewMode)
	{
		Settings->EditingMode = NewMode;
		OnActiveEditingModeModified.Broadcast(NewMode);

		bHaveActiveEditCell = false;
		UpdateActiveInteraction();
		UpdateGizmosForActiveEditingCell();
		UpdateUI();
	}
}


void UModelGridEditorTool::SetActiveDrawEditType(EModelGridDrawEditType NewType)
{
	if (ModelSettings->EditType != NewType)
	{
		ModelSettings->EditType = NewType;
		//OnActiveEditingModeModified.Broadcast(NewType);

		UpdateActiveInteraction();
	}
}

void UModelGridEditorTool::SetActiveWorkPlaneMode(EModelGridWorkPlane NewPlane)
{
	if (ModelSettings->WorkPlane != NewPlane)
	{
		ModelSettings->WorkPlane = NewPlane;
		//OnActiveEditingModeModified.Broadcast(NewType);
	}
}

void UModelGridEditorTool::SetActiveDrawOrientationMode(EModelGridDrawOrientationType NewOrientationMode)
{
	if (ModelSettings->OrientationMode != NewOrientationMode)
	{
		ModelSettings->OrientationMode = NewOrientationMode;
		//OnActiveEditingModeModified.Broadcast(NewType);
	}
}


void UModelGridEditorTool::GetKnownCellTypes(TArray<FCellTypeItem>& CellTypesOut)
{
	CellTypesOut.Add({ EModelGridDrawCellType::Solid, LOCTEXT("Solid","Solid"), TEXT("GridCellTypeIcons.Solid") });
	CellTypesOut.Add({ EModelGridDrawCellType::Slab, LOCTEXT("Slab","Slab"), TEXT("GridCellTypeIcons.Slab") });
	CellTypesOut.Add({ EModelGridDrawCellType::Ramp, LOCTEXT("Ramp","Ramp"), TEXT("GridCellTypeIcons.Ramp") });
	CellTypesOut.Add({ EModelGridDrawCellType::Corner, LOCTEXT("Corner","Corner"), TEXT("GridCellTypeIcons.Corner") });
	CellTypesOut.Add({ EModelGridDrawCellType::CutCorner, LOCTEXT("CutCorner","CutCorner"), TEXT("GridCellTypeIcons.CutCorner") });
	CellTypesOut.Add({ EModelGridDrawCellType::Pyramid, LOCTEXT("Pyramid","Pyramid"), TEXT("GridCellTypeIcons.Pyramid") });
	CellTypesOut.Add({ EModelGridDrawCellType::Peak, LOCTEXT("Peak","Peak"), TEXT("GridCellTypeIcons.Peak") });
	CellTypesOut.Add({ EModelGridDrawCellType::Cylinder, LOCTEXT("Cylinder","Cylinder"), TEXT("GridCellTypeIcons.Cylinder") });
}
void UModelGridEditorTool::SetActiveCellType(EModelGridDrawCellType NewType) 
{ 
	if (ModelSettings->CellType != NewType)
	{
		ModelSettings->CellType = NewType;
		OnActiveCellTypeModified.Broadcast(NewType);
		UpdateActiveInteraction();
		UpdateUI();
	}
}



void UModelGridEditorTool::GetKnownDrawModes(TArray<FDrawModeItem>& DrawModesOut)
{
	DrawModesOut.Add({ EModelGridDrawMode::Single, LOCTEXT("Single","Single"), TEXT("GridDrawModeIcons.Single") });
	DrawModesOut.Add({ EModelGridDrawMode::Brush2D, LOCTEXT("Brush2D","Brush2D"), TEXT("GridDrawModeIcons.Brush2D") });
	DrawModesOut.Add({ EModelGridDrawMode::Brush3D, LOCTEXT("Brush3D","Brush3D"), TEXT("GridDrawModeIcons.Brush3D") });
	DrawModesOut.Add({ EModelGridDrawMode::Connected, LOCTEXT("FillLayer","Fill Layer"), TEXT("GridDrawModeIcons.FillLayer") });
	DrawModesOut.Add({ EModelGridDrawMode::Rect2D, LOCTEXT("Rectangle2D","Rectangle"), TEXT("GridDrawModeIcons.Rectangle2D") });
	DrawModesOut.Add({ EModelGridDrawMode::FloodFill2D, LOCTEXT("FloodFill2D","FloodFill 2D"), TEXT("GridDrawModeIcons.FloodFill2D") });
}
void UModelGridEditorTool::SetActiveDrawMode(EModelGridDrawMode NewDrawMode)
{
	if (ModelSettings->DrawMode != NewDrawMode)
	{
		ModelSettings->DrawMode = NewDrawMode;
		OnActiveDrawModeModified.Broadcast(NewDrawMode);
		UpdateActiveInteraction();
		UpdateUI();
	}
}


void UModelGridEditorTool::GetKnownPaintModes(TArray<FPaintModeItem>& PaintModesOut)
{
	PaintModesOut.Add({ EModelGridPaintMode::Single, LOCTEXT("Single","Single"), TEXT("GridPaintModeIcons.Single") });
	PaintModesOut.Add({ EModelGridPaintMode::Brush2D, LOCTEXT("Brush2D","Brush2D"), TEXT("GridPaintModeIcons.Brush2D")});
	PaintModesOut.Add({ EModelGridPaintMode::Brush3D, LOCTEXT("Brush3D","Brush3D"), TEXT("GridPaintModeIcons.Brush3D") });
	PaintModesOut.Add({ EModelGridPaintMode::ConnectedLayer, LOCTEXT("FillLayer","Fill Layer"), TEXT("GridPaintModeIcons.FillLayer") });
	PaintModesOut.Add({ EModelGridPaintMode::Rect2D, LOCTEXT("Rectangle2D","Rectangle"), TEXT("GridPaintModeIcons.Rectangle2D") });
	PaintModesOut.Add({ EModelGridPaintMode::FillVolume, LOCTEXT("FillVolume","Fill Volume"), TEXT("GridPaintModeIcons.FillVolume") });
	PaintModesOut.Add({ EModelGridPaintMode::SingleFace, LOCTEXT("SingleFace","Single Face"), TEXT("GridPaintModeIcons.SingleFace") });
}
void UModelGridEditorTool::SetActivePaintMode(EModelGridPaintMode NewPaintMode)
{
	if (PaintSettings->PaintMode != NewPaintMode)
	{
		PaintSettings->PaintMode = NewPaintMode;
		OnActivePaintModeModified.Broadcast(NewPaintMode);
		UpdateActiveInteraction();
		UpdateUI();
	}
}



void UModelGridEditorTool::SetActiveSelectedMaterialByItem(FGridMaterialListItem Item)
{
	for (int k = 0; k < ColorSettings->AvailableMaterials.Num(); ++k)
	{
		if (ColorSettings->AvailableMaterials[k].InternalMaterialID == Item.InternalMaterialID)
		{
			ColorSettings->SelectedMaterialIndex = k;
			return;
		}
	}
}

void UModelGridEditorTool::SetActiveSelectedMaterialByIndex(int32 ListIndex)
{
	ColorSettings->SelectedMaterialIndex = (ListIndex >= 0 && ListIndex < ColorSettings->AvailableMaterials.Num()) ? ListIndex : 0;
}


void UModelGridEditorTool::UpdateGridFrame(const UE::Geometry::FFrame3d& Frame)
{
	Internal->GridFrame = (GS::Frame3d)Frame;
	PreviewGeometry->SetWorldTransform( ((FFrame3d)Internal->GridFrame).ToFTransform() );
}


void UModelGridEditorTool::UpdateCollider()
{
	if (Internal->PendingColliderUpdateRegion.IsValid())
	{
		AxisBox3i ModifiedCellBounds = Internal->PendingColliderUpdateRegion;
		AxisBox3d UpdateBox = Internal->Builder.GetCellLocalBounds(ModifiedCellBounds.Min);
		UpdateBox.Contain(Internal->Builder.GetCellLocalBounds(ModifiedCellBounds.Max));
		Internal->Collider.UpdateInBounds(Internal->Builder, UpdateBox);
		Internal->PendingColliderUpdateRegion = AxisBox3i::Empty();
	}
}



void UModelGridEditorTool::OnColumnsModified(const TArray<GS::Vector2i>& Columns)
{
	if (Columns.Num() == 0) return;

	PreviewGeometry->ParallelUpdateColumns(Columns, [&](GS::Vector2i Column, FDynamicMesh3& Mesh) {
		FDynamicMesh3Collector MeshAccumulator(&Mesh, true, true);
		Internal->MeshCache.ExtractColumnMesh_Async(Column, MeshAccumulator);
	});
}


static FDateTime LastHideTimeHACK;

void UModelGridEditorTool::ValidatePreviewMesh()
{
	// handle draw preview mesh (preview of placed cell). should not be
	// done here, should not be done every frame, etc etc
	bool bShowDrawPreview =
		(MiscSettings->bShowCellPreview
			&& Settings->EditingMode == EModelGridEditorToolType::Model
			&& ModelSettings->DrawMode == EModelGridDrawMode::Single
			&& ModelSettings->EditType == EModelGridDrawEditType::Add
			&& bCtrlToggle == false
			&& bHovering );

	// very hacky way to try to avoid flickering of preview as mouse is moved around - require
	// some time to pass after hiding it before showing it again
	if (bShowDrawPreview) {
		if ((FDateTime::Now() - LastHideTimeHACK).GetTotalSeconds() < 0.4)
			bShowDrawPreview = false;
	} else
		LastHideTimeHACK = FDateTime::Now();

	DrawPreviewMesh->SetVisible(bShowDrawPreview);
	if (bShowDrawPreview)
	{
		DrawPreviewMesh->SetTransform(((FFrame3d)Internal->GridFrame).ToFTransform());
		UpdateDrawPreviewMesh();
	}


	if (bPreviewMeshDirty)
	{
		TArray<Vector2i> ColumnsToUpdate;
		if (Internal->PendingMeshUpdateRegion.IsValid())
		{
			AxisBox3i ModifiedCellBounds = Internal->PendingMeshUpdateRegion;
			AxisBox3d UpdateBox = Internal->Builder.GetCellLocalBounds(ModifiedCellBounds.Min);
			UpdateBox.Contain(Internal->Builder.GetCellLocalBounds(ModifiedCellBounds.Max));

			Internal->MeshCache.UpdateInBounds(Internal->Builder, UpdateBox, 
				[&](Vector2i Column) { ColumnsToUpdate.Add(Column); });

			Internal->PendingMeshUpdateRegion = AxisBox3i::Empty();
		}

		if (ColumnsToUpdate.Num() > 0)
			OnColumnsModified(ColumnsToUpdate);

		PreviewGeometry->SetWorldTransform( ((FFrame3d)Internal->GridFrame).ToFTransform());

		if (ActiveMaterial)
		{
			bool bDisableGrid = (MiscSettings->SubDLevel > 0);
			ActiveMaterial->SetScalarParameterValue(TEXT("ShowGrid"), bDisableGrid ? 0.0 : 1.0);
		}

		bPreviewMeshDirty = false;
	}
}


void UModelGridEditorTool::ProcessPendingDrawUpdates()
{
	if (DrawUpdateQueue.Num() > 0)
	{
		if (Internal->EditSM.IsInCurrentInteraction())
		{
			if (ToolStateAPI->GetInteractionMode() == IModelGridEditToolStateAPI::EInteractionMode::SingleCell_Continuous)
			{
				for (DrawUpdatePos& CursorPos : DrawUpdateQueue)
					Internal->EditSM.UpdateCellCursor(CursorPos.CellIndex, CursorPos.LocalPosition, CursorPos.LocalNormal);
			}
			else
			{
				// todo skip if all the input parameters are the same...
				DrawUpdatePos CursorPos = DrawUpdateQueue.Last();
				Internal->EditSM.UpdateCellCursor(CursorPos.CellIndex, CursorPos.LocalPosition, CursorPos.LocalNormal);
			}
		}
		DrawUpdateQueue.Reset();
	}

	Internal->AccumulateMeshUpdateRegion();
}


void UModelGridEditorTool::OnTick(float DeltaTime)
{
	static int TickCounter = 0;

	FFrame3d CameraFrame(ViewState.Position, ViewState.Right(), ViewState.Up(), ViewState.Forward());
	FFrame3d LocalCameraFrame = ((FFrame3d)Internal->GridFrame).ToFrame(CameraFrame);
	Internal->EditSM.SetCurrentCameraFrame((GS::Frame3d)LocalCameraFrame);
	Internal->EditSM.SetEnableAutoOrientPlacedBlocksToView( ModelSettings->OrientationMode == EModelGridDrawOrientationType::AlignedToView );

	ProcessPendingDrawUpdates();

	if (Settings != nullptr && TickCounter++ % 2 == 0)
	{
		ValidatePreviewMesh();

		// makes sense to do every other frame?
		DrawPreviewCells.clear();
		if (bHovering && bHaveValidHoverCell)
			UpdateDrawPreviewVisualization();
	}

	PlaneMechanic->PlaneTransformGizmo->SetVisibility(MiscSettings->bShowGizmo);

	// DEBUG
	//FTransformSequence3d CurTransformSeq;
	//Internal->Builder.GetCellOrientationTransform(ActiveEditKey, CurTransformSeq);
	//UnitCellRotationGizmo->UpdateTransform(CurTransformSeq);
}

void UModelGridEditorTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	bool bHaveActiveInteraction = (ActiveInteraction.IsValid());

	ViewState = RenderAPI->GetCameraState();

	FToolDataVisualizer Draw;
	Draw.BeginFrame(RenderAPI);

	FTransform ToLocalTransform = ((FFrame3d)Internal->GridFrame).ToFTransform();
	Draw.PushTransform(ToLocalTransform);

	FVector3d CellDims = Internal->Builder.CellSize();

	double WorkPlaneSquareDim = 3*CellDims.GetAbsMax();
	if (ModelSettings->DrawMode == EModelGridDrawMode::Brush2D || ModelSettings->DrawMode == EModelGridDrawMode::Brush3D)
		WorkPlaneSquareDim = FMath::Max(WorkPlaneSquareDim, BrushSettings->BrushRadius*CellDims.GetAbsMax());

	if (bHovering)
	{
		Draw.DrawPoint(HoverPositionLocal, FLinearColor::Red, 5.0f, false);
		Draw.DrawLine(HoverPositionLocal, HoverPositionLocal + 20.0*(FVector)ActiveHoverCellFaceNormal, FLinearColor::Red, 1.0f, false);

		// draw a rectangle on the drawing plane
		Vector3d CenterPos = Internal->Builder.GetCellLocalBounds(ActiveHoverCellIndex).Center();
		FVector WorkPlaneNormal = (ModelSettings->WorkPlane != EModelGridWorkPlane::Auto) ?
			GetWorkPlaneNormal(ModelSettings->WorkPlane, (FFrame3d)Internal->GridFrame, LastWorldRay.Direction, true) : (FVector)ActiveHoverCellFaceNormal;
		double NormalCellDim = GS::Abs(WorkPlaneNormal.Dot(CellDims));
		FFrame3d PlaneFrame(CenterPos, WorkPlaneNormal);
		FVector X, Y, Z; PlaneFrame.GetAxes(X, Y, Z);
		double Offset = InModifyExistingOperationMode() ? 0.5 : -0.5;
		Draw.DrawSquare(PlaneFrame.Origin + Offset*NormalCellDim*WorkPlaneNormal, WorkPlaneSquareDim*X, WorkPlaneSquareDim*Y,
			FLinearColor::Gray, 1.0f, false);
	}

	// draw grid outer bounds
	AxisBox3i Bounds = Internal->Builder.GetModifiedRegionBounds(4); //   Internal->Builder.CellBounds;
	FVector3d MinCorner = ((FVector3d)Bounds.Min) * CellDims;
	FVector3d MaxCorner = ((FVector3d)(Bounds.Max + Vector3i::One()) ) * CellDims;
	FAxisAlignedBox3d Box(MinCorner, MaxCorner);
	Draw.DrawWireBox((FBox)Box, FLinearColor::White, 1.0f, true);

	// draw grid origin lines
	Draw.DrawLine(FVector(MinCorner.X, 0, 0), FVector(MaxCorner.X, 0, 0), FLinearColor::Blue, 2.0f, true);
	Draw.DrawLine(FVector(0, MinCorner.Y, 0), FVector(0, MaxCorner.Y, 0), FLinearColor::Blue, 2.0f, true);
	FVector3d C = Box.Center(); C.Z = 0;
	Draw.DrawSquare(C, FVector(Box.Width(), 0, 0), FVector(0, Box.Height(), 0), FLinearColor::Yellow, 2.0f, true);

	// draw box for active selected cell
	if (bHaveActiveEditCell)
	{
		// local bounds of the cell
		FAxisAlignedBox3d EditCellBounds = (FAxisAlignedBox3d)Internal->Builder.GetCellLocalBounds(ActiveEditKey);

		// get transform for the cell, without any subcell dimensions, so box we draw is always boundary of cell
		GS::TransformListd CellTransform;
		Internal->Builder.GetCellOrientationTransform(ActiveEditKey, CellTransform, /*bIgnoreSubCellDimensions=*/true);
		// push corners of the box through this transform, so we can show correct local axes
		FVector BoxCorners[8] = {
			FVector(0,0,0), FVector(CellDims.X,0,0), FVector(0,CellDims.Y,0), FVector(0,0,CellDims.Z),
			FVector(CellDims.X, CellDims.Y, 0), FVector(CellDims.X, 0, CellDims.Z), FVector(0, CellDims.Y, CellDims.Z), CellDims
		};
		for (int i = 0; i < 8; ++i)
			BoxCorners[i] = (FVector)CellTransform.TransformPosition((GS::Vector3d)BoxCorners[i]);

		Draw.PushTransform(FTransform(EditCellBounds.Min));

		// x/y/z axes are drawn a bit longer, in r/g/b
		double dx = (CellDims.X + CellDims.Y + CellDims.Z) / 3.0; dx = (dx * 1.1) - dx;
		FVector Extra1 = (FVector)CellTransform.TransformPosition(GS::Vector3d(CellDims.X+dx, 0, 0));
		FVector Extra2 = (FVector)CellTransform.TransformPosition(GS::Vector3d(0, CellDims.Y+dx, 0));
		FVector Extra3 = (FVector)CellTransform.TransformPosition(GS::Vector3d(0, 0, CellDims.Z+dx));
		Draw.DrawLine(BoxCorners[0], Extra1, FLinearColor::Red, 6.0f, true);
		Draw.DrawLine(BoxCorners[0], Extra2, FLinearColor::Green, 6.0f, true);
		Draw.DrawLine(BoxCorners[0], Extra3, FLinearColor::Blue, 6.0f, true);
		// draw rest in yellow and thinner
		Draw.DrawLine(BoxCorners[1], BoxCorners[5], FLinearColor::Yellow, 3.0f, true);
		Draw.DrawLine(BoxCorners[3], BoxCorners[5], FLinearColor::Yellow, 3.0f, true);
		Draw.DrawLine(BoxCorners[3], BoxCorners[6], FLinearColor::Yellow, 3.0f, true);
		Draw.DrawLine(BoxCorners[5], BoxCorners[7], FLinearColor::Yellow, 3.0f, true);
		Draw.DrawLine(BoxCorners[1], BoxCorners[4], FLinearColor::Yellow, 3.0f, true);
		Draw.DrawLine(BoxCorners[4], BoxCorners[7], FLinearColor::Yellow, 3.0f, true);
		Draw.DrawLine(BoxCorners[7], BoxCorners[6], FLinearColor::Yellow, 3.0f, true);
		Draw.DrawLine(BoxCorners[6], BoxCorners[2], FLinearColor::Yellow, 3.0f, true);
		Draw.DrawLine(BoxCorners[2], BoxCorners[4], FLinearColor::Yellow, 3.0f, true);

		Draw.PopTransform();
	}

	// draw preview of potentially-affected cells. this is not very nice but it works for now.
	// should probably be refactored into the Interaction somehow...
	if (bHovering && bHaveValidHoverCell && DrawPreviewCells.size() > 0)
	{
		Vector3i HoverCell = ActiveHoverCellIndex;
		Vector3d DrawPlaneOffset = -Internal->EditSM.GetActiveDrawPlaneNormal() * Internal->Builder.GetCellDimensions() * 0.5;
		if ( InModifyExistingOperationMode() )
			DrawPlaneOffset = -DrawPlaneOffset;
		if (ModelSettings->DrawMode == EModelGridDrawMode::FloodFill2D || ModelSettings->DrawMode == EModelGridDrawMode::Brush2D || ModelSettings->DrawMode == EModelGridDrawMode::Brush3D || ModelSettings->DrawMode == EModelGridDrawMode::Rect2D)
			DrawPlaneOffset = Vector3d::Zero();
		FLinearColor PreviewColor = (ModelSettings->EditType == EModelGridDrawEditType::Erase || bCtrlToggle) ? FLinearColor(0.0f, 0.0f, 0.0f) : FLinearColor(0.95f, 0.1f, 0.0f);
		for (Vector3i CellIndex : DrawPreviewCells) {
			AxisBox3d CellBox = Internal->Builder.GetCellLocalBounds(CellIndex);
			//Draw.DrawWireBox((FBox)CellBox, FLinearColor::Gray, 0.5f, true);
			Draw.DrawPoint(CellBox.Center()+DrawPlaneOffset, PreviewColor, 2.0f, false);
		}
	}

	Draw.PopTransform();

	if (bHaveActiveInteraction)
	{
		ModelGridEditorToolInteraction::EInteractionState CurState = ModelGridEditorToolInteraction::EInteractionState::NoInteraction;
		if (Internal->EditSM.IsInCurrentInteraction())
			CurState = ModelGridEditorToolInteraction::EInteractionState::ClickDragging;
		else if (bHovering)
			CurState = ModelGridEditorToolInteraction::EInteractionState::Hovering;
		ActiveInteraction->Render(Draw, ToLocalTransform, CurState);
	}

	Draw.EndFrame();
}


void UModelGridEditorTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	float DPIScale = Canvas->GetDPIScale();
	UFont* SmallFont = GEngine->GetSmallFont();
	UFont* TinyFont = GEngine->GetTinyFont();
	FViewCameraState CamState = RenderAPI->GetCameraState();
	const FSceneView* SceneView = RenderAPI->GetSceneView();

	if (bHaveValidHoverCell && MiscSettings->ShowCoords != EModelGridCoordinateDisplay::None)
	{
		FAxisAlignedBox3d ActiveCellBounds = (FAxisAlignedBox3d)Internal->Builder.GetCellLocalBounds(ActiveHoverCellIndex);

		FVector2D CursorPixelPos(-9999, 9999);
		for (int k = 0; k < 8; ++k)
		{
			FVector2D CornerPixelPos;
			FVector3d WorldPos = ((FFrame3d)Internal->GridFrame).FromFramePoint( ActiveCellBounds.GetCorner(k) );
			SceneView->WorldToPixel(WorldPos, CornerPixelPos);
			CursorPixelPos.X = FMath::Max(CursorPixelPos.X, CornerPixelPos.X);
			CursorPixelPos.Y = FMath::Min(CursorPixelPos.Y, CornerPixelPos.Y);
		}
		CursorPixelPos += FVector2D(10, -10);

		FString CursorString = (MiscSettings->ShowCoords == EModelGridCoordinateDisplay::ShowXYZ) ?
			FString::Printf(TEXT("%d,%d,%d"), ActiveHoverCellIndex.X, ActiveHoverCellIndex.Y, ActiveHoverCellIndex.Z)
			: FString::Printf(TEXT("%d,%d"), ActiveHoverCellIndex.X, ActiveHoverCellIndex.Y);
		Canvas->DrawShadowedString(CursorPixelPos.X / (double)DPIScale, CursorPixelPos.Y / (double)DPIScale, *CursorString, TinyFont, FLinearColor::White);
	}
}



void UModelGridEditorTool::BeginGridChange()
{
	bool bOK = Internal->EditSM.BeginTrackedChange();
	check(bOK);
}

void UModelGridEditorTool::EndGridChange()
{
	check(Internal->EditSM.IsTrackingChange());
	if (Internal->EditSM.IsTrackingChange())
	{
		TUniquePtr<FModelGrid_GridDeltaChange> DeltaChange = MakeUnique<FModelGrid_GridDeltaChange>();
		DeltaChange->GridChange = std::move(Internal->EditSM.EndTrackedChange());
		if (DeltaChange->GridChange) 
			GetToolManager()->EmitObjectChange(this, MoveTemp(DeltaChange), LOCTEXT("ModelGrid_ModelEdit", "Edit Grid"));
	}

	check(Internal->EditSM.IsTrackingChange() == false);

	Internal->AccumulateMeshUpdateRegion();
}





bool UModelGridEditorTool::GetWantSelectExisting() const
{
	return (Settings->EditingMode == EModelGridEditorToolType::Paint)
		|| bCtrlToggle
		|| bShiftToggle
		|| (Settings->EditingMode == EModelGridEditorToolType::Model && ModelSettings->EditType != EModelGridDrawEditType::Add);
}

bool UModelGridEditorTool::InModifyExistingOperationMode() const
{
	return (Settings->EditingMode == EModelGridEditorToolType::Paint)
		|| (Settings->EditingMode == EModelGridEditorToolType::Model && ModelSettings->EditType != EModelGridDrawEditType::Add)
		|| (Settings->EditingMode == EModelGridEditorToolType::Model && bCtrlToggle == true);
}

void UModelGridEditorTool::UpdateModelingParametersFromSettings(const FRay& WorldRay)
{
	Internal->EditSM.SetCurrentBrushParameters(BrushSettings->BrushRadius, (ModelGridEditMachine::EBrushShape)(int)BrushSettings->BrushShape);

	EModelGridCellType SetType = ToolTypeToGridType(ModelSettings->CellType);
	Internal->EditSM.SetCurrentDrawCellType(SetType);

	if (ModelSettings->WorkPlane != EModelGridWorkPlane::Auto)
	{
		FVector3d LocalWorkPlaneNormal = GetWorkPlaneNormal(ModelSettings->WorkPlane, (FFrame3d)Internal->GridFrame, WorldRay.Direction, true);
		Internal->EditSM.SetActiveDrawPlaneNormal(LocalWorkPlaneNormal);
	}
	else
	{
		Internal->EditSM.SetActiveDrawPlaneNormal((Vector3d)ActiveHoverPlane.Z());
	}

	ModelGridEditMachine::ESculptMode SculptMode = ModelGridEditMachine::ESculptMode::Add;
	if (ModelSettings->EditType == EModelGridDrawEditType::Erase || bCtrlToggle)
		SculptMode = ModelGridEditMachine::ESculptMode::Erase;
	else if (ModelSettings->EditType == EModelGridDrawEditType::Replace)
		SculptMode = ModelGridEditMachine::ESculptMode::Replace;
	Internal->EditSM.SetCurrentSculptMode(SculptMode);

	Internal->EditSM.SetFillLayerSettings(
		(ModelGridEditMachine::ERegionFillMode)(int)FillLayerSettings->FillMode.SelectedIndex,
		(ModelGridEditMachine::ERegionFillOperation)(int)FillLayerSettings->FillOperation.SelectedIndex,
		(ModelGridEditMachine::ERegionFillFilter)FillLayerSettings->Filter.SelectedIndex );

	ModelGridAxisMirrorInfo MirrorX = { (ModelSettings->SymmetryMode.SelectedIndex == 1 || ModelSettings->SymmetryMode.SelectedIndex == 2),
		ModelSettings->SymmetryOrigin.X, (ModelSettings->SymmetryMode.SelectedIndex == 2) };
	ModelGridAxisMirrorInfo MirrorY = { (ModelSettings->SymmetryMode.SelectedIndex == 3 || ModelSettings->SymmetryMode.SelectedIndex == 4),
		ModelSettings->SymmetryOrigin.Y, (ModelSettings->SymmetryMode.SelectedIndex == 4) };
	Internal->EditSM.SetSymmetryState(MirrorX, MirrorY);
}

void UModelGridEditorTool::UpdatePaintParametersFromSettings()
{
	// These colors are in SRGB!!
	Internal->EditSM.SetCurrentPrimaryColor((Color3b)ColorSettings->PaintColor);
	Internal->EditSM.SetCurrentSecondaryColor((Color3b)ColorSettings->EraseColor);
	Internal->EditSM.SetPaintWithSecondaryColor(bCtrlToggle);

	Internal->EditSM.SetCurrentMaterialMode(ModelGridEditMachine::EMaterialMode::ColorRGB);

	int UseSelectedMaterial = FMath::Clamp(ColorSettings->SelectedMaterialIndex, 0, ColorSettings->AvailableMaterials.Num() - 1);
	if (UseSelectedMaterial > 0)
	{
		Internal->EditSM.SetCurrentMaterialMode(ModelGridEditMachine::EMaterialMode::MaterialIndex);
		Internal->EditSM.SetCurrentMaterialIndex(UseSelectedMaterial);
	}

	Internal->ColorModifier_Randomize.HueRange = ColorSettings->HueRange;
	Internal->ColorModifier_Randomize.ValueRange = ColorSettings->ValueRange;
	Internal->ColorModifier_Randomize.SaturationRange = ColorSettings->SaturationRange;
	if (ColorSettings->bRandomize) {
		Internal->EditSM.SetCurrentColorModifier(&Internal->ColorModifier_Randomize);
	}
	else {
		Internal->EditSM.ClearCurrentColorModifier();
	}
}

// ugh why is this still done via a global variable??
ModelGridCell TEMP_ActiveDragEditCell;

void UModelGridEditorTool::OnBeginDrag(const FRay& Ray, const FVector2d& ScreenPos, bool bIsSingleClickInteraction)
{
	if (ActiveInteraction == SelectCellInteraction) {
		ActiveInteraction->BeginClickDrag((GS::Ray3d)Ray);
		return;
	}

	BeginGridChange();

	UpdatePaintParametersFromSettings();
	UpdateModelingParametersFromSettings(Ray);

	if (Settings->EditingMode == EModelGridEditorToolType::Model)
	{
		switch (ModelSettings->DrawMode) {
			case EModelGridDrawMode::Single: Internal->EditSM.BeginSculptCells_Pencil(); break;
			case EModelGridDrawMode::Brush2D: Internal->EditSM.BeginSculptCells_Brush2D(); break;
			case EModelGridDrawMode::Brush3D: Internal->EditSM.BeginSculptCells_Brush3D(); break;
			case EModelGridDrawMode::Connected: Internal->EditSM.BeginSculptCells_FillLayer(!bIsSingleClickInteraction); break;
			case EModelGridDrawMode::FloodFill2D: Internal->EditSM.BeginSculptCells_FloodFillPlanar(); break;
			case EModelGridDrawMode::Rect2D: Internal->EditSM.BeginSculptCells_Rect2D(); break;
		}
	}
	else if (Settings->EditingMode == EModelGridEditorToolType::Paint)
	{
		switch (PaintSettings->PaintMode) {
			case EModelGridPaintMode::Single: Internal->EditSM.BeginPaintCells_Single(); break;
			case EModelGridPaintMode::Brush2D: Internal->EditSM.BeginPaintCells_Brush2D(); break;
			case EModelGridPaintMode::Brush3D: Internal->EditSM.BeginPaintCells_Brush3D(); break;
			case EModelGridPaintMode::ConnectedLayer: Internal->EditSM.BeginPaintCells_FillLayer(); break;
			case EModelGridPaintMode::FillVolume: Internal->EditSM.BeginPaintCells_FillConnected(); break;
			case EModelGridPaintMode::SingleFace: Internal->EditSM.BeginPaintCellFaces_Single(); break;
			case EModelGridPaintMode::Rect2D: Internal->EditSM.BeginPaintCells_Rect2D(); break;
		}
	}

	check(ActiveInteraction.IsValid());
	ActiveInteraction->BeginClickDrag((GS::Ray3d)Ray);
	Internal->AccumulateMeshUpdateRegion();
}



void UModelGridEditorTool::OnUpdateDrag(const FRay& Ray, const FVector2d& ScreenPos)
{
	check(ActiveInteraction.IsValid());
	ActiveInteraction->UpdateClickDrag((GS::Ray3d)Ray);
}


void UModelGridEditorTool::TryAppendPendingDrawCell(GS::Vector3i CellIndex, GS::Vector3d LocalPosition, GS::Vector3d LocalNormal)
{
	bool bPush = (DrawUpdateQueue.Num() == 0 || DrawUpdateQueue.Last().CellIndex != CellIndex);
	bool bIsFaceOp = (Settings->EditingMode == EModelGridEditorToolType::Paint && PaintSettings->PaintMode == EModelGridPaintMode::SingleFace);
	if (bIsFaceOp && DrawUpdateQueue.Num() > 0 && Dot(DrawUpdateQueue.Last().LocalNormal, LocalNormal) < 0.99)
		bPush = true;

	if ( bPush )
		DrawUpdateQueue.Add( DrawUpdatePos{CellIndex,LocalPosition,LocalNormal} );

	NestedCancelCount = 0;
}

void UModelGridEditorTool::OnEndDrag(const FRay& Ray, const FVector2d& ScreenPos)
{
	// if we are doing selection interaction, there is no end-drag and we want to skip any grid-change stuff
	if (ActiveInteraction == SelectCellInteraction)
		return;

	// make sure all stamps/etc have been applied
	ProcessPendingDrawUpdates();

	if (Internal->EditSM.IsInCurrentInteraction())
		Internal->EditSM.EndCurrentInteraction();

	Internal->AccumulateMeshUpdateRegion();		//in case end-interaction did something

	EndGridChange();

	ValidatePreviewMesh();
	PreviewGeometry->BeginCollisionUpdate();
	UpdateCollider();

	// if we were in an active interaction and pressed the ctrl or shift key, it was ignored but now
	// we might want to respect it...
	UpdateActiveInteraction();
}


void UModelGridEditorTool::ConfigureCellRotationGizmo(const GS::ModelGridCell& EditCell)
{
	FFrame3d GizmoFrameWorld = (FFrame3d)Internal->Builder.GetCellFrameWorld(ActiveEditKey, Internal->GridFrame);
	GS::TransformListd CellTransform;
	Internal->Builder.GetCellOrientationTransform(ActiveEditKey, CellTransform);
	FTransformSequence3d CurTransformSeq;
	GS::ConvertToUE(CellTransform, CurTransformSeq);
	UnitCellRotationGizmo->SetCell(GizmoFrameWorld, Internal->Builder.CellSize(), CurTransformSeq);

}

void UModelGridEditorTool::ConfigureCellDimensionGizmos(const GS::ModelGridCell& EditCell)
{
	FFrame3d GizmoFrameWorld = (FFrame3d)Internal->Builder.GetCellFrameWorld(ActiveEditKey, Internal->GridFrame);

	FVector3d UnitCellDimensions = Internal->Builder.GetCellDimensions();
	GS::TransformListd UnitCellTransform;
	Internal->Builder.GetCellOrientationTransform(ActiveEditKey, UnitCellTransform, true);

	bool bIncludeB = (EditCell.CellType == EModelGridCellType::Ramp_Parametric);
	//bool bIncludeB = false;

	ModelGridCellData_StandardRST SubCellParams;
	InitializeSubCellFromGridCell(EditCell, SubCellParams);
	EdgeGizmoA->SetEnabled(true);
	EdgeGizmoB->SetEnabled(bIncludeB);

	//CellDimensions = Internal->Builder.GetTransformedCellDimensions(SubCellParams.Params.AxisDirection, SubCellParams.Params.AxisRotation);

	FVector3d Origin(0, 0, 0);
	FVector3d MaxX(UnitCellDimensions.X, 0, 0);
	FVector3d MaxY(0, UnitCellDimensions.Y, 0);
	FVector3d MaxZ(0, 0, UnitCellDimensions.Z);
	Origin = UnitCellTransform.TransformPosition(Origin);
	MaxX = UnitCellTransform.TransformPosition(MaxX);
	MaxY = UnitCellTransform.TransformPosition(MaxY);
	MaxZ = UnitCellTransform.TransformPosition(MaxZ);

	FVector3d WorldZBottom = GizmoFrameWorld.FromFramePoint(Origin);
	FVector3d WorldZTop = GizmoFrameWorld.FromFramePoint(MaxZ);
	FVector3d WorldZDir = Normalized(WorldZTop - WorldZBottom);
	FFrame3d UseZGizmoWorldFrame(WorldZBottom, WorldZDir);
	FVector3d WorldZLineStart = UseZGizmoWorldFrame.Origin;
	FVector3d WorldZLineEnd = GizmoFrameWorld.FromFramePoint(MaxX);

	double CurParamZ = (double)(SubCellParams.Params.DimensionZ + 1) / (double)(ModelGridCellData_StandardRST::MaxDimension + 1);
	EdgeGizmoA->InitializeParameters(UseZGizmoWorldFrame, 2,
		WorldZLineStart, WorldZLineEnd,
		0, UnitCellDimensions.Z, CurParamZ * UnitCellDimensions.Z);

	if (bIncludeB)
	{
		double CurParamY = (double)(SubCellParams.Params.DimensionY + 1) / (double)(ModelGridCellData_StandardRST::MaxDimension + 1);

		FVector3d WorldYBottom = GizmoFrameWorld.FromFramePoint(Origin);
		FVector3d WorldYTop = GizmoFrameWorld.FromFramePoint(MaxY);
		FVector3d WorldYDir = Normalized(WorldYTop - WorldYBottom);
		FFrame3d UseYGizmoWorldFrame(WorldYBottom, WorldYDir);
		FVector3d WorldYLineStart = UseYGizmoWorldFrame.Origin;
		FVector3d WorldYLineEnd = GizmoFrameWorld.FromFramePoint(MaxX);

		EdgeGizmoB->InitializeParameters(UseYGizmoWorldFrame, 2,
			WorldYLineStart, WorldYLineEnd,
			0, UnitCellDimensions.Y, CurParamY * UnitCellDimensions.Y);
	}
}

void UModelGridEditorTool::ConfigureBoxPartsClickGizmo(const GS::ModelGridCell& EditCell)
{
	FFrame3d GizmoFrameWorld = (FFrame3d)Internal->Builder.GetCellFrameWorld(ActiveEditKey, Internal->GridFrame);
	FVector3d UnitCellDimensions = Internal->Builder.GetCellDimensions();
	BoxPartsClickGizmo->InitializeParameters(GizmoFrameWorld, FAxisAlignedBox3d(FVector3d::Zero(), UnitCellDimensions));
}


void UModelGridEditorTool::UpdateGizmosForActiveEditingCell()
{
	NestedCancelCount = 0;

	UnitCellRotationGizmo->SetEnabled(false);
	EdgeGizmoA->SetEnabled(false);
	EdgeGizmoB->SetEnabled(false);
	BoxPartsClickGizmo->SetEnabled(false);
	SetToolPropertySourceEnabled(CellSettings_StandardRST, false);
	CellSettingsUI_CurrentIndex = FVector3i::MaxVector();

	if (bHaveActiveEditCell == false)
	{
		return;
	}

	bool bIsInGrid = false;
	ModelGridCell EditCell = Internal->Builder.GetCellInfo(ActiveEditKey, bIsInGrid);
	if (!bIsInGrid) return;

	bool bShowCellGizmo = (EditCell.CellType == EModelGridCellType::Slab_Parametric)
							|| (EditCell.CellType == EModelGridCellType::Ramp_Parametric)
							|| (EditCell.CellType == EModelGridCellType::Corner_Parametric)
							|| (EditCell.CellType == EModelGridCellType::CutCorner_Parametric)
							|| (EditCell.CellType == EModelGridCellType::Pyramid_Parametric)
							|| (EditCell.CellType == EModelGridCellType::Peak_Parametric)
							|| (EditCell.CellType == EModelGridCellType::Cylinder_Parametric);

	if (bShowCellGizmo)
	{
		UnitCellRotationGizmo->SetEnabled(true);
		ConfigureCellRotationGizmo(EditCell);
	}

	if (EditCell.CellType == EModelGridCellType::Slab_Parametric || EditCell.CellType == EModelGridCellType::Ramp_Parametric)
	{
		ConfigureCellDimensionGizmos(EditCell);
	}
	else if (EditCell.CellType == EModelGridCellType::Filled)
	{
		BoxPartsClickGizmo->SetEnabled(true);
		ConfigureBoxPartsClickGizmo(EditCell);
	}


	// enable properties...
	if (ModelGridCellData_StandardRST::IsSubType(EditCell.CellType))
	{
		CellSettings_StandardRST->InitializeFromCell(EditCell);
		CellSettingsUI_CurrentIndex = ActiveEditKey;
		SetToolPropertySourceEnabled(CellSettings_StandardRST, true);
	}
}


void UModelGridEditorTool::UpdateLiveGizmosOnExternalChange()
{
	if (!bHaveActiveEditCell) return;

	// This is basically the same code as UpdateGizmosForActiveEditingCell but it doesn't change
	// any gizmo visibility and just re-initializes the internal parameters. Should 
	// combine this stuff because maintaining the duplicate will be messy

	bool bIsInGrid = false;
	ModelGridCell EditCell = Internal->Builder.GetCellInfo(ActiveEditKey, bIsInGrid);
	if (!bIsInGrid) return;

	bool bShowCellGizmo = (EditCell.CellType == EModelGridCellType::Slab_Parametric)
		|| (EditCell.CellType == EModelGridCellType::Ramp_Parametric)
		|| (EditCell.CellType == EModelGridCellType::Corner_Parametric)
		|| (EditCell.CellType == EModelGridCellType::CutCorner_Parametric)
		|| (EditCell.CellType == EModelGridCellType::Pyramid_Parametric)
		|| (EditCell.CellType == EModelGridCellType::Peak_Parametric)
		|| (EditCell.CellType == EModelGridCellType::Cylinder_Parametric);

	if (bShowCellGizmo)
		ConfigureCellRotationGizmo(EditCell);


	if (EditCell.CellType == EModelGridCellType::Slab_Parametric || EditCell.CellType == EModelGridCellType::Ramp_Parametric)
	{
		ConfigureCellDimensionGizmos(EditCell);
	}
	else if (EditCell.CellType == EModelGridCellType::Filled)
	{
		ConfigureBoxPartsClickGizmo(EditCell);
	}


}



void UModelGridStandardRSTCellProperties::InitializeFromCell(const GS::ModelGridCell& Cell)
{
	if (!ModelGridCellData_StandardRST::IsSubType(Cell.CellType)) 
		return;

	ModelGridCellData_StandardRST CellParams = ModelGridCellData_StandardRST();
	CellParams.Params.Fields = Cell.CellData;

	this->Direction_SelectedIndex = CellParams.Params.AxisDirection;
	this->Rotation.SelectedIndex = CellParams.Params.AxisRotation;
	this->DimensionType_SelectedIndex = (int)(EModelGridEditorCellDimensionType)CellParams.Params.DimensionMode;
	this->DimensionX.SelectedIndex = CellParams.Params.DimensionX;
	this->DimensionY.SelectedIndex = CellParams.Params.DimensionY;
	this->DimensionZ.SelectedIndex = CellParams.Params.DimensionZ;
	this->TranslateX.SelectedIndex = CellParams.Params.TranslateX;
	this->TranslateY.SelectedIndex = CellParams.Params.TranslateY;
	this->TranslateZ.SelectedIndex = CellParams.Params.TranslateZ;
	this->bFlipX = (CellParams.Params.FlipX == 1);
	this->bFlipY = (CellParams.Params.FlipY == 1);
	this->bFlipZ = (CellParams.Params.FlipZ == 1);
}
void UModelGridStandardRSTCellProperties::UpdateCell(GS::ModelGridCell& CellToUpdate)
{
	if (!ModelGridCellData_StandardRST::IsSubType(CellToUpdate.CellType))
		return;

	ModelGridCellData_StandardRST CellParams;
	CellParams.Params.Fields = CellToUpdate.CellData;

	CellParams.Params.AxisDirection = (uint8_t)GS::Clamp((unsigned int)this->Direction_SelectedIndex, 0u, ModelGridCellData_StandardRST::MaxRotationAxis);
	CellParams.Params.AxisRotation = (uint8_t)GS::Clamp((unsigned int)this->Rotation.SelectedIndex, 0u, ModelGridCellData_StandardRST::MaxRotationAngle);
	CellParams.Params.DimensionMode = (uint8_t)GS::Clamp((unsigned int)this->DimensionType_SelectedIndex, 0u, ModelGridCellData_StandardRST::MaxDimensionMode);
	CellParams.Params.DimensionX = (uint8_t)GS::Clamp((unsigned int)this->DimensionX.SelectedIndex, 0u, ModelGridCellData_StandardRST::MaxDimension);
	CellParams.Params.DimensionY = (uint8_t)GS::Clamp((unsigned int)this->DimensionY.SelectedIndex, 0u, ModelGridCellData_StandardRST::MaxDimension);
	CellParams.Params.DimensionZ = (uint8_t)GS::Clamp((unsigned int)this->DimensionZ.SelectedIndex, 0u, ModelGridCellData_StandardRST::MaxDimension);
	CellParams.Params.TranslateX = (uint8_t)GS::Clamp((unsigned int)this->TranslateX.SelectedIndex, 0u, ModelGridCellData_StandardRST::MaxTranslate);
	CellParams.Params.TranslateY = (uint8_t)GS::Clamp((unsigned int)this->TranslateY.SelectedIndex, 0u, ModelGridCellData_StandardRST::MaxTranslate);
	CellParams.Params.TranslateZ = (uint8_t)GS::Clamp((unsigned int)this->TranslateZ.SelectedIndex, 0u, ModelGridCellData_StandardRST::MaxTranslate);
	CellParams.Params.FlipX = this->bFlipX ? 1 : 0;
	CellParams.Params.FlipY = this->bFlipY ? 1 : 0;
	CellParams.Params.FlipZ = this->bFlipZ ? 1 : 0;

	CellToUpdate.CellData = CellParams.Params.Fields;
}

void UModelGridEditorTool::OnCellSettingsUIChanged()
{
	if (CellSettingsUI_CurrentIndex != ActiveEditKey) return;
	if (CellSettings_StandardRST->IsPropertySetEnabled())
	{
		bool bIsInGrid = false;
		ModelGridCell InitialCell = Internal->Builder.GetCellInfo(ActiveEditKey, bIsInGrid);
		if (bIsInGrid)
		{
			ModelGridCell EditCell = InitialCell;
			CellSettings_StandardRST->UpdateCell(EditCell);
			if ( EditCell != InitialCell)
			{
				BeginGridChange();
				Internal->EditSM.ApplySingleCellUpdate(ActiveEditKey, EditCell);
				Internal->AccumulateMeshUpdateRegion();
				EndGridChange();

				// update any active gizmos with new parameters
				UpdateLiveGizmosOnExternalChange();
			}
		}
	}
}


void UModelGridEditorTool::OnEdgeGizmoBegin(UEdgeOnAxisGizmo* Gizmo, double CurrentValue)
{
	BeginGridChange();
}

void UModelGridEditorTool::OnEdgeGizmoUpdate(UEdgeOnAxisGizmo* Gizmo, double CurrentValue)
{
	bool bIsInGrid = false;
	ModelGridCell EditCell = Internal->Builder.GetCellInfo(ActiveEditKey, bIsInGrid);
	if (!bIsInGrid) return;

	bool bCellModified = false;
	bool bIsRSTCell = false;
	if (EditCell.CellType == EModelGridCellType::Slab_Parametric || EditCell.CellType == EModelGridCellType::Ramp_Parametric)
	{
		bool bIncludeB = (EditCell.CellType == EModelGridCellType::Ramp_Parametric);

		ModelGridCellData_StandardRST SubCellParams;
		InitializeSubCellFromGridCell(EditCell, SubCellParams);
		FVector3d CellDimensions = Internal->Builder.GetTransformedCellDimensions(SubCellParams.Params.AxisDirection, SubCellParams.Params.AxisRotation);

		if (Gizmo == EdgeGizmoA)
		{
			double NewT = FMath::Clamp(CurrentValue / CellDimensions.Z, 0.0, 1.0);
			double RemappedT = FMathd::Lerp(1.0, 16.0, NewT);
			int QuantizedT = FMath::Clamp((int)RemappedT, 0, ModelGridCellData_StandardRST::MaxDimension + 1);
			SubCellParams.Params.DimensionZ = QuantizedT - 1;
		}
		else if (Gizmo == EdgeGizmoB)
		{
			double NewT = FMath::Clamp(CurrentValue / CellDimensions.Y, 0.0, 1.0);
			double RemappedT = FMathd::Lerp(1.0, 16.0, NewT);
			int QuantizedT = FMath::Clamp((int)RemappedT, 0, ModelGridCellData_StandardRST::MaxDimension + 1);
			SubCellParams.Params.DimensionY = QuantizedT - 1;
		}

		UpdateGridCellParamsFromSubCell(EditCell, SubCellParams);
		bCellModified = true;
	}


	if (bCellModified)
	{
		Internal->EditSM.ApplySingleCellUpdate(ActiveEditKey, EditCell);

		// read back new cell and update properties UI
		ModelGridCell NewCell = Internal->Builder.GetCellInfo(ActiveEditKey, bIsInGrid);
		CellSettings_StandardRST->InitializeFromCell(NewCell);
		CellSettings_StandardRST->SilentUpdateWatched();
	}
}

void UModelGridEditorTool::OnEdgeGizmoEnd(UEdgeOnAxisGizmo* Gizmo, double CurrentValue)
{
	EndGridChange();
}


void UModelGridEditorTool::OnBoxPartsClick(UBoxRegionClickGizmo* Gizmo, int RegionTypeInt, FVector3d RegionNormals[3])
{
	bool bIsInGrid = false;
	ModelGridCell EditCell = Internal->Builder.GetCellInfo(ActiveEditKey, bIsInGrid);
	if (!bIsInGrid) return;

	bool bCellModified = false;

	UBoxRegionClickGizmo::EBoxRegionType RegionType = static_cast<UBoxRegionClickGizmo::EBoxRegionType>(RegionTypeInt);
	if (RegionType == UBoxRegionClickGizmo::EBoxRegionType::Face)
	{
		BeginGridChange();
		MGCell_Slab NewSubCellParams = MGCell_Slab::GetDefaultCellParams();
		uint8_t AxisDirection = 0;
		MGCell_Slab::DetermineOrientationFromAxis(RegionNormals[0], AxisDirection);
		NewSubCellParams.Params.AxisDirection = AxisDirection;
		NewSubCellParams.Params.DimensionZ = 7;
		UpdateGridCellFromSubCell(EditCell, NewSubCellParams);
		bCellModified = true;
	}
	else if (RegionType == UBoxRegionClickGizmo::EBoxRegionType::Edge)
	{
		BeginGridChange();
		MGCell_Ramp NewSubCellParams = MGCell_Ramp::GetDefaultCellParams();
		uint8_t AxisDirection = 0, AxisRotation = 0;
		MGCell_Ramp::DetermineOrientationFromAxes(RegionNormals[1], RegionNormals[0], AxisDirection, AxisRotation);
		NewSubCellParams.Params.AxisDirection = AxisDirection;
		NewSubCellParams.Params.AxisRotation = AxisRotation;
		UpdateGridCellFromSubCell(EditCell, NewSubCellParams);
		bCellModified = true;
	}
	else if (RegionType == UBoxRegionClickGizmo::EBoxRegionType::Corner)
	{
		BeginGridChange();
		MGCell_Corner NewSubCellParams = MGCell_Corner::GetDefaultCellParams();
		uint8_t AxisDirection = 0, AxisRotation = 0;
		FVector3d CornerDir = Normalized(RegionNormals[0] + RegionNormals[1] + RegionNormals[2]);
		MGCell_Corner::DetermineOrientationFromDiagonal(CornerDir, AxisDirection, AxisRotation);
		NewSubCellParams.Params.AxisDirection = AxisDirection;
		NewSubCellParams.Params.AxisRotation = AxisRotation;
		UpdateGridCellFromSubCell(EditCell, NewSubCellParams);
		bCellModified = true;
	}

	if (bCellModified)
	{
		Internal->EditSM.ApplySingleCellUpdate(ActiveEditKey, EditCell);
		Internal->AccumulateMeshUpdateRegion();
	}

	EndGridChange();	// will discard change if no cell was modified
	UpdateGizmosForActiveEditingCell();
}



void UModelGridEditorTool::OnRotationAxisClicked(int Axis, int Steps)
{
	BeginGridChange();

	bool bIsInGrid = false;
	ModelGridCell ActiveCellInfo = Internal->Builder.GetCellInfo(ActiveEditKey, bIsInGrid);
	bool bCellModified = GS::ApplyRotationToCell(ActiveCellInfo, Axis, Steps);
	if (bCellModified)
	{
		Internal->EditSM.ApplySingleCellUpdate(ActiveEditKey, ActiveCellInfo);
		Internal->AccumulateMeshUpdateRegion();

		GS::TransformListd CellTransform;
		Internal->Builder.GetCellOrientationTransform(ActiveEditKey, CellTransform);
		FTransformSequence3d CurTransformSeq;
		GS::ConvertToUE(CellTransform, CurTransformSeq);
		UnitCellRotationGizmo->UpdateTransform(CurTransformSeq);
	}

	EndGridChange();

	UpdateGizmosForActiveEditingCell();
}


void UModelGridEditorTool::OnRotationHotkey(bool bReverse)
{
	if (bHaveActiveEditCell)
	{
		OnRotationAxisClicked(2, (bReverse) ? -1 : 1);
	}
	else
	{
		EModelGridCellType ActiveType = ToolTypeToGridType(ModelSettings->CellType);
		ModelGridCell GridCell = Internal->EditSM.GetCurrentDrawCellPreview(ActiveType, ActiveHoverPlane.Z());
		bool bCellModified = GS::ApplyRotationToCell(GridCell, 2, (bReverse) ? -1 : 1);
		if (bCellModified) {
			Internal->EditSM.UpdateDrawCellDefaultsForType(GridCell);
		}
	}
}


void UModelGridEditorTool::ForceUpdateOnUndoRedo()
{
	Internal->AccumulateMeshUpdateRegion();

	bPreviewMeshDirty = true;
	ValidatePreviewMesh();
	PreviewGeometry->BeginCollisionUpdate();
	UpdateCollider();
	GridTimestamp++;

	UpdateGizmosForActiveEditingCell();
}




void UModelGridEditorTool::OnUpdateModifierState(int ModifierID, bool bIsOn)
{
	if (ModifierID == UModelGridEditorTool_ShiftModifier)
	{
		bShiftToggle = bIsOn;
		UpdateActiveInteraction();
	}
	else if (ModifierID == UModelGridEditorTool_CtrlModifier)
	{
		bCtrlToggle = bIsOn;
		UpdateActiveInteraction();
	}
}


FInputRayHit UModelGridEditorTool::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	ensure(ActiveInteraction.IsValid());
	
	bool bCanBegin = ActiveInteraction->CanBeginClickDrag((GS::Ray3d)ClickPos.WorldRay);
	return (bCanBegin) ? FInputRayHit(0) : FInputRayHit();
}

void UModelGridEditorTool::OnClicked(const FInputDeviceRay& ClickPos)
{
	LastWorldRay = ClickPos.WorldRay;
	bInSimulatingClickViaDrag = true;
	OnBeginDrag(ClickPos.WorldRay, ClickPos.ScreenPosition, true);
	OnUpdateDrag(ClickPos.WorldRay, ClickPos.ScreenPosition);
	OnEndDrag(ClickPos.WorldRay, ClickPos.ScreenPosition);
	bInSimulatingClickViaDrag = false;
}


FInputRayHit UModelGridEditorTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	if (ActiveInteraction.IsValid()) {
		bool bCanBegin = ActiveInteraction->CanBeginClickDrag((GS::Ray3d)PressPos.WorldRay);
		return (bCanBegin) ? FInputRayHit(0) : FInputRayHit();
	}

	// should be unhittable now...
	check(false);
	return FInputRayHit();
}

void UModelGridEditorTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	LastWorldRay = PressPos.WorldRay;
	OnBeginDrag(PressPos.WorldRay, PressPos.ScreenPosition, false);
}

void UModelGridEditorTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	LastWorldRay = DragPos.WorldRay;
	OnUpdateDrag(DragPos.WorldRay, DragPos.ScreenPosition);
}

void UModelGridEditorTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	LastWorldRay = ReleasePos.WorldRay;
	OnEndDrag(ReleasePos.WorldRay, ReleasePos.ScreenPosition);
}

void UModelGridEditorTool::OnTerminateDragSequence()
{
	// TODO: save both intead of just world ray
	OnEndDrag(LastWorldRay, FVector2d::Zero());
}




FInputRayHit UModelGridEditorTool::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	LastWorldRay = PressPos.WorldRay;

	if (ActiveInteraction.IsValid()) 
	{
		double HoverDist = GS::Ray3d::SafeMaxDist();
		if (ActiveInteraction->HoverUpdate((GS::Ray3d)PressPos.WorldRay, HoverDist))
			return FInputRayHit(HoverDist);
		return FInputRayHit();
	}

	check(false);
	return FInputRayHit();
}

void UModelGridEditorTool::OnBeginHover(const FInputDeviceRay& DevicePos)
{
	bHovering = true;
}

void UModelGridEditorTool::OnEndHover()
{
	bHovering = false;
}

bool UModelGridEditorTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	if (ActiveInteraction.IsValid()) {
		double HoverDist = GS::Ray3d::SafeMaxDist();
		ActiveInteraction->HoverUpdate( (GS::Ray3d)DevicePos.WorldRay, HoverDist);
		return true;
	}

	// should be a dead path now
	check(false);
	return true;
}


FInputRayHit UModelGridEditorTool::ShouldRespondToMouseWheel(const FInputDeviceRay& CurrentPos) {
	return FInputRayHit();
}
void UModelGridEditorTool::OnMouseWheelScrollUp(const FInputDeviceRay& CurrentPos) { }
void UModelGridEditorTool::OnMouseWheelScrollDown(const FInputDeviceRay& CurrentPos) { }



EModelGridDrawCellType UModelGridEditorTool::GridTypeToToolType(EModelGridCellType CellType, bool& bValid)
{
	bValid = true;
	switch (CellType)
	{
	case EModelGridCellType::Filled: return EModelGridDrawCellType::Solid;
	case EModelGridCellType::Slab_Parametric: return EModelGridDrawCellType::Slab;
	case EModelGridCellType::Ramp_Parametric: return EModelGridDrawCellType::Ramp;
	case EModelGridCellType::Corner_Parametric: return EModelGridDrawCellType::Corner;
	case EModelGridCellType::CutCorner_Parametric: return EModelGridDrawCellType::CutCorner;
	case EModelGridCellType::Pyramid_Parametric: return EModelGridDrawCellType::Pyramid;
	case EModelGridCellType::Peak_Parametric: return EModelGridDrawCellType::Peak;
	case EModelGridCellType::Cylinder_Parametric: return EModelGridDrawCellType::Cylinder;
	default: bValid = false; return EModelGridDrawCellType::Solid;
	}
}
EModelGridCellType UModelGridEditorTool::ToolTypeToGridType(EModelGridDrawCellType CellType)
{
	switch (CellType)
	{
	case EModelGridDrawCellType::Solid: return EModelGridCellType::Filled;
	case EModelGridDrawCellType::Slab: return EModelGridCellType::Slab_Parametric;
	case EModelGridDrawCellType::Ramp: return EModelGridCellType::Ramp_Parametric;
	case EModelGridDrawCellType::Corner: return EModelGridCellType::Corner_Parametric;
	case EModelGridDrawCellType::CutCorner: return EModelGridCellType::CutCorner_Parametric;
	case EModelGridDrawCellType::Pyramid: return EModelGridCellType::Pyramid_Parametric;
	case EModelGridDrawCellType::Peak: return EModelGridCellType::Peak_Parametric;
	case EModelGridDrawCellType::Cylinder: return EModelGridCellType::Cylinder_Parametric;
	default: return EModelGridCellType::Filled;
	}
}


void UModelGridEditorTool::PickAtCursor(bool bErasePick)
{
	if (!bHovering) return;

	Internal->EditSM.UpdateCellCursor(ActiveHoverCellIndex);
	if (Internal->EditSM.PickDrawCellFromCursorLocation())
	{
		bool bFound = false;
		EModelGridDrawCellType KnownType = GridTypeToToolType(Internal->EditSM.GetCurrentDrawCellType(), bFound);
		if (bFound)
		{
			SetActiveCellType(KnownType);
			ModelSettings->SilentUpdateWatched();
		}
		FColor SetColor = (FColor)Internal->EditSM.GetCurrentPrimaryColor();
		if (bErasePick)
			ColorSettings->EraseColor = SetColor;
		else
			ColorSettings->PaintColor = SetColor;
		ColorSettings->SilentUpdateWatched();
	}
}

void UModelGridEditorTool::CyclePlacementOrientation()
{
	ModelSettings->OrientationMode = (ModelSettings->OrientationMode == EModelGridDrawOrientationType::Fixed) ?
		EModelGridDrawOrientationType::AlignedToView : EModelGridDrawOrientationType::Fixed;
	ModelSettings->SilentUpdateWatched();
}


void UModelGridEditorTool::CycleDrawPlane()
{
	ModelSettings->WorkPlane = (EModelGridWorkPlane)(((int)ModelSettings->WorkPlane + 1) % 4);
	ModelSettings->SilentUpdateWatched();
}

void UModelGridEditorTool::PickDrawPlaneAtCursor()
{
	if (!bHovering) return;
	int AxisIndex = MaxAbsElementIndex((FVector3d)ActiveHoverCellFaceNormal);
	EModelGridWorkPlane NewWorkPlane = (EModelGridWorkPlane)(AxisIndex + 1);
	if (NewWorkPlane != ModelSettings->WorkPlane)
	{
		ModelSettings->WorkPlane = NewWorkPlane;
		ModelSettings->SilentUpdateWatched();
	}
}

void UModelGridEditorTool::SetSymmetryOriginAtCursor()
{
	if (!bHovering) return;
	ModelSettings->SymmetryOrigin = FIntVector2(ActiveHoverCellIndex.X, ActiveHoverCellIndex.Y);
}

void UModelGridEditorTool::SetTargetWorld(UWorld* World)
{
	TargetWorld = World;
}

UWorld* UModelGridEditorTool::GetTargetWorld()
{
	return TargetWorld.Get();
}

void UModelGridEditorTool::SetExistingActor(AGSModelGridActor* Actor)
{
	ExistingActor = Actor;
}




bool UModelGridEditorTool::SupportsWorldSpaceFocusBox()
{
	return true;
}


FBox UModelGridEditorTool::GetWorldSpaceFocusBox()
{
	FVector3d MinCorner, MaxCorner;
	if (bHaveActiveEditCell || bHaveValidHoverCell)
	{
		Vector3i UseKey = (bHaveActiveEditCell) ? ActiveEditKey : ActiveHoverCellIndex;
		AxisBox3d CellBounds = Internal->Builder.GetCellLocalBounds(UseKey);
		CellBounds.Expand(2.0 * Internal->Builder.CellSize());
		MinCorner = CellBounds.Min; MaxCorner = CellBounds.Max;
	}
	else
	{
		AxisBox3i RegionBounds = Internal->Builder.GetModifiedRegionBounds(4);
		MinCorner = ((FVector3d)RegionBounds.Min) * Internal->Builder.CellSize();
		MaxCorner = ((FVector3d)(RegionBounds.Max + Vector3i::One())) * Internal->Builder.CellSize();
	}
	MinCorner = Internal->GridFrame.ToWorldPoint(MinCorner);
	MaxCorner = Internal->GridFrame.ToWorldPoint(MaxCorner);
	FAxisAlignedBox3d Box(MinCorner, MaxCorner);
	return (FBox)Box;
}


bool UModelGridEditorTool::SupportsWorldSpaceFocusPoint()
{
	return true;

}
bool UModelGridEditorTool::GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut)
{
	ModelGridEditHitInfo HitInfo;
	if (ToolStateAPI->FindModelGridSurfaceRayIntersection((Ray3d)WorldRay, HitInfo))
	{
		PointOut = HitInfo.WorldHitLocation;
		return true;
	}
	else if (ToolStateAPI->FindOccupiedCellRayIntersection((Ray3d)WorldRay, HitInfo))
	{
		PointOut = HitInfo.WorldHitLocation;
		return true;
	}
	return false;
}



void UModelGridEditorTool::UpdateDrawPreviewMesh()
{
	IMeshBuilder* TempMeshBuilder = Internal->MeshBuilderFactory.Allocate();
	ModelGridMesher& GridMesher = Internal->MeshCache.MeshBuilder;

	ModelGridMesher::AppendCache Cache;
	GridMesher.InitAppendCache(Cache);

	EModelGridCellType CellType = ToolTypeToGridType(ModelSettings->CellType);
	ModelGridCell CellInfo = Internal->EditSM.GetCurrentDrawCellPreview(CellType, ActiveHoverPlane.Z());

	ModelGridMesher::CellMaterials UseMaterials;
	UseMaterials.CellType = CellInfo.MaterialType;
	UseMaterials.CellMaterial = CellInfo.CellMaterial;

	TransformListd TransformSeq;
	GS::GetUnitCellTransform(CellInfo, Internal->Builder.CellSize(), TransformSeq);

	Vector3i CellIndex = ActiveHoverCellIndex;
	AxisBox3d LocalBounds = Internal->Builder.GetCellLocalBounds(CellIndex);

	if (CellInfo.CellType == EModelGridCellType::Filled ||  CellInfo.CellType == EModelGridCellType::Slab_Parametric)
		GridMesher.AppendBox(LocalBounds, UseMaterials, *TempMeshBuilder, TransformSeq, Cache);
	else if (CellInfo.CellType == EModelGridCellType::Ramp_Parametric)
		GridMesher.AppendRamp(LocalBounds, UseMaterials, *TempMeshBuilder, TransformSeq, Cache);
	else if (CellInfo.CellType == EModelGridCellType::Corner_Parametric)
		GridMesher.AppendCorner(LocalBounds, UseMaterials, *TempMeshBuilder, TransformSeq, Cache);
	else if (CellInfo.CellType == EModelGridCellType::CutCorner_Parametric)
		GridMesher.AppendCutCorner(LocalBounds, UseMaterials, *TempMeshBuilder, TransformSeq, Cache);
	else if (CellInfo.CellType == EModelGridCellType::Pyramid_Parametric)
		GridMesher.AppendPyramid(LocalBounds, UseMaterials, *TempMeshBuilder, TransformSeq, Cache);
	else if (CellInfo.CellType == EModelGridCellType::Peak_Parametric)
		GridMesher.AppendPeak(LocalBounds, UseMaterials, *TempMeshBuilder, TransformSeq, Cache);
	else if (CellInfo.CellType == EModelGridCellType::Cylinder_Parametric)
		GridMesher.AppendCylinder(LocalBounds, UseMaterials, *TempMeshBuilder, TransformSeq, Cache);


	FDynamicMesh3Builder* DynamicMeshBuilder = (FDynamicMesh3Builder*)TempMeshBuilder;
	DynamicMeshBuilder->bDeleteMeshOnDestruct = true;
	DrawPreviewMesh->ReplaceMesh(std::move(*DynamicMeshBuilder->Mesh));

	delete TempMeshBuilder;
	TempMeshBuilder = nullptr;
}

// 
void UModelGridEditorTool::UpdateDrawPreviewVisualization()
{
	DrawPreviewCells.clear();
	//if (Settings->EditingMode == EModelGridEditorToolType::Paint)
	//	return;

	// todo: do not need to update this every frame...only when settings
	// or hovered cell have changed

	// update drawing parameters in EditSM. maybe this should just happen every hover, not here?
	UpdateModelingParametersFromSettings(LastWorldRay);

	std::vector<Vector3i> PreviewEditCells;
	
	ModelGridEditMachine::EditState PreviewState = ModelGridEditMachine::EditState::SculptCells_Pencil;
	if (Settings->EditingMode == EModelGridEditorToolType::Model)
	{
		if (ModelSettings->DrawMode == EModelGridDrawMode::Brush2D)
			PreviewState = ModelGridEditMachine::EditState::SculptCells_Brush2D;
		else if (ModelSettings->DrawMode == EModelGridDrawMode::Brush3D)
			PreviewState = ModelGridEditMachine::EditState::SculptCells_Brush3D;
		else if (ModelSettings->DrawMode == EModelGridDrawMode::Connected)
			PreviewState = ModelGridEditMachine::EditState::SculptCells_FillLayer;
		else if (ModelSettings->DrawMode == EModelGridDrawMode::FloodFill2D)
			PreviewState = ModelGridEditMachine::EditState::SculptCells_FloodFillPlanar;
		else if (ModelSettings->DrawMode == EModelGridDrawMode::Rect2D)
			PreviewState = ModelGridEditMachine::EditState::SculptCells_Rectangle2D_Parametric;
	}
	else {
		if (PaintSettings->PaintMode == EModelGridPaintMode::Brush2D)
			PreviewState = ModelGridEditMachine::EditState::PaintCells_Brush2D;
		else if (PaintSettings->PaintMode == EModelGridPaintMode::Brush3D)
			PreviewState = ModelGridEditMachine::EditState::PaintCells_Brush3D;
		else if (PaintSettings->PaintMode == EModelGridPaintMode::ConnectedLayer)
			PreviewState = ModelGridEditMachine::EditState::PaintCells_FillLayer;
		else if (PaintSettings->PaintMode == EModelGridPaintMode::FillVolume)
			PreviewState = ModelGridEditMachine::EditState::PaintCells_FillConnected;
		else if (PaintSettings->PaintMode == EModelGridPaintMode::Rect2D)
			PreviewState = ModelGridEditMachine::EditState::PaintCells_Rectangle2D_Parametric;
	}

	PreviewEditCells.clear();
	Internal->EditSM.GetPreviewOfCellEdit(PreviewState,
		ActiveHoverCellIndex, ActiveHoverPlane.Origin, ActiveHoverPlane.Z(),
			[&](const ModelGridCellEditSet::EditCell& cell) { PreviewEditCells.push_back(cell.CellIndex); } );

	// filter draw preview cells, for cases where we might have too many cells.
	// this is no good...
	if (ModelSettings->DrawMode == EModelGridDrawMode::Brush3D)
	{
		for (Vector3i CellIndex : PreviewEditCells) {
			Vector3i delta = (CellIndex - ActiveHoverCellIndex);
			if (((Vector3d)delta).Length() > 0.9 * BrushSettings->BrushRadius)
				DrawPreviewCells.push_back(CellIndex);
		}
	}
	else if (ModelSettings->DrawMode == EModelGridDrawMode::FloodFill2D)
	{
		for (Vector3i CellIndex : PreviewEditCells) {
			Vector3i delta = (CellIndex - ActiveHoverCellIndex).Abs();
			if (GS::Max3(delta.X, delta.Y, delta.Z) < 25)
				DrawPreviewCells.push_back(CellIndex);
		}
	}
	else
		DrawPreviewCells = std::move(PreviewEditCells);

}


void UModelGridEditorTool::UpdateActiveInteraction()
{
	// cannot change interaction during an edit
	if (Internal->EditSM.IsInCurrentInteraction())
		return;

	if (ActiveInteraction.IsValid()) {
		ActiveInteraction.Reset();
	}

	if (Settings->EditingMode == EModelGridEditorToolType::Paint)
	{
		ActiveInteraction = PaintCellsInteraction;
		//if (PaintSettings->PaintMode == EModelGridPaintMode::SingleFace)
			// need custom interaction for this? somehow need face info...
	}
	else if (Settings->EditingMode == EModelGridEditorToolType::Model)
	{
		if (bShiftToggle)
			ActiveInteraction = SelectCellInteraction;
		else if (ModelSettings->EditType == EModelGridDrawEditType::Erase || bCtrlToggle)
			ActiveInteraction = EraseCellsInteraction;
		else if (ModelSettings->EditType == EModelGridDrawEditType::Replace)
			ActiveInteraction = ReplaceCellsInteraction;
		else
			ActiveInteraction = AppendCellsInteraction;
	}

	if (ActiveInteraction.IsValid()) 
	{
		// ?
	}
}



bool UModelGridEditorTool::CanCurrentlyNestedCancel()
{
	if (bHaveActiveEditCell)
		return true;
	else
		return (NestedCancelCount < 3);
}

bool UModelGridEditorTool::ExecuteNestedCancelCommand()
{
	if (bHaveActiveEditCell) 
		ToolStateAPI->ClearActiveGridSelection();

	if (NestedCancelCount == 0)
		GetToolManager()->DisplayMessage(LOCTEXT("CancelMsg", "To avoid accidental Cancellation of your edit session, this tool requires you to hit Escape 3 times in a row to Exit."), EToolMessageLevel::UserMessage);

	NestedCancelCount++;
	return NestedCancelCount < 3;
}




class ToolStateAPIImplementation : public IModelGridEditToolStateAPI
{
public:
	UModelGridEditorTool* Tool;
	static constexpr double HitNudge = 0.001;		// amount to nudge hit points to be "inside" cells, should be
													// smaller than smallest cell extent
									

	ToolStateAPIImplementation(UModelGridEditorTool* ParentTool) { Tool = ParentTool; }

	virtual GS::Frame3d GetGridFrame() override {
		return Tool->Internal->GridFrame;
	}

	virtual void AccessModelGrid(TFunctionRef<void(const GS::ModelGrid&)> ProcessFunc) override {
		ProcessFunc(Tool->Internal->Builder);
	}

	virtual GS::AxisBox3d GetLocalCellBounds(Vector3i CellIndex) override {
		return Tool->Internal->Builder.GetCellLocalBounds(CellIndex);
	}

	// wrapper around PreviewGeometry->FindRayIntersection in tool
	virtual bool FindModelGridSurfaceRayIntersection(const GS::Ray3d& WorldRay, ModelGridEditHitInfo& HitInfoOut) override
	{
		FFrame3d GridFrame = (FFrame3d)Tool->Internal->GridFrame;
		FRay3d LocalRay = (FRay3d)GridFrame.ToFrame((FRay3d)WorldRay);		// argh add this to Frame3d

		FHitResult MeshHitResult;
		if (Tool->PreviewGeometry->FindRayIntersection((FRay3d)WorldRay, MeshHitResult))
		{
			Vector3d WorldHitPoint = MeshHitResult.Location;
			Vector3d LocalHitPoint = Tool->Internal->GridFrame.ToLocalPoint(WorldHitPoint);
			bool bIsInGrid = false;
			Vector3i HitCellKey = Tool->Internal->Builder.GetCellAtPosition(LocalHitPoint + HitNudge * LocalRay.Direction, bIsInGrid);
			if (bIsInGrid) {
				HitInfoOut.LocalHitLocation = LocalHitPoint;
				HitInfoOut.WorldHitLocation = GridFrame.FromFramePoint(LocalHitPoint);
				HitInfoOut.WorldHitNormal = MeshHitResult.Normal;
				HitInfoOut.LocalHitNormal = Normalized(GridFrame.ToFrameVector(HitInfoOut.WorldHitNormal));
				HitInfoOut.HitCellIndex = HitCellKey;
				HitInfoOut.LocalHitCellFaceNormal = Tool->Internal->GetHitFaceNormalForCell(LocalRay, false, HitCellKey);
				HitInfoOut.WorldHitCellFaceNormal = Normalized(GridFrame.FromFrameVector(HitInfoOut.LocalHitCellFaceNormal));
				HitInfoOut.HitType = ModelGridEditHitInfo::EHitType::GridSurface;
				return true;
			}
		}
		return false;
	}

	// wrapper around Collider.FindNearestHitCell  (no collider access needed otherwise...)
	virtual bool FindOccupiedCellRayIntersection(const GS::Ray3d& WorldRay, ModelGridEditHitInfo& HitInfoOut) override
	{
		FFrame3d GridFrame = (FFrame3d)Tool->Internal->GridFrame;
		FRay3d LocalRay = (FRay3d)GridFrame.ToFrame((FRay3d)WorldRay);		// argh add this to Frame3d

		double RayHitT; Vector3i HitCellKey; Vector3d HitFaceNormal;
		if (Tool->Internal->Collider.FindNearestHitCell((Ray3d)LocalRay, RayHitT, HitFaceNormal, HitCellKey))
		{
			FVector LocalHitLocation = LocalRay.PointAt(RayHitT);
			HitInfoOut.LocalHitLocation = LocalHitLocation;
			HitInfoOut.WorldHitLocation = GridFrame.FromFramePoint(LocalHitLocation);
			HitInfoOut.LocalHitNormal = HitFaceNormal;
			HitInfoOut.WorldHitNormal = Normalized(GridFrame.FromFrameVector(HitInfoOut.LocalHitNormal));
			HitInfoOut.HitCellIndex = HitCellKey;
			HitInfoOut.LocalHitCellFaceNormal = HitFaceNormal;
			HitInfoOut.WorldHitCellFaceNormal = Normalized(GridFrame.FromFrameVector(HitInfoOut.LocalHitCellFaceNormal));
			HitInfoOut.HitType = ModelGridEditHitInfo::EHitType::GridCell;
			return true;
		}
		return false;
	}

	virtual bool FindSceneRayIntersectionCell(const GS::Ray3d& WorldRay, FHitResult& WorldHitResultOut, ModelGridEditHitInfo& HitInfoOut) override
	{
		FFrame3d GridFrame = (FFrame3d)Tool->Internal->GridFrame;
		FRay3d LocalRay = (FRay3d)GridFrame.ToFrame((FRay3d)WorldRay);		// argh add this to Frame3d

		const TArray<const UPrimitiveComponent*>& IgnoreComponents = Tool->PreviewGeometry->GetVisibleComponents();
		bool bWorldHit = ToolSceneQueriesUtil::FindNearestVisibleObjectHit(Tool, WorldHitResultOut, (FRay)WorldRay, &IgnoreComponents);
		if (bWorldHit)
		{
			HitInfoOut.WorldHitLocation = WorldHitResultOut.Location + HitNudge*WorldRay.Direction;
			HitInfoOut.WorldHitNormal = WorldHitResultOut.Normal;
			HitInfoOut.LocalHitLocation = Tool->Internal->GridFrame.ToLocalPoint(HitInfoOut.WorldHitLocation);
			HitInfoOut.LocalHitNormal = Tool->Internal->GridFrame.ToLocalVector(HitInfoOut.WorldHitNormal);

			bool bIsInGrid = false;
			Vector3i HitCellKey = Tool->Internal->Builder.GetCellAtPosition(HitInfoOut.LocalHitLocation, bIsInGrid);
			if (bIsInGrid) {
				HitInfoOut.HitCellIndex = HitCellKey;
				HitInfoOut.LocalHitCellFaceNormal = Tool->Internal->GetHitFaceNormalForCell(LocalRay, false, HitCellKey);
				HitInfoOut.WorldHitCellFaceNormal = Tool->Internal->GridFrame.ToWorldVector(HitInfoOut.LocalHitCellFaceNormal);
				HitInfoOut.HitType = ModelGridEditHitInfo::EHitType::SceneSurfaces;
				return true;
			}
		}
		return false;
	}

	virtual bool FindGroundRayIntersectionCell(const GS::Ray3d& WorldRay, FHitResult& WorldHitResultOut, ModelGridEditHitInfo& HitInfoOut)
	{
		FFrame3d GridFrame = (FFrame3d)Tool->Internal->GridFrame;
		FRay3d LocalRay = (FRay3d)GridFrame.ToFrame((FRay3d)WorldRay);		// argh add this to Frame3d

		FVector HitPoint;
		if (GridFrame.RayPlaneIntersection(WorldRay.Origin, WorldRay.Direction, 2, HitPoint))
		{
			WorldHitResultOut.Location = WorldHitResultOut.ImpactPoint = HitPoint;
			WorldHitResultOut.Normal = WorldHitResultOut.ImpactNormal = GridFrame.Z();

			HitInfoOut.WorldHitLocation = WorldHitResultOut.Location + HitNudge*GridFrame.Z();
			HitInfoOut.WorldHitNormal = WorldHitResultOut.Normal;
			HitInfoOut.LocalHitLocation = Tool->Internal->GridFrame.ToLocalPoint(HitInfoOut.WorldHitLocation);
			HitInfoOut.LocalHitNormal = Tool->Internal->GridFrame.ToLocalVector(HitInfoOut.WorldHitNormal);

			bool bIsInGrid = false;
			Vector3i HitCellKey = Tool->Internal->Builder.GetCellAtPosition(HitInfoOut.LocalHitLocation, bIsInGrid);
			if (bIsInGrid) {
				HitInfoOut.HitCellIndex = HitCellKey;
				HitInfoOut.LocalHitCellFaceNormal = Tool->Internal->GetHitFaceNormalForCell(LocalRay, false, HitCellKey);
				HitInfoOut.WorldHitCellFaceNormal = Tool->Internal->GridFrame.ToWorldVector(HitInfoOut.LocalHitCellFaceNormal);
				HitInfoOut.HitType = ModelGridEditHitInfo::EHitType::GroundPlane;
				return true;
			}
		}
		return false;
	}

	virtual GS::Vector3i GetCellAtPosition(const GS::Vector3d& Position, bool bIsWorldPosition, bool& bIsInGrid) override
	{
		Vector3d LocalPosition = (bIsWorldPosition) ? Tool->Internal->GridFrame.ToLocalPoint(Position) : Position;
		return Tool->Internal->Builder.GetCellAtPosition(LocalPosition, bIsInGrid);
	}

	virtual void QueryCellAtIndex(const GS::Vector3i& CellIndex, bool& bIsInGrid, bool& bIsEmpty) override
	{
		bIsInGrid = Tool->Internal->Builder.IsValidCell(CellIndex);
		bIsEmpty = bIsInGrid && Tool->Internal->Builder.IsCellEmpty(CellIndex);
	}

	virtual bool CanApplyActiveDrawActionToCell(const GS::Vector3i& CellIndex) override
	{
		if (Tool->ModelSettings->DrawMode == EModelGridDrawMode::Brush2D || Tool->ModelSettings->DrawMode == EModelGridDrawMode::Brush3D)
			return true;
		bool bIsEmpty = Tool->Internal->Builder.IsCellEmpty(CellIndex);
		if (Tool->ModelSettings->EditType == EModelGridDrawEditType::Erase || Tool->bCtrlToggle)
			return !bIsEmpty;
		else
			return bIsEmpty;
	}

	virtual IModelGridEditToolStateAPI::EInteractionMode GetInteractionMode() const override
	{
		if (Tool->Settings->EditingMode == EModelGridEditorToolType::Model)
		{
			switch (Tool->ModelSettings->DrawMode) {
				case EModelGridDrawMode::Connected:
					if ( Tool->bInSimulatingClickViaDrag == false)
						return IModelGridEditToolStateAPI::EInteractionMode::StartEndCells_NormalAxis;
					break;
				case EModelGridDrawMode::Rect2D:
					return IModelGridEditToolStateAPI::EInteractionMode::StartEndCells_InPlane;
			}
			return IModelGridEditToolStateAPI::EInteractionMode::SingleCell_Continuous;
		}
		else
		{
			switch (Tool->PaintSettings->PaintMode) {
				case EModelGridPaintMode::Rect2D:
					return IModelGridEditToolStateAPI::EInteractionMode::StartEndCells_InPlane;
				default:
					return IModelGridEditToolStateAPI::EInteractionMode::SingleCell_Continuous;
			}
		}
	}

	virtual int GetFixedWorkPlaneMode() const override
	{
		return (int)Tool->ModelSettings->WorkPlane - 1;
	}

	virtual DrawModifiers GetDrawModifiers() const override
	{
		return DrawModifiers{ Tool->ModelSettings->bHitWorkPlane, Tool->ModelSettings->bHitWorld };
	}

	virtual void InitializeGridEditCursorLocation(GS::Vector3i NewLocation, GS::Vector3d LocalPosition, GS::Vector3d LocalNormal) override
	{
		// should be done at tool level - have to clear selection before applying any edits...
		ClearActiveGridSelection();

		Tool->Internal->EditSM.SetInitialCellCursor(NewLocation, LocalPosition, LocalNormal);
	}

	virtual void UpdateGridEditCursorLocation(GS::Vector3i NewLocation, GS::Vector3d LocalPosition, GS::Vector3d LocalNormal) override
	{
		// should be done at tool level - have to clear selection before applying any edits...
		ClearActiveGridSelection();

		Tool->TryAppendPendingDrawCell(NewLocation, LocalPosition, LocalNormal);
	}

	virtual void UpdateGridEditPreviewLocation(bool bHaveValidPreview, GS::Vector3i NewLocation, GS::Frame3d LocationDrawPlaneLocal) override
	{
		Tool->bHaveValidHoverCell = bHaveValidPreview;
		Tool->ActiveHoverCellIndex = NewLocation;
		Tool->ActiveHoverPlane = (FFrame3d)LocationDrawPlaneLocal;
		Tool->HoverPositionLocal = Tool->ActiveHoverPlane.Origin;
		Tool->ActiveHoverCellFaceNormal = (Vector3i)GS::ClosestUnitVector((Vector3d)Tool->ActiveHoverPlane.Z());
	}

	virtual void TrySetActiveGridSelection(GS::Vector3i SelectedCell) override
	{
		bool bHadActiveEditCell = Tool->bHaveActiveEditCell;
		Tool->bHaveActiveEditCell = false;
		check(Tool->Settings->EditingMode == EModelGridEditorToolType::Model);

		bool bIsInGrid = false;
		TEMP_ActiveDragEditCell = Tool->Internal->Builder.GetCellInfo(SelectedCell, bIsInGrid);
		if (bIsInGrid && TEMP_ActiveDragEditCell.IsEmpty() == false)
		{
			if (!(bHadActiveEditCell && Tool->ActiveEditKey == (FVector3i)SelectedCell))
			{
				Tool->bHaveActiveEditCell = true;
				Tool->ActiveEditKey = SelectedCell;
			}
		}

		if (Tool->bHaveActiveEditCell || bHadActiveEditCell)
			Tool->UpdateGizmosForActiveEditingCell();
	}

	virtual void ClearActiveGridSelection() override
	{
		if (Tool->bHaveActiveEditCell)
		{
			Tool->bHaveActiveEditCell = false;
			Tool->UpdateGizmosForActiveEditingCell();
		}
	}
};

void UModelGridEditorTool::InitializeInteractions()
{
	ToolStateAPI = MakeShared<ToolStateAPIImplementation>(this);
	
	PaintCellsInteraction = MakeShared<ModelGridPaintCellsInteraction>();
	PaintCellsInteraction->SetCurrentState(ToolStateAPI);

	EraseCellsInteraction = MakeShared<ModelGridEraseCellsInteraction>();
	EraseCellsInteraction->SetCurrentState(ToolStateAPI);
	
	ReplaceCellsInteraction = MakeShared<ModelGridReplaceCellsInteraction>();
	ReplaceCellsInteraction->SetCurrentState(ToolStateAPI);

	AppendCellsInteraction = MakeShared<ModelGridAppendCellsInteraction>();
	AppendCellsInteraction->SetCurrentState(ToolStateAPI);

	SelectCellInteraction = MakeShared<ModelGridSelectCellInteraction>();
	SelectCellInteraction->SetCurrentState(ToolStateAPI);
}




void FModelGrid_GridDeltaChange::Apply(UObject* Object)
{
	if (UModelGridEditorTool* Tool = Cast<UModelGridEditorTool>(Object))
	{
		Tool->Internal->EditSM.ReapplyChange(*GridChange, false);
		Tool->ForceUpdateOnUndoRedo();
	}
}
void FModelGrid_GridDeltaChange::Revert(UObject* Object)
{
	if (UModelGridEditorTool* Tool = Cast<UModelGridEditorTool>(Object))
	{
		Tool->Internal->EditSM.ReapplyChange(*GridChange, true);
		Tool->ForceUpdateOnUndoRedo();
	}
}




#undef LOCTEXT_NAMESPACE
