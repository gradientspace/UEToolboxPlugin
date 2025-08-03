// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/ReshapeModelGridTool.h"

#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"
#include "ToolSetupUtil.h"
#include "Selection/ToolSelectionUtil.h"
#include "ModelingObjectsCreationAPI.h"
#include "EditorModelingObjectsCreationAPI.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "PreviewMesh.h"

#include "GridActor/ModelGridActor.h"
#include "GridActor/UGSModelGrid.h"
#include "GridActor/UGSModelGridAsset.h"
#include "GridActor/UGSModelGridAssetUtils.h"
#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridMeshCache.h"
#include "Grid/GSGridUtil.h"
#include "Utility/GridToolUtil.h"
#include "Utility/GSUEModelGridUtil.h"

#include "Mechanics/ConstructionPlaneMechanic.h"
#include "BaseGizmos/CombinedTransformGizmo.h"

#include "Engine/World.h"
#include "Materials/Material.h"
#include "Editor/EditorEngine.h"

using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UReshapeModelGridTool"

bool UReshapeModelGridToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return (SceneState.SelectedActors.Num() == 1) &&
		(Cast<AGSModelGridActor>(SceneState.SelectedActors[0]) != nullptr);
}

UInteractiveTool* UReshapeModelGridToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UReshapeModelGridTool* Tool = NewObject<UReshapeModelGridTool>(SceneState.ToolManager);
	Tool->SetTargetWorld(SceneState.World);

	if ( AGSModelGridActor* Actor = Cast<AGSModelGridActor>(SceneState.SelectedActors[0]) )
	{
		Tool->SetExistingActor(Actor);
	}

	return Tool;
}



struct FReshapeModelGridToolInternalData
{
	UGSModelGrid* SourceUGrid;
	ModelGrid InitialGrid;
	AxisBox3i InitialGridBounds;

	AxisBox3i CurrentPostCropGridBounds;

	ModelGrid CurrentGrid;
};


UReshapeModelGridTool::UReshapeModelGridTool()
{
	UInteractiveTool::SetToolDisplayName(LOCTEXT("ToolName", "Reshape ModelGrid"));
}

void UReshapeModelGridTool::SetTargetWorld(UWorld* World)
{
	TargetWorld = World;
}

UWorld* UReshapeModelGridTool::GetTargetWorld()
{
	return TargetWorld.Get();
}

void UReshapeModelGridTool::SetExistingActor(AGSModelGridActor* Actor)
{
	ExistingActor = Actor;
}



