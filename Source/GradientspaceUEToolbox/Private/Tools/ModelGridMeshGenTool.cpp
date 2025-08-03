// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/ModelGridMeshGenTool.h"

#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"
#include "ToolSetupUtil.h"
#include "Selection/ToolSelectionUtil.h"
#include "ModelingObjectsCreationAPI.h"
#include "EditorModelingObjectsCreationAPI.h"
#include "Engine/World.h"
#include "Materials/Material.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "MeshOpPreviewHelpers.h" // UMeshOpPreviewWithBackgroundCompute
#include "DynamicMesh/MeshSharingUtil.h"

#include "GridActor/ModelGridActor.h"
#include "GridActor/UGSModelGrid.h"
#include "GridActor/UGSModelGridAsset.h"
#include "ModelGrid/ModelGrid.h"
#include "Operators/ModelGridMeshingOp.h"

#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "AssetUtils/Texture2DBuilder.h"
#include "Engine/StaticMesh.h"

#include "Utility/GSUEBakeUtil.h"
#include "Utility/GSUEMaterialUtil.h"


using namespace GS;
using namespace UE::Geometry;


#define LOCTEXT_NAMESPACE "UModelGridMeshGenTool"


bool UModelGridMeshGenToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	if (SceneState.SelectedActors.Num() == 1)
	{
		if (AGSModelGridActor* Actor = Cast<AGSModelGridActor>(SceneState.SelectedActors[0]))
			return true;
	}
	return false;
}

UInteractiveTool* UModelGridMeshGenToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	AGSModelGridActor* Actor = Cast<AGSModelGridActor>(SceneState.SelectedActors[0]);

	UModelGridMeshGenTool* Tool = NewObject<UModelGridMeshGenTool>(SceneState.ToolManager);
	Tool->SetTargetWorld(SceneState.World);
	Tool->SetSourceActor(Actor);

	return Tool;
}






UModelGridMeshGenTextureSettings::UModelGridMeshGenTextureSettings()
{
	BaseMaterial = LoadObject<UMaterial>(nullptr, TEXT("/GradientspaceUEToolbox/Materials/M_GridPixelPaintMaterial"));
}




void UModelGridMeshGenTool::SetTargetWorld(UWorld* World)
{
	TargetWorld = World;
}

UWorld* UModelGridMeshGenTool::GetTargetWorld()
{
	return TargetWorld.Get();
}

void UModelGridMeshGenTool::SetSourceActor(AGSModelGridActor* Actor)
{
	ExistingActor = Actor;
}






class FGridMeshGenOpFactory : public UE::Geometry::IDynamicMeshOperatorFactory
{
public:
	UModelGridMeshGenTool* SourceTool;
	TSharedPtr<FModelGridMeshingOp::FModelGridData> SourceData;

	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<FDynamicMeshOperator> MakeNewOperator() override
	{
		TUniquePtr<FModelGridMeshingOp> NewOperator = MakeUnique<FModelGridMeshingOp>();
		NewOperator->SourceData = SourceData;
		NewOperator->bInvertFaces = SourceTool->ToolSettings->bInvertFaces;
		NewOperator->bRemoveCoincidentFaces = SourceTool->ToolSettings->bRemoveHidden;
		NewOperator->bSelfUnion = SourceTool->ToolSettings->bSelfUnion;
		NewOperator->bOptimizePlanarAreas = SourceTool->ToolSettings->bOptimizePlanarAreas;
		NewOperator->bRecomputeGroups = (SourceTool->ToolSettings->GroupMode == EModelGridMeshGenGroupModes::PlanarAreas);

		if (SourceTool->ToolSettings->UVMode == EModelGridMeshGenUVModes::DiscardUVs)
			NewOperator->UVMode = FModelGridMeshingOp::EUVMode::Discard;
		else if (SourceTool->ToolSettings->UVMode == EModelGridMeshGenUVModes::Repack)
			NewOperator->UVMode = FModelGridMeshingOp::EUVMode::Repack;
		else if (SourceTool->ToolSettings->UVMode == EModelGridMeshGenUVModes::FacePixelsPack)
		{
			NewOperator->UVMode = FModelGridMeshingOp::EUVMode::PixelLayoutRepack;
			NewOperator->DimensionPixelCount = SourceTool->ToolSettings->PixelResolution;
			NewOperator->UVIslandPixelBorder = SourceTool->ToolSettings->IslandPixelBorder;
		}
		NewOperator->TargetUVResolution = FMath::Max(SourceTool->ToolSettings->TargetUVResolution, 32);

		NewOperator->SetResultTransform(SourceTool->WorldTransform);

		return NewOperator;
	}


};


