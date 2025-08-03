// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/ModelGridRasterizeTool.h"
#include "Misc/EngineVersionComparison.h"

#include "ModelGrid/ModelGrid.h"
#include "GridActor/UGSModelGrid.h"
#include "GridActor/UGSModelGridAsset.h"
#include "GridActor/ModelGridComponent.h"
#include "GridActor/ModelGridActor.h"

#include "Operators/RasterizeToGridOp.h"

#include "ModelingToolTargetUtil.h"
#include "Selection/ToolSelectionUtil.h"
#include "BaseGizmos/TransformProxy.h"
#include "InteractiveGizmoManager.h"
#include "ToolSetupUtil.h"
#include "MeshDescriptionToDynamicMesh.h"

#include "Engine/World.h"
#include "Editor/EditorEngine.h"   // for FActorLabelUtilities
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UModelGridRasterizeTool"



void UModelGridRasterizeTool::Setup()
{
	UBaseVoxelTool::Setup();

	int NumMeshes = Targets.Num();
	CombinedBounds = FAxisAlignedBox3d::Empty();
	for (int k = 0; k < NumMeshes; ++k)
	{
		TSharedPtr<FSharedConstDynamicMesh3> SharedMeshHandle = MakeShared<FSharedConstDynamicMesh3>(OriginalDynamicMeshes[k]);
		SharedMeshHandles.Add(SharedMeshHandle);
		FTransform MeshTransform = TransformProxies[k]->GetTransform();
		SharedMeshHandle->AccessSharedObject([&](const FDynamicMesh3& Mesh) {
			FAxisAlignedBox3d WorldBounds = TMeshQueries<FDynamicMesh3>::GetVerticesBounds(Mesh,
				Mesh.VertexIndicesItr(), MeshTransform);
			CombinedBounds.Contain(WorldBounds);
		});
	}
	Preview->InvalidateResult();

	Preview->OnOpCompleted.AddUObject(this, &UModelGridRasterizeTool::OnOpCompleted);

	if (UMaterial* GridMaterial = LoadObject<UMaterial>(nullptr, TEXT("/GradientspaceUEToolbox/Materials/M_GridEditMaterial")))
	{
		ActiveMaterial = UMaterialInstanceDynamic::Create(GridMaterial, this);
	}
	ActiveMaterials.Add(ActiveMaterial);

	Preview->ConfigureMaterials(ActiveMaterials, nullptr);

	RasterizeProperties->WatchProperty(RasterizeProperties->GridSize, [&](EGridRasterizeSizeType) { Preview->InvalidateResult(); });
	RasterizeProperties->WatchProperty(RasterizeProperties->CellCount, [&](int) { Preview->InvalidateResult(); });
	RasterizeProperties->WatchProperty(RasterizeProperties->Dimension, [&](double) { Preview->InvalidateResult(); });
	RasterizeProperties->WatchProperty(RasterizeProperties->bSampleVertexColors, [&](double) { Preview->InvalidateResult(); });

	// have to do this here so that it happens after base tool
	SetToolPropertySourceEnabled(VoxProperties, false);
	SetToolPropertySourceEnabled(OutputTypeProperties, false);
	SetToolPropertySourceEnabled(HandleSourcesProperties, false);
}