void UReshapeModelGridTool::Setup()
{
	InternalData = MakePimpl<FReshapeModelGridToolInternalData>();

	Settings = NewObject<UReshapeModelGridSettings>(this);
	//Settings->RestoreProperties(this);		// do not save/restore properties here...they are specific to grid
	AddToolPropertySource(Settings);

	AGSModelGridActor* GridActor = ExistingActor.Get();
	GridActor->SetIsTemporarilyHiddenInEditor(true);

	InternalData->SourceUGrid = GridActor->GetGrid();
	InternalData->SourceUGrid->ProcessGrid([&](const ModelGrid& Grid) { InternalData->InitialGrid = Grid; });
	InternalData->InitialGridBounds = InternalData->InitialGrid.GetOccupiedRegionBounds(0);
	InternalData->CurrentGrid = InternalData->InitialGrid;

	FVector3d Dimensions = InternalData->InitialGrid.CellSize();
	Settings->bNonUniform = (Dimensions.X != Dimensions.Y || Dimensions.Y != Dimensions.Z);
	Settings->DimensionX = Dimensions.X;
	Settings->DimensionY = Dimensions.Y;
	Settings->DimensionZ = Dimensions.Z;
	Settings->Dimension = (Settings->bNonUniform) ? (Dimensions.X+Dimensions.Y+Dimensions.Z)/3.0 : Dimensions.X;
	
	Settings->WatchProperty(Settings->Dimension, [&](double) { OnGridCellDimensionsModified(); });
	Settings->WatchProperty(Settings->bNonUniform, [&](bool) { OnGridCellDimensionsModified(); });
	Settings->WatchProperty(Settings->DimensionX, [&](double) { OnGridCellDimensionsModified(); });
	Settings->WatchProperty(Settings->DimensionY, [&](double) { OnGridCellDimensionsModified(); });
	Settings->WatchProperty(Settings->DimensionZ, [&](double) { OnGridCellDimensionsModified(); });
	//Settings->WatchProperty(Settings->Origin, [&](EReshapeModelGridOrigin) { UpdatePreviewGridShape(); });

	CropButtonsTarget = MakeShared<FGSActionButtonTarget>();
	CropButtonsTarget->ExecuteCommand = [this](FString CommandName) { OnCropButtonClicked(CommandName); };
	Settings->CropButtons.Target = CropButtonsTarget;
	Settings->CropButtons.AddAction(TEXT("ResetCrop"), LOCTEXT("ResetCrop", "Reset Cropping"), LOCTEXT("ResetCropTooltip", "Reset the crop rect"));

	Settings->WatchProperty(Settings->Translation, [&](FIntVector) { UpdatePreviewGridShape(); });
	Settings->WatchProperty(Settings->bFlipX, [&](bool) { UpdatePreviewGridShape(); });
	Settings->WatchProperty(Settings->bFlipY, [&](bool) { UpdatePreviewGridShape(); });
	Settings->WatchProperty(Settings->bFlipZ, [&](bool) { UpdatePreviewGridShape(); });
	Settings->WatchProperty(Settings->CropRangeX, [&](FIntVector2) { UpdatePreviewGridShape(); });
	Settings->WatchProperty(Settings->CropRangeY, [&](FIntVector2) { UpdatePreviewGridShape(); });
	Settings->WatchProperty(Settings->CropRangeZ, [&](FIntVector2) { UpdatePreviewGridShape(); });

	PivotButtonsTarget = MakeShared<FGSActionButtonTarget>();
	PivotButtonsTarget->ExecuteCommand = [this](FString CommandName) { OnPivotButtonClicked(CommandName); };
	Settings->PivotButtons.Target = PivotButtonsTarget;
	Settings->PivotButtons.AddAction(TEXT("PivotCenter"), LOCTEXT("PivotCenter", "Center"), LOCTEXT("PivotCenterTooltip", "Recenter the Grid to the Centermost Cell"));
	Settings->PivotButtons.AddAction(TEXT("PivotBase"), LOCTEXT("PivotBase", "Base"), LOCTEXT("PivotBaseTooltip", "Recenter the Grid to the Base-Center Cell"));

	PreviewMesh = NewObject<UPreviewMesh>(this);
	PreviewMesh->CreateInWorld(GetTargetWorld(), FTransform::Identity);
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(PreviewMesh, nullptr);
	PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::NoTangents);

	if (UMaterial* GridMaterial = LoadObject<UMaterial>(nullptr, TEXT("/GradientspaceUEToolbox/Materials/M_GridEditMaterial")))
	{
		ActiveMaterial = UMaterialInstanceDynamic::Create(GridMaterial, this);
	}
	ActiveMaterials.Add(ActiveMaterial);
	PreviewMesh->SetMaterials(ActiveMaterials);

	ModelGrid NewGrid;
	NewGrid.Initialize(GetCellDimensions());

	UpdatePreviewGridShape();

	PreviewMesh->SetVisible(true);

	FTransform TargetTransform = GridActor->GetTransform();
	GridFrame = (FFrame3d)TargetTransform;
	PreviewMesh->SetTransform(TargetTransform);

	PlaneMechanic = NewObject<UConstructionPlaneMechanic>(this);
	PlaneMechanic->Setup(this);
	PlaneMechanic->CanUpdatePlaneFunc = [this]() { return true; };
	PlaneMechanic->OnPlaneChanged.AddLambda([this]() { UpdateGridPivotFrame(PlaneMechanic->Plane); });
	PlaneMechanic->Initialize(GetTargetWorld(), GridFrame);
	//PlaneMechanic->PlaneTransformGizmo->bUseContextCoordinateSystem = false;
	//PlaneMechanic->PlaneTransformGizmo->CurrentCoordinateSystem = EToolContextCoordinateSystem::Local;
	PlaneMechanic->PlaneTransformGizmo->SetIsNonUniformScaleAllowedFunction([]() { return false; });
	PlaneMechanic->PlaneTransformGizmo->SetVisibility(false);
}