UModelGridMeshGenTool::UModelGridMeshGenTool()
{
	UInteractiveTool::SetToolDisplayName(LOCTEXT("ToolName", "ModelGrid Mesh Generator"));
}


void UModelGridMeshGenTool::Setup()
{
	ToolSettings = NewObject<UModelGridMeshGenToolSettings>(this);
	ToolSettings->RestoreProperties(this);
	AddToolPropertySource(ToolSettings);

	ToolSettings->WatchProperty(ToolSettings->bRemoveHidden, [&](bool) { EditCompute->InvalidateResult(); });
	ToolSettings->WatchProperty(ToolSettings->bSelfUnion, [&](bool) { EditCompute->InvalidateResult(); });
	ToolSettings->WatchProperty(ToolSettings->bOptimizePlanarAreas, [&](bool) { EditCompute->InvalidateResult(); });
	ToolSettings->WatchProperty(ToolSettings->bInvertFaces, [&](bool) { EditCompute->InvalidateResult(); });
	ToolSettings->WatchProperty(ToolSettings->GroupMode, [&](EModelGridMeshGenGroupModes) { EditCompute->InvalidateResult(); });
	ToolSettings->WatchProperty(ToolSettings->bShowGroupColors, [&](bool) { UpdateVisualizationSettings(); });
	ToolSettings->WatchProperty(ToolSettings->UVMode, [&](EModelGridMeshGenUVModes) { EditCompute->InvalidateResult(); UpdatePropertySetVisibility(); });
	ToolSettings->WatchProperty(ToolSettings->PixelResolution, [&](int) { EditCompute->InvalidateResult(); });
	ToolSettings->WatchProperty(ToolSettings->IslandPixelBorder, [&](int) { EditCompute->InvalidateResult(); });
	ToolSettings->WatchProperty(ToolSettings->TargetUVResolution, [&](int) { EditCompute->InvalidateResult(); });


	OutputTypeProperties = NewObject<UCreateMeshObjectTypeProperties>(this);
	OutputTypeProperties->RestoreProperties(this);
	OutputTypeProperties->InitializeDefault();
	OutputTypeProperties->WatchProperty(OutputTypeProperties->OutputType, [this](FString) { OutputTypeProperties->UpdatePropertyVisibility(); });
	if (OutputTypeProperties->ShouldShowPropertySet())
	{
		AddToolPropertySource(OutputTypeProperties);
	}

	TextureOutputProperties = NewObject<UModelGridMeshGenTextureSettings>(this);
	TextureOutputProperties->RestoreProperties(this);
	AddToolPropertySource(TextureOutputProperties);
	SetToolPropertySourceEnabled(TextureOutputProperties, false);

	TSharedPtr<FModelGridMeshingOp::FModelGridData> SourceData = MakeShared<FModelGridMeshingOp::FModelGridData>();
	
	AGSModelGridActor* InitFromActor = ExistingActor.Get();
	if (InitFromActor)
	{
		UGSModelGrid* ModelGridContainer = InitFromActor->GetGrid();
		ModelGridContainer->ProcessGrid([&](const ModelGrid& SourceGrid)
		{
			SourceData->SourceGrid = SourceGrid;
		});

		UGSGridMaterialSet* MaterialSet = InitFromActor->GetGridComponent()->GetGridMaterials();
		FReferenceSetMaterialMap GridMaterialMap;
		UGSGridMaterialSet::BuildMaterialMapForSet(MaterialSet, GridMaterialMap);
		ActiveMaterialSet = GridMaterialMap.MaterialList;
	}
	if (ActiveMaterialSet.Num() == 0)
	{
		UMaterial* GridMaterial = LoadObject<UMaterial>(nullptr, TEXT("/GradientspaceUEToolbox/Materials/M_GridPreviewMaterial"));
		ActiveMaterialSet.Add(GridMaterial);
	}

	WorldTransform = InitFromActor->GetTransform();

	InitFromActor->SetIsTemporarilyHiddenInEditor(true);

	this->OperatorFactory = MakePimpl<FGridMeshGenOpFactory>();
	this->OperatorFactory->SourceData = SourceData;
	this->OperatorFactory->SourceTool = this;

	// Create the preview compute for the extrusion operation
	EditCompute = NewObject<UMeshOpPreviewWithBackgroundCompute>(this);
	EditCompute->Setup(GetTargetWorld(), this->OperatorFactory.Get());
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(EditCompute->PreviewMesh);
	EditCompute->ConfigureMaterials(ActiveMaterialSet, nullptr, nullptr);

	EditCompute->OnOpCompleted.AddLambda([this](const UE::Geometry::FDynamicMeshOperator* ResultOp) {
		ToolSettings->TexResolution = ((const FModelGridMeshingOp*)ResultOp)->PixelLayoutImageDimensionX_Result;
	});

	EditCompute->PreviewMesh->SetTransform((FTransform)WorldTransform);

	// is this the right tangents behavior?
	EditCompute->PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
	EditCompute->SetVisibility(true);

	EditCompute->InvalidateResult();

	UpdatePropertySetVisibility();
}



