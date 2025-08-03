// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/CreateModelGridTool.h"

#include "InteractiveToolManager.h"
#include "ToolSetupUtil.h"
#include "Selection/ToolSelectionUtil.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "PreviewMesh.h"
#include "SceneQueries/SceneSnappingManager.h"

#include "GridActor/ModelGridActor.h"
#include "GridActor/UGSModelGrid.h"
#include "GridActor/UGSModelGridAsset.h"
#include "GridActor/UGSModelGridAssetUtils.h"
#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridMeshCache.h"
#include "Grid/GSGridUtil.h"
#include "Utility/GridToolUtil.h"
#include "Utility/GSUEModelGridUtil.h"

#include "Utility/PlacementUtils.h"

#include "Mechanics/ConstructionPlaneMechanic.h"
#include "Mechanics/DragAlignmentMechanic.h"
#include "BaseGizmos/CombinedTransformGizmo.h"

#include "Engine/World.h"
#include "Materials/Material.h"


using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UCreateModelGridTool"

bool UCreateModelGridToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return true;
}

UInteractiveTool* UCreateModelGridToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UCreateModelGridTool* Tool = CreateNewToolOfType(SceneState);
	Tool->SetTargetWorld(SceneState.World);

	if (SceneState.SelectedActors.Num() == 1) {
		if ( AGSModelGridActor* Actor = Cast<AGSModelGridActor>(SceneState.SelectedActors[0]) )
			Tool->SetExistingActor(Actor);
	}

	return Tool;
}

UCreateModelGridTool* UCreateModelGridToolBuilder::CreateNewToolOfType(const FToolBuilderState& SceneState) const
{
	return NewObject<UCreateModelGridTool>(SceneState.ToolManager);
}



struct FCreateModelGridToolInternalData
{
	ModelGrid SourceGrid;
};



UCreateModelGridTool::UCreateModelGridTool()
{
	UInteractiveTool::SetToolDisplayName(LOCTEXT("ToolName", "Create ModelGrid"));
}

void UCreateModelGridTool::SetTargetWorld(UWorld* World)
{
	TargetWorld = World;
}

UWorld* UCreateModelGridTool::GetTargetWorld()
{
	return TargetWorld.Get();
}

void UCreateModelGridTool::SetExistingActor(AGSModelGridActor* Actor)
{
	ExistingActor = Actor;
}



void UCreateModelGridTool::Setup()
{
	InternalData = MakePimpl<FCreateModelGridToolInternalData>();
	GridFrame = FFrame3d();

	Settings = NewObject<UCreateModelGridSettings>(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);

	bool bInitializedFromExisting = false;
	if (ExistingActor != nullptr) {
		if (UGSModelGrid* ExistingGrid = ExistingActor->GetGrid()) {
			ExistingGrid->ProcessGrid([&](const ModelGrid& OtherGrid) {
				Vector3d Dims = OtherGrid.GetCellDimensions();
				Settings->DimensionX = Dims.X; Settings->DimensionY = Dims.Y; Settings->DimensionZ = Dims.Z;
				Settings->bNonUniform = !( GS::ToleranceEqual(Dims.X,Dims.Y) && GS::ToleranceEqual(Dims.Y,Dims.Z) );
				Settings->Dimension = Settings->bNonUniform ? (Dims.X+Dims.Y+Dims.Z)/3.0 : Dims.X;
				Settings->GridType = ECreateModelGridInitialGrid::NoCells;
				GridFrame = FFrame3d(ExistingActor->GetTransform());
				bInitializedFromExisting = true;
			});
		}
	}

	Settings->WatchProperty(Settings->Dimension, [&](double) { OnGridCellDimensionsModified(); });
	Settings->WatchProperty(Settings->bNonUniform, [&](bool) { OnGridCellDimensionsModified(); });
	Settings->WatchProperty(Settings->DimensionX, [&](double) { OnGridCellDimensionsModified(); });
	Settings->WatchProperty(Settings->DimensionY, [&](double) { OnGridCellDimensionsModified(); });
	Settings->WatchProperty(Settings->DimensionZ, [&](double) { OnGridCellDimensionsModified(); });
	Settings->WatchProperty(Settings->Origin, [&](ECreateModelGridOrigin) { UpdatePreviewGridShape(); });

	Settings->WatchProperty(Settings->GridType, [&](ECreateModelGridInitialGrid) { UpdatePreviewGridShape(); });
	Settings->WatchProperty(Settings->NumCellsX, [&](int) { UpdatePreviewGridShape(); });
	Settings->WatchProperty(Settings->NumCellsY, [&](int) { UpdatePreviewGridShape(); });
	Settings->WatchProperty(Settings->NumCellsZ, [&](int) { UpdatePreviewGridShape(); });


	PreviewMesh = NewObject<UPreviewMesh>(this);
	PreviewMesh->CreateInWorld(GetTargetWorld(), GridFrame.ToFTransform());
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(PreviewMesh, nullptr);
	PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::NoTangents);

	if (UMaterial* GridMaterial = LoadObject<UMaterial>(nullptr, TEXT("/GradientspaceUEToolbox/Materials/M_GridEditMaterial")))
	{
		ActiveMaterial = UMaterialInstanceDynamic::Create(GridMaterial, this);
	}
	ActiveMaterials.Add(ActiveMaterial);
	PreviewMesh->SetMaterials(ActiveMaterials);

	UpdatePreviewGridShape();

	PreviewMesh->SetVisible(true);

	PlaneMechanic = NewObject<UConstructionPlaneMechanic>(this);
	PlaneMechanic->Setup(this);
	PlaneMechanic->CanUpdatePlaneFunc = [this]() { return true; };
	PlaneMechanic->OnPlaneChanged.AddLambda([this]() { UpdateGridFrameFromGizmo(PlaneMechanic->Plane); });
	PlaneMechanic->Initialize(GetTargetWorld(), GridFrame);
	//PlaneMechanic->PlaneTransformGizmo->bUseContextCoordinateSystem = false;
	//PlaneMechanic->PlaneTransformGizmo->CurrentCoordinateSystem = EToolContextCoordinateSystem::Local;
	PlaneMechanic->PlaneTransformGizmo->SetIsNonUniformScaleAllowedFunction([]() { return false; });
	PlaneMechanic->PlaneTransformGizmo->SetVisibility(true);

	DragAlignmentMechanic = NewObject<UDragAlignmentMechanic>(this);
	DragAlignmentMechanic->Setup(this);
	DragAlignmentMechanic->AddToGizmo(PlaneMechanic->PlaneTransformGizmo);

	if (!bInitializedFromExisting)
		bAutoPlacementPending = true;
}