bool UReshapeModelGridTool::CanAccept() const
{
	return true;
}

void UReshapeModelGridTool::OnTick(float DeltaTime)
{
}


void UReshapeModelGridTool::UpdateGridPivotFrame(const UE::Geometry::FFrame3d& Frame)
{
	//GridFrame = Frame;
	//PreviewMesh->SetTransform( GridFrame.ToFTransform() );
}



FVector3d UReshapeModelGridTool::GetCellDimensions() const
{
	if (Settings->bNonUniform)
		return FVector3d(Settings->DimensionX, Settings->DimensionY, Settings->DimensionZ);
	else
		return FVector3d(Settings->Dimension);
}



void UReshapeModelGridTool::UpdatePreviewGridShape()
{
	Vector3i Translation = (Vector3i)Settings->Translation;
	bool bFlipX = Settings->bFlipX, bFlipY = Settings->bFlipY, bFlipZ = Settings->bFlipZ;

	ModelGrid NewGrid;
	NewGrid.Initialize(GetCellDimensions());

	AxisBox3i CropBox = InternalData->InitialGridBounds;
	CropBox.Min.X += Settings->CropRangeX.X;	CropBox.Max.X -= Settings->CropRangeX.Y;
	CropBox.Min.Y += Settings->CropRangeY.X;	CropBox.Max.Y -= Settings->CropRangeY.Y;
	CropBox.Min.Z += Settings->CropRangeZ.X;	CropBox.Max.Z -= Settings->CropRangeZ.Y;

	InternalData->CurrentPostCropGridBounds = CropBox;

	InternalData->InitialGrid.EnumerateFilledCells(
		[&](Vector3i CellIndex, const ModelGridCell& CellInfo, AxisBox3d LocalBounds)
	{
		if (CropBox.Contains(CellIndex) == false) return;

		CellIndex += Translation;
		if (bFlipX)
			CellIndex.X = -CellIndex.X;
		if (bFlipY)
			CellIndex.Y = -CellIndex.Y;
		if (bFlipZ)
			CellIndex.Z = -CellIndex.Z;

		NewGrid.ReinitializeCell(CellIndex, CellInfo);
	});

	InternalData->CurrentGrid = MoveTemp(NewGrid);

	AxisBox3i ModifiedRegion = InternalData->CurrentGrid.GetModifiedRegionBounds(0);
	//Settings->GridExtents = FString::Printf(TEXT("%d x %d x %d"),
	//	ModifiedRegion.CountX(), ModifiedRegion.CountY(), ModifiedRegion.CountZ());
	Settings->GridExtents = FIntVector(ModifiedRegion.CountX(), ModifiedRegion.CountY(), ModifiedRegion.CountZ());

	if (ModifiedRegion.IsValid())
	{
		Settings->GridMin = ModifiedRegion.Min;
		Settings->GridMax = ModifiedRegion.Max;
	}
	else
	{
		Settings->GridMin = Settings->GridMax = FIntVector(0, 0, 0);
	}

	OnGridCellDimensionsModified();
}


void UReshapeModelGridTool::OnGridCellDimensionsModified()
{
	ModelGrid& UpdateGrid = InternalData->CurrentGrid;
	UpdateGrid.SetNewCellDimensions(GetCellDimensions());

	// update mesh...
	FDynamicMesh3 FinalMesh;
	GS::ExtractGridFullMesh(UpdateGrid, FinalMesh, /*materials=*/true, /*uvs=*/true);

	PreviewMesh->ReplaceMesh(MoveTemp(FinalMesh));
	ActiveMaterial->SetVectorParameterValue(TEXT("CellDimensions"), UpdateGrid.GetCellDimensions());
}

void UReshapeModelGridTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	GS::GridToolUtil::DrawGridOriginCell(RenderAPI, GridFrame, InternalData->CurrentGrid);
}



void UReshapeModelGridTool::OnCropButtonClicked(FString CommandName)
{
	if (CommandName == TEXT("ResetCrop")) {
		Settings->CropRangeX = Settings->CropRangeY = Settings->CropRangeZ = FIntVector2(0, 0);
		NotifyOfPropertyChangeByTool(Settings);
	} else
		UE_LOG(LogTemp, Warning, TEXT("[UReshapeModelGridTool::OnCropButtonClicked] Unknown Command [%s]"), *CommandName);
}

void UReshapeModelGridTool::OnPivotButtonClicked(FString CommandName)
{
	if (CommandName == TEXT("PivotCenter") || CommandName == TEXT("PivotBase"))
	{
		AxisBox3i CellBounds = InternalData->CurrentPostCropGridBounds;
		Vector3i Center(
			(CellBounds.Min.X + CellBounds.Max.X) / 2,
			(CellBounds.Min.Y + CellBounds.Max.Y) / 2,
			(CellBounds.Min.Z + CellBounds.Max.Z) / 2);
		if (CommandName == TEXT("PivotBase"))
			Center.Z = CellBounds.Min.Z;
		Settings->Translation = -Center;
		// = InternalData->CurrentGrid.GetModifiedRegionBounds()
		NotifyOfPropertyChangeByTool(Settings);
	}
	else
		UE_LOG(LogTemp, Warning, TEXT("[UReshapeModelGridTool::OnPivotButtonClicked] Unknown Command [%s]"), *CommandName);
}



void UReshapeModelGridTool::Shutdown(EToolShutdownType ShutdownType)
{
	//Settings->SaveProperties(this);

	PlaneMechanic->Shutdown();
	PlaneMechanic = nullptr;

	PreviewMesh->Disconnect();
	PreviewMesh = nullptr;

	AGSModelGridActor* UpdateActor = ExistingActor.Get();
	if (UpdateActor)
	{
		UpdateActor->SetIsTemporarilyHiddenInEditor(false);
	}

	if (ShutdownType == EToolShutdownType::Accept)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("ShutdownEdit", "Reshape Grid"));

		// make sure transactions are enabled on target grid object
		UGSModelGrid* GridWrapper = UpdateActor->GetGrid();
		GridWrapper->SetEnableTransactions(true);

		// do we need to mark asset dirty?? make it transactional??

		UpdateActor->Modify();
		GridWrapper->Modify();

		GridWrapper->EditGrid([&](ModelGrid& EditGrid)
		{
			EditGrid = InternalData->CurrentGrid;
		});

		//UpdateActor->SetActorTransform(((FFrame3d)Internal->GridFrame).ToFTransform());

		GridWrapper->PostEditChange();
		UpdateActor->PostEditChange();

		ToolSelectionUtil::SetNewActorSelection(GetToolManager(), UpdateActor);

		GetToolManager()->EndUndoTransaction();
	}

}






bool UReshapeModelGridTool::SupportsWorldSpaceFocusBox()
{
	return true;
}


FBox UReshapeModelGridTool::GetWorldSpaceFocusBox()
{
	FAxisAlignedBox3d LocalBounds = PreviewMesh->GetMesh()->GetBounds();
	if (LocalBounds.IsEmpty())
		LocalBounds = FAxisAlignedBox3d(FVector::Zero(), FVector::Zero());
	FAxisAlignedBox3d WorldBox = FAxisAlignedBox3d::Empty();
	for (int j = 0; j < 8; ++j)
		WorldBox.Contain(GridFrame.FromFramePoint(LocalBounds.GetCorner(j)));
	return (FBox)WorldBox;
}


bool UReshapeModelGridTool::SupportsWorldSpaceFocusPoint()
{
	return false;

}
bool UReshapeModelGridTool::GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut)
{
	return false;
}






#undef LOCTEXT_NAMESPACE