void UModelGridMeshGenTool::OnTick(float DeltaTime)
{
	EditCompute->Tick(DeltaTime);
}


bool UModelGridMeshGenTool::HasCancel() const { return true; }
bool UModelGridMeshGenTool::HasAccept() const { return true; }
bool UModelGridMeshGenTool::CanAccept() const 
{ 
	return EditCompute->HaveValidResult();
}

void UModelGridMeshGenTool::Shutdown(EToolShutdownType ShutdownType)
{
	ToolSettings->SaveProperties(this);
	OutputTypeProperties->SaveProperties(this);
	TextureOutputProperties->SaveProperties(this);

	ExistingActor.Get()->SetIsTemporarilyHiddenInEditor(false);

	if (EditCompute != nullptr)
	{
		FDynamicMeshOpResult ComputeResult = EditCompute->Shutdown();
		EditCompute->ClearOpFactory();
		EditCompute->OnOpCompleted.RemoveAll(this);
		EditCompute = nullptr;

		if (ShutdownType == EToolShutdownType::Accept)
		{
			bool bWantTexture = (ToolSettings->UVMode == EModelGridMeshGenUVModes::FacePixelsPack &&
				TextureOutputProperties->bCreateTexture);
			bool bWantBakedTexture = (bWantTexture && TextureOutputProperties->bBakeGridColors);

			UTexture2D* NewTexture = nullptr;
			if (bWantBakedTexture)
			{
				TImageBuilder<FVector4f> BakedImage;
				double ProjectionDistance = 100*FMathf::ZeroTolerance; // todo derive from grid dims?
				GS::BakeMeshVertexColorsToImage(*ComputeResult.Mesh,
					ToolSettings->TexResolution,
					ProjectionDistance, 1, true, BakedImage);

				// todo convert this to a utility...

				const bool bConvertToSRGB = true;
				const ETextureSourceFormat SourceDataFormat = TSF_BGRA8;

				FTexture2DBuilder TextureBuilder;
				TextureBuilder.Initialize(FTexture2DBuilder::ETextureType::Color, BakedImage.GetDimensions());
				TextureBuilder.Copy(BakedImage, bConvertToSRGB);
				TextureBuilder.Commit(false);

				const bool bConvertSourceToSRGB = bConvertToSRGB && SourceDataFormat == TSF_BGRA8;
				TextureBuilder.CopyImageToSourceData(BakedImage, SourceDataFormat, bConvertSourceToSRGB);
				NewTexture = TextureBuilder.GetTexture2D();

				NewTexture->Filter = TextureFilter::TF_Nearest;
				NewTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
				NewTexture->UpdateResource();
			}
			else if (bWantTexture)
			{
				NewTexture = FImageUtils::CreateCheckerboardTexture(FColor::White, FColor::White, ToolSettings->TexResolution);
				FTexture2DBuilder::CopyPlatformDataToSourceData(NewTexture, FTexture2DBuilder::ETextureType::Color);
				NewTexture->Filter = TextureFilter::TF_Nearest;
				NewTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
				NewTexture->UpdateResource();
			}

			GetToolManager()->BeginUndoTransaction(LOCTEXT("GridToMesh", "Grid to Mesh"));

			FCreateMeshObjectParams NewMeshObjectParams;
			NewMeshObjectParams.TargetWorld = GetTargetWorld();
			NewMeshObjectParams.Transform = ComputeResult.Transform;
			NewMeshObjectParams.BaseName = TEXT("MeshedGrid");
			for ( UMaterialInterface* Material : ActiveMaterialSet )
				NewMeshObjectParams.Materials.Add(Material);
			NewMeshObjectParams.SetMesh( MoveTemp(*ComputeResult.Mesh) );
			OutputTypeProperties->ConfigureCreateMeshObjectParams(NewMeshObjectParams);
			FCreateMeshObjectResult CreateMeshResult = UE::Modeling::CreateMeshObject(GetToolManager(), MoveTemp(NewMeshObjectParams));
			AActor* NewActor = CreateMeshResult.IsOK() ? CreateMeshResult.NewActor : nullptr;
			if (NewActor != nullptr)
				ToolSelectionUtil::SetNewActorSelection(GetToolManager(), CreateMeshResult.NewActor);

			if (NewActor != nullptr && NewTexture != nullptr)
			{
				FCreateTextureObjectParams NewTexObjectParams;
				NewTexObjectParams.TargetWorld = GetTargetWorld();
				if (CreateMeshResult.NewAsset != nullptr)
					NewTexObjectParams.StoreRelativeToObject = CreateMeshResult.NewAsset;
				NewTexObjectParams.BaseName = FString("T_") + NewMeshObjectParams.BaseName;
				NewTexObjectParams.GeneratedTransientTexture = NewTexture;
				FCreateTextureObjectResult CreateTexResult = UE::Modeling::CreateTextureObject(GetToolManager(), MoveTemp(NewTexObjectParams));
				UTexture2D* NewPixelTexture = CreateTexResult.IsOK() ? Cast<UTexture2D>(CreateTexResult.NewAsset) : nullptr;

				if (NewPixelTexture != nullptr && TextureOutputProperties->bCreateMaterial)
				{
					UMaterialInterface* PixelTexMaterial = TextureOutputProperties->BaseMaterial;
					if (PixelTexMaterial == nullptr)
						PixelTexMaterial = LoadObject<UMaterial>(nullptr, TEXT("/GradientspaceUEToolbox/Materials/M_GridPixelPaintMaterial"));

					FCreateMaterialObjectParams NewMaterialParams;
					NewMaterialParams.TargetWorld = GetTargetWorld();
					NewMaterialParams.StoreRelativeToObject = CreateTexResult.NewAsset;
					NewMaterialParams.MaterialToDuplicate = PixelTexMaterial;
					NewMaterialParams.BaseName = FString("M_") + NewMeshObjectParams.BaseName;
					FCreateMaterialObjectResult CreateMatResult = UE::Modeling::CreateMaterialObject(GetToolManager(), MoveTemp(NewMaterialParams));
					UMaterial* NewPixelMaterial = CreateMatResult.IsOK() ? Cast<UMaterial>(CreateMatResult.NewAsset) : nullptr;
					if (NewPixelMaterial != nullptr)
					{
						FString TexParamName = TextureOutputProperties->TexParameterName;
						if (! GS::SetMaterialTextureParameter(NewPixelMaterial, *TexParamName, NewPixelTexture))
							UE_LOG(LogTemp, Warning, TEXT("[GridToMeshTool] Texture Parameter %s not found in Material %s"), *TexParamName, *PixelTexMaterial->GetName());

						// assign to generated asset or component
						if (! GS::AssignMaterialToTarget(NewPixelMaterial, 0, CreateMeshResult.NewActor, CreateMeshResult.NewAsset, true))
							UE_LOG(LogTemp, Warning, TEXT("[GridToMeshTool] Could not find new Asset or Component to assign generated Material to"));

					} else
						UE_LOG(LogTemp, Warning, TEXT("[GridToMeshTool] Failed to derive duplicate Material from %s"), *PixelTexMaterial->GetName());
				} else
					UE_LOG(LogTemp, Warning, TEXT("[GridToMeshTool] Failed to create new Texture!"));
			}

			GetToolManager()->EndUndoTransaction();
		}
	}
}


void UModelGridMeshGenTool::UpdateVisualizationSettings()
{
	if (UDynamicMeshComponent* DMC = Cast<UDynamicMeshComponent>(EditCompute->PreviewMesh->GetRootComponent()))
	{
		EDynamicMeshComponentColorOverrideMode ColorMode = EDynamicMeshComponentColorOverrideMode::None;
		if (ToolSettings->bShowGroupColors)
			ColorMode = EDynamicMeshComponentColorOverrideMode::Polygroups;
		DMC->SetColorOverrideMode(ColorMode);
	}
}

void UModelGridMeshGenTool::UpdatePropertySetVisibility()
{
	bool bShowTextureProps = (ToolSettings->UVMode == EModelGridMeshGenUVModes::FacePixelsPack);
	SetToolPropertySourceEnabled(TextureOutputProperties, bShowTextureProps);
}


#undef LOCTEXT_NAMESPACE