bool UCreateModelGridTool::CanAccept() const
{
	return true;
}

void UCreateModelGridTool::OnTick(float DeltaTime)
{
	if (bAutoPlacementPending)
		UpdateAutoPlacement();
}


void UCreateModelGridTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	ViewState = RenderAPI->GetCameraState();
	LastViewInfo.Initialize(RenderAPI);
	bHaveViewState = true;

	GS::GridToolUtil::DrawGridOriginCell(RenderAPI, GridFrame, InternalData->SourceGrid);
}







void UCreateModelGridTool::UpdateAutoPlacement()
{
	if (bAutoPlacementPending == false)
		return;
	if (bHaveViewState == false)
		return;

	double dx = Settings->NumCellsX * Settings->DimensionX;
	double dy = Settings->NumCellsY * Settings->DimensionY;
	double dz = Settings->NumCellsZ * Settings->DimensionZ;
	GS::AxisBox3d Bounds(-dx / 2, -dy / 2, 0, dx / 2, dy / 2, dz);

	// apply snapping
	auto SnapFunc = [this](FVector Position)
	{
		FToolContextSnappingConfiguration SnapConfig = GetToolManager()->GetContextQueriesAPI()->GetCurrentSnappingSettings();
		if (SnapConfig.bEnablePositionGridSnapping)
		{
			USceneSnappingManager* SnapManager = USceneSnappingManager::Find(GetToolManager());
			FSceneSnapQueryRequest Request;
			Request.RequestType = ESceneSnapQueryType::Position;
			Request.TargetTypes = ESceneSnapQueryTargetType::Grid;
			Request.Position = Position;
			TArray<FSceneSnapQueryResult> Results;
			if (SnapManager->ExecuteSceneSnapQuery(Request, Results))
				return Results[0].Position;
		}
		return Position;
	};

	FVector InitialPosition = GS::FindInitialPlacementPositionFromViewInfo(ViewState, LastViewInfo, GetTargetWorld(), Bounds, Bounds.BaseZ(), SnapFunc);

	GridFrame = FFrame3d(InitialPosition);
	PreviewMesh->SetTransform(GridFrame.ToFTransform());
	PlaneMechanic->SetPlaneWithoutBroadcast(GridFrame);

	if ( Settings->bAutoPosition == false )
		bAutoPlacementPending = false;
}


void UCreateModelGridTool::UpdateGridFrameFromGizmo(const UE::Geometry::FFrame3d& Frame)
{
	GridFrame = Frame;
	PreviewMesh->SetTransform( GridFrame.ToFTransform() );

	// lock auto-placement once gizmo has been used
	bAutoPlacementLocked = true;
	Settings->bPlacementIsLocked = true;
	bAutoPlacementPending = false;
}

FVector3d UCreateModelGridTool::GetCellDimensions() const
{
	if (Settings->bNonUniform)
		return FVector3d(Settings->DimensionX, Settings->DimensionY, Settings->DimensionZ);
	else
		return FVector3d(Settings->Dimension);
}