void UModelGridRasterizeTool::SetupProperties()
{
	Super::SetupProperties();

	RasterizeProperties = NewObject<UModelGridRasterizeToolProperties>(this);
	RasterizeProperties->RestoreProperties(this);
	AddToolPropertySource(RasterizeProperties);

	GridOutputProperties = NewObject<UModelGridOutputProperties>(this);
	AddToolPropertySource(GridOutputProperties);
	GridOutputProperties->OutputNewName = PrefixWithSourceNameIfSingleSelection(GetCreatedAssetName());

	SetToolDisplayName(LOCTEXT("ToolName", "Rasterize To Grid"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("ToolDescription", "Convert meshes to grids"),
		EToolMessageLevel::UserNotification);
}


void UModelGridRasterizeTool::SaveProperties()
{
	Super::SaveProperties();
	RasterizeProperties->SaveProperties(this);
}


void UModelGridRasterizeTool::ConvertInputsAndSetPreviewMaterials(bool bSetPreviewMesh)
{
	// base version always goes through default MD to DM conversion, which converts Linear colors to SRGB.
	// We want the colors in the meshes to be Linear, and we want to handle the various conversion options in DMC
	//Super::ConvertInputsAndSetPreviewMaterials(bSetPreviewMesh);

	OriginalDynamicMeshes.SetNum(Targets.Num());

	for (int ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
	{
		OriginalDynamicMeshes[ComponentIdx] = MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>();

		UPrimitiveComponent* SourceComponent = UE::ToolTarget::GetTargetComponent(Targets[ComponentIdx]);
		UDynamicMeshComponent* SourceDMC = Cast<UDynamicMeshComponent>(SourceComponent);
		if (SourceDMC != nullptr)
		{

#if UE_VERSION_OLDER_THAN(5,5,0)
			FDynamicMesh3 SourceMesh = UE::ToolTarget::GetDynamicMeshCopy(Targets[ComponentIdx], false);
#else
			FGetMeshParameters Params;
			Params.bWantMeshTangents = false;
			FDynamicMesh3 SourceMesh = UE::ToolTarget::GetDynamicMeshCopy(Targets[ComponentIdx], Params);
#endif

			EDynamicMeshVertexColorTransformMode ColorMode = SourceDMC->GetVertexColorSpaceTransformMode();
			// todo: convert SRGB to linear
			//ensure(ColorMode != EDynamicMeshVertexColorTransformMode::SRGBToLinear);
			//if (ColorMode == EDynamicMeshVertexColorTransformMode::SRGBToLinear)
			//	;		
			*OriginalDynamicMeshes[ComponentIdx] = MoveTemp(SourceMesh);
		}
		else 
		{
			// have to use MeshDescription api, assume static mesh and skip the linear-to-srgb conversion
			const FMeshDescription* MeshDescription = UE::ToolTarget::GetMeshDescription(Targets[ComponentIdx]);
			FMeshDescriptionToDynamicMesh Converter;
			Converter.bTransformVertexColorsLinearToSRGB = false;
			Converter.Convert(MeshDescription, *OriginalDynamicMeshes[ComponentIdx]);
		}
	}


	Preview->ConfigureMaterials(
		ToolSetupUtil::GetVertexColorMaterial(GetToolManager()),
		ToolSetupUtil::GetDefaultWorkingMaterial(GetToolManager())
	);


	if (!RasterizeProperties->bFillHolesAndSolidify && HasOpenBoundariesInMeshInputs())
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("NonClosedMeshWarning", "Some input meshes are not closed solids, consider using VoxWrap Tool first to get better results"),
			EToolMessageLevel::UserWarning);
	}
}




TUniquePtr<FDynamicMeshOperator> UModelGridRasterizeTool::MakeNewOperator()
{
	TUniquePtr<FRasterizeToGridOp> NewOperator = MakeUnique<FRasterizeToGridOp>();

	int NumMeshes = SharedMeshHandles.Num();
	if (NumMeshes == 0) return NewOperator;		// happens at startup due to dumb shit

	for (int k = 0; k < NumMeshes; ++k)
	{
		NewOperator->OriginalMeshesShared.Add(SharedMeshHandles[k]);
		NewOperator->WorldTransforms.Add(TransformProxies[k]->GetTransform());
	}
	
	NewOperator->SetResultTransform(FTransform());

	if (RasterizeProperties->GridSize == EGridRasterizeSizeType::CellCount)
	{
		int UseGridSize = RasterizeProperties->CellCount;
		double UseDimension = CombinedBounds.Diagonal().GetAbsMax() / (double)UseGridSize;
		NewOperator->CellDimensions = FVector3d::One() * UseDimension;
	}
	else if (RasterizeProperties->GridSize == EGridRasterizeSizeType::WorldDimension)
	{
		FVector3d CurDimensions = FVector3d::One() * RasterizeProperties->Dimension;
		if (RasterizeProperties->bNonUniform)
		{
			CurDimensions = FVector3d(RasterizeProperties->DimensionX, RasterizeProperties->DimensionY, RasterizeProperties->DimensionZ);
		}
		double UseDimension = RasterizeProperties->Dimension;
		NewOperator->CellDimensions = CurDimensions;
		while ((CombinedBounds.Diagonal() / CurDimensions).GetAbsMax() > 512)
		{
			CurDimensions *= 0.9;
			NewOperator->CellDimensions = CurDimensions;
		}
	}
	else
	{
		NewOperator->CellDimensions = FVector3d(50, 50, 50);
	}

	NewOperator->bSampleVertexColors = RasterizeProperties->bSampleVertexColors;
	NewOperator->bOutputSRGBColors = true;

	return NewOperator;
}