void UCreateModelGridTool::UpdatePreviewGridShape()
{
	ModelGrid NewGrid;
	NewGrid.Initialize(GetCellDimensions());

	ModelGridCell CurCell = ModelGridCell::SolidCell();
	CurCell.SetToSolidColor(GS::Color3b(255, 255, 255));


	if (Settings->GridType == ECreateModelGridInitialGrid::SingleCell)
	{
		NewGrid.ReinitializeCell(Vector3i(0, 0, 0), CurCell);
	}
	else if (Settings->GridType == ECreateModelGridInitialGrid::Plane || Settings->GridType == ECreateModelGridInitialGrid::Box)
	{
		Vector3i Dimensions(Settings->NumCellsX, Settings->NumCellsY,
			(Settings->GridType == ECreateModelGridInitialGrid::Plane) ? 1 : Settings->NumCellsZ);
		Vector3i MinCorner = Vector3i::Zero();
		if (Settings->Origin != ECreateModelGridOrigin::Zero)
		{
			MinCorner = Vector3i::Zero() - Dimensions / 2;
			if ( Settings->Origin == ECreateModelGridOrigin::BaseZ )
				MinCorner.Z = 0;	// bottom-center
		}
		GS::EnumerateDimensionsExclusive(Dimensions, [&](Vector3i Index) {
			NewGrid.ReinitializeCell(MinCorner + Index, CurCell);
		});
	}

	InternalData->SourceGrid = MoveTemp(NewGrid);
	OnGridCellDimensionsModified();
}


void UCreateModelGridTool::OnGridCellDimensionsModified()
{
	ModelGrid& SourceGrid = InternalData->SourceGrid;
	SourceGrid.SetNewCellDimensions(GetCellDimensions());

	FDynamicMesh3 FinalMesh;
	GS::ExtractGridFullMesh(SourceGrid, FinalMesh, /*materials=*/true, /*uvs=*/false);

	PreviewMesh->ReplaceMesh(MoveTemp(FinalMesh));
	ActiveMaterial->SetVectorParameterValue(TEXT("CellDimensions"), SourceGrid.GetCellDimensions());
}




void UCreateModelGridTool::Shutdown(EToolShutdownType ShutdownType)
{
	Settings->SaveProperties(this);

	DragAlignmentMechanic->Shutdown();
	DragAlignmentMechanic = nullptr;
	PlaneMechanic->Shutdown();
	PlaneMechanic = nullptr;

	PreviewMesh->Disconnect();
	PreviewMesh = nullptr;

	//if (ExistingActor.IsValid())
	//{
	//	ExistingActor.Get()->SetIsTemporarilyHiddenInEditor(false);
	//}

	if (ShutdownType == EToolShutdownType::Accept)
	{
		OnCreateNewModelGrid( MoveTemp(InternalData->SourceGrid) );
	}

}




void UCreateModelGridTool::OnCreateNewModelGrid(GS::ModelGrid&& SourceGrid)
{
	FString UseName = Settings->Name;
	if (UseName.Len() == 0)
		UseName = TEXT("ModelGrid");

	GetToolManager()->BeginUndoTransaction(LOCTEXT("ShutdownEdit", "Create ModelGrid"));

	FActorSpawnParameters SpawnInfo;
	AGSModelGridActor* NewActor = GetTargetWorld()->SpawnActor<AGSModelGridActor>(SpawnInfo);
	if (NewActor)
	{
		NewActor->GetGrid()->EditGrid([&](ModelGrid& EditGrid)
		{
			EditGrid = InternalData->SourceGrid;
		});

		NewActor->SetActorTransform(GridFrame.ToFTransform());
#if WITH_EDITOR
		NewActor->PostEditChange();
#endif
		ToolSelectionUtil::SetNewActorSelection(GetToolManager(), NewActor);
	}

	GetToolManager()->EndUndoTransaction();
}




bool UCreateModelGridTool::SupportsWorldSpaceFocusBox()
{
	return true;
}


FBox UCreateModelGridTool::GetWorldSpaceFocusBox()
{
	FAxisAlignedBox3d LocalBounds = PreviewMesh->GetMesh()->GetBounds();
	if (LocalBounds.IsEmpty())
		LocalBounds = FAxisAlignedBox3d(FVector::Zero(), FVector::Zero());
	FAxisAlignedBox3d WorldBox = FAxisAlignedBox3d::Empty();
	for (int j = 0; j < 8; ++j)
		WorldBox.Contain(GridFrame.FromFramePoint(LocalBounds.GetCorner(j)));
	return (FBox)WorldBox;
}


bool UCreateModelGridTool::SupportsWorldSpaceFocusPoint()
{
	return false;

}
bool UCreateModelGridTool::GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut)
{
	return false;
}






#undef LOCTEXT_NAMESPACE