void UModelGridRasterizeTool::OnOpCompleted(const UE::Geometry::FDynamicMeshOperator* Operator)
{
	FRasterizeToGridOp* RasterizeOp = (FRasterizeToGridOp*)Operator;
	if (RasterizeOp->ResultGrid.IsValid())
	{
		LastResultGrid = RasterizeOp->ResultGrid;

		ActiveMaterial->SetVectorParameterValue(TEXT("CellDimensions"), LastResultGrid->GetCellDimensions());

		FVector3d EstCellCounts = (CombinedBounds.Diagonal() / LastResultGrid->GetCellDimensions());
		RasterizeProperties->CellCounts = FString::Printf(TEXT("%d %d %d"),
			FMath::CeilToInt(EstCellCounts.X), FMath::CeilToInt(EstCellCounts.Y), FMath::CeilToInt(EstCellCounts.Z));
	}
}


void UModelGridRasterizeTool::OnShutdown(EToolShutdownType ShutdownType)
{
	SaveProperties();
	TransformProperties->SaveProperties(this);

	FTransform WorldTransform = Preview->PreviewMesh->GetTransform();
	FDynamicMeshOpResult Result = Preview->Shutdown();
	// Restore (unhide) the source meshes
	for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
	{
		UE::ToolTarget::ShowSourceObject(Targets[ComponentIdx]);
	}

	if (ShutdownType == EToolShutdownType::Accept && LastResultGrid.IsValid())
	{
		GetToolManager()->BeginUndoTransaction(GetActionName());


		// max len explicitly enforced here, would ideally notify user
		FString UseBaseName = GridOutputProperties->OutputNewName.Left(250);
		if (UseBaseName.IsEmpty())
		{
			UseBaseName = PrefixWithSourceNameIfSingleSelection(GetCreatedAssetName());
		}

		FActorSpawnParameters SpawnInfo;
		AGSModelGridActor* NewActor = GetTargetWorld()->SpawnActor<AGSModelGridActor>(SpawnInfo);
		if (NewActor)
		{
			NewActor->GetGrid()->EditGrid([&](ModelGrid& EditGrid)
			{
				EditGrid = MoveTemp(*LastResultGrid);
			});

			FFrame3d UseFrame = (FFrame3d)WorldTransform;
			NewActor->SetActorTransform(((FFrame3d)UseFrame).ToFTransform());

			FActorLabelUtilities::SetActorLabelUnique(NewActor, UseBaseName);

			NewActor->PostEditChange();

			ToolSelectionUtil::SetNewActorSelection(GetToolManager(), NewActor);

			// process exisiting actors
			TArray<AActor*> Actors;
			for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
			{
				AActor* Actor = UE::ToolTarget::GetTargetActor(Targets[ComponentIdx]);
				Actors.Add(Actor);
			}
			AActor* KeepActor = nullptr;
			GridOutputProperties->ApplyMethod(Actors, GetToolManager(), KeepActor);
		}

		GetToolManager()->EndUndoTransaction();
	}

	UInteractiveGizmoManager* GizmoManager = GetToolManager()->GetPairedGizmoManager();
	GizmoManager->DestroyAllGizmosByOwner(this);

	// release handles
	for (auto Handle : SharedMeshHandles)
	{
		Handle->ReleaseSharedObject();
	}
}


FString UModelGridRasterizeTool::GetCreatedAssetName() const
{
	return TEXT("Grid");
}

FText UModelGridRasterizeTool::GetActionName() const
{
	return LOCTEXT("RasterizeToGrid", "Rasterize");
}



#undef LOCTEXT_NAMESPACE
