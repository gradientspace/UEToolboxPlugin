// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/ImportGridTool.h"

#include "HAL/Platform.h"

#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"
#include "ToolSetupUtil.h"
#include "Selection/ToolSelectionUtil.h"
#include "ModelingObjectsCreationAPI.h"
#include "EditorModelingObjectsCreationAPI.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "MeshOpPreviewHelpers.h" // UMeshOpPreviewWithBackgroundCompute
#include "SceneQueries/SceneSnappingManager.h"

#include "GridActor/ModelGridActor.h"
#include "GridActor/UGSModelGrid.h"
#include "GridActor/UGSModelGridAsset.h"
#include "GridActor/UGSModelGridAssetUtils.h"
#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridMeshCache.h"
#include "ModelGrid/ModelGridEditMachine.h"
#include "GridIO/MagicaVoxReader.h"
#include "GenericGrid/RasterizeToGrid.h"
#include "GenericGrid/ImageToGrid.h"
#include "Utility/GridToolUtil.h"
#include "Utility/GSUEModelGridUtil.h"

#include "Image/ImageBuilder.h"
#include "ModelingOperators.h"

#include "Utility/PlacementUtils.h"

#include "Engine/World.h"
#include "Materials/Material.h"
#include "Editor/EditorEngine.h"

// for file picker on export
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Settings/PersistentToolSettings.h"

// for image import
#ifdef LoadImage
#undef LoadImage
#endif
// ^^ For some reason, the windows define LoadImage -> LoadImageW is active here and will mess up FImageUtils::LoadImage
#include "ImageUtils.h"
#include "IImageWrapperModule.h"
#include "ImageWrapperHelper.h"


using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UModelGridImportTool"

bool UModelGridImportToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return true;
}

UInteractiveTool* UModelGridImportToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UModelGridImportTool* Tool = NewObject<UModelGridImportTool>(SceneState.ToolManager);
	Tool->SetTargetWorld(SceneState.World);

	//if (SceneState.SelectedActors.Num() == 1)
	//{
	//	AGSModelGridActor* Actor = Cast<AGSModelGridActor>(SceneState.SelectedActors[0]);
	//	if (Actor)
	//	{
	//		Tool->SetExistingActor(Actor);
	//	}
	//}

	return Tool;
}



struct FImportedModelGridData
{
	ModelGrid SourceGrid;

	// if this is initialized, we want to export multiple sub-grids...
	TArray<ModelGrid> ObjectGrids;
	TArray<FFrame3d> ObjectFrames;
};


class GridImportPreviewOp : public FDynamicMeshOperator
{
public:
	virtual ~GridImportPreviewOp() {}

	TSharedPtr<FImportedModelGridData> SourceData;

	virtual void CalculateResult(FProgressCancel* Progress) override
	{
		*ResultMesh = FDynamicMesh3();
		CalculateResultInPlace(*ResultMesh, Progress);
	}

	virtual bool CalculateResultInPlace(FDynamicMesh3& EditMesh, FProgressCancel* Progress)
	{
		FDynamicMesh3 FinalMesh;
		GS::ExtractGridFullMesh(SourceData->SourceGrid, FinalMesh, /*materials=*/true, /*uvs=*/false);
		EditMesh = MoveTemp(FinalMesh);
		return true;
	}

};



class FGridImportPreviewOpFactory : public UE::Geometry::IDynamicMeshOperatorFactory
{
public:
	UModelGridImportTool* SourceTool;
	TSharedPtr<FImportedModelGridData> SourceData;

	virtual TUniquePtr<FDynamicMeshOperator> MakeNewOperator() override
	{
		TUniquePtr<GridImportPreviewOp> NewOperator = MakeUnique<GridImportPreviewOp>();
		NewOperator->SourceData = SourceData;
		NewOperator->SetResultTransform(FTransformSRT3d());
		return NewOperator;
	}

};



void UModelGridImportTool_ImportFunctions::SelectMagicaVOX()
{
	Tool.Get()->SelectFileToImport(EImportModelGridImportType::MagicaVoxFormat);
}
void UModelGridImportTool_ImportFunctions::SelectImage()
{
	Tool.Get()->SelectFileToImport(EImportModelGridImportType::ImageFormat);
}


UModelGridImportTool::UModelGridImportTool()
{
	UInteractiveTool::SetToolDisplayName(LOCTEXT("ToolName", "Import ModelGrid"));
}

void UModelGridImportTool::SetTargetWorld(UWorld* World)
{
	TargetWorld = World;
}

UWorld* UModelGridImportTool::GetTargetWorld()
{
	return TargetWorld.Get();
}

void UModelGridImportTool::SetExistingActor(AGSModelGridActor* Actor)
{
	ExistingActor = Actor;
}



void UModelGridImportTool::Setup()
{
	this->OperatorFactory = MakePimpl<FGridImportPreviewOpFactory>();
	this->OperatorFactory->SourceTool = this;

	ImportFunctions = NewObject<UModelGridImportTool_ImportFunctions>(this);
	ImportFunctions->Tool = this;
	ImportFunctions->RestoreProperties(this);

	ImportFunctions->WatchProperty(ImportFunctions->Dimension, [&](double) { OnGridCellDimensionsModified(); });
	ImportFunctions->WatchProperty(ImportFunctions->bNonUniform, [&](bool) { OnGridCellDimensionsModified(); });
	ImportFunctions->WatchProperty(ImportFunctions->DimensionX, [&](double) { OnGridCellDimensionsModified(); });
	ImportFunctions->WatchProperty(ImportFunctions->DimensionY, [&](double) { OnGridCellDimensionsModified(); });
	ImportFunctions->WatchProperty(ImportFunctions->DimensionZ, [&](double) { OnGridCellDimensionsModified(); });
	ImportFunctions->WatchProperty(ImportFunctions->bAutoPosition, [&](bool bNewValue) { if (bNewValue) { ImportFunctions->bPlacementIsLocked = false; bAutoPlacementPending = true; } });

	AddToolPropertySource(ImportFunctions);


	VoxFormatOptions = NewObject<UModelGridImportTool_VoxOptions>(this);
	VoxFormatOptions->RestoreProperties(this);
	VoxFormatOptions->WatchProperty(VoxFormatOptions->bIgnoreColors, [&](bool) { UpdateImport(); });
	VoxFormatOptions->WatchProperty(VoxFormatOptions->bCombineAllObjects, [&](bool) { UpdateImport(); });
	VoxFormatOptions->WatchProperty(VoxFormatOptions->ObjectOrigins, [&](EVoxObjectPivotLocation) { UpdateImport(); });
	VoxFormatOptions->WatchProperty(VoxFormatOptions->bFlipX, [&](bool) { UpdateImport(); });
	AddToolPropertySource(VoxFormatOptions);
	SetToolPropertySourceEnabled(VoxFormatOptions, false);

	ImageFormatOptions = NewObject<UModelGridImportTool_ImageOptions>(this);
	ImageFormatOptions->RestoreProperties(this);
	ImageFormatOptions->WatchProperty(ImageFormatOptions->bSRGB, [&](bool) { UpdateImport(); });
	ImageFormatOptions->WatchProperty(ImageFormatOptions->ImagePlane, [&](EImportModelGridImagePlane) { UpdateImport(); });
	ImageFormatOptions->WatchProperty(ImageFormatOptions->bFlipX, [&](bool) { UpdateImport(); });
	ImageFormatOptions->WatchProperty(ImageFormatOptions->bFlipY, [&](bool) { UpdateImport(); });
	ImageFormatOptions->WatchProperty(ImageFormatOptions->bRotate90, [&](bool) { UpdateImport(); });
	ImageFormatOptions->WatchProperty(ImageFormatOptions->bAlphaCrop, [&](bool) { UpdateImport(); });
	AddToolPropertySource(ImageFormatOptions);
	SetToolPropertySourceEnabled(ImageFormatOptions, false);
	

	OutputOptions = NewObject<UModelGridImportTool_OutputOptions>(this);
	OutputOptions->RestoreProperties(this);
	AddToolPropertySource(OutputOptions);


	// Create the preview compute for the extrusion operation
	EditCompute = NewObject<UMeshOpPreviewWithBackgroundCompute>(this);
	EditCompute->Setup(GetTargetWorld(), this->OperatorFactory.Get());
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(EditCompute->PreviewMesh);

	if (UMaterial* GridMaterial = LoadObject<UMaterial>(nullptr, TEXT("/GradientspaceUEToolbox/Materials/M_GridEditMaterial")))
	{
		ActiveMaterial = UMaterialInstanceDynamic::Create(GridMaterial, this);
	}
	ActiveMaterials.Add(ActiveMaterial);
	EditCompute->ConfigureMaterials(ActiveMaterials, nullptr);

	GridFrame = FFrame3d();
	EditCompute->PreviewMesh->SetTransform(GridFrame.ToFTransform());

	// is this the right tangents behavior?
	EditCompute->PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::NoTangents);
	EditCompute->SetVisibility(true);

	GetToolManager()->DisplayMessage(
		LOCTEXT("StartupMessage", "Click the Select File button to choose a file to import"),
		EToolMessageLevel::UserNotification);
}

bool UModelGridImportTool::CanAccept() const
{
	return OperatorFactory->SourceData.IsValid();
}

void UModelGridImportTool::OnTick(float DeltaTime)
{
	if (bAutoPlacementPending)
		UpdateAutoPlacement();

	EditCompute->PreviewMesh->SetTransform(GridFrame.ToFTransform());
	EditCompute->Tick(DeltaTime);
}


void UModelGridImportTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	ViewState = RenderAPI->GetCameraState();
	LastViewInfo.Initialize(RenderAPI);
	bHaveViewState = true;

	if (!OperatorFactory || OperatorFactory->SourceData.IsValid() == false) return;

	GS::GridToolUtil::DrawGridOriginCell(RenderAPI, GridFrame, OperatorFactory->SourceData->SourceGrid);
}



FVector3d UModelGridImportTool::GetCellDimensions() const
{
	if (ImportFunctions->bNonUniform)
		return FVector3d(ImportFunctions->DimensionX, ImportFunctions->DimensionY, ImportFunctions->DimensionZ);
	else
		return FVector3d(ImportFunctions->Dimension);
}


void UModelGridImportTool::OnGridCellDimensionsModified()
{
	if (this->OperatorFactory && this->OperatorFactory->SourceData.IsValid())
	{
		this->OperatorFactory->SourceData->SourceGrid.SetNewCellDimensions(GetCellDimensions());
		EditCompute->InvalidateResult();
	}

	ActiveMaterial->SetVectorParameterValue(TEXT("CellDimensions"), GetCellDimensions());
}


void UModelGridImportTool::UpdateAutoPlacement()
{
	if (bAutoPlacementPending == false)
		return;
	if (bHaveViewState == false)
		return;

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

	double dx = 500;
	double dy = 500;
	double dz = 500;
	GS::AxisBox3d Bounds(-dx / 2, -dy / 2, 0, dx / 2, dy / 2, dz);
	FVector InitialPosition = GS::FindInitialPlacementPositionFromViewInfo(ViewState, LastViewInfo, GetTargetWorld(), Bounds, Bounds.BaseZ(), SnapFunc);

	GridFrame = FFrame3d(InitialPosition);

	if (ImportFunctions->bAutoPosition == false)
		bAutoPlacementPending = false;
}


void UModelGridImportTool::SelectFileToImport(EImportModelGridImportType ImportFormat)
{
	bool bFolderSelected = false;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return;

	//void* TopWindowHandle = FSlateApplication::Get().GetActiveTopLevelWindow().IsValid() ? FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle() : nullptr;
	const void* TopWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	FString DefaultPath = UGradientspaceUEToolboxToolSettings::GetLastGridImportFolder();
	FString VoxFilterType = TEXT("Vox File (*.vox)|*.vox");
	FString ImageFilterType(ImageWrapperHelper::GetImageFilesFilterString(false));

	//FString FileTypeFilter = VoxFilterType + TEXT("|") + ImageFilterType;
	FString FileTypeFilter =
		(ImportFormat == EImportModelGridImportType::ImageFormat) ? ImageFilterType : VoxFilterType;

	TArray<FString> OutFiles;
	int OutFilterIndex = -1;

	//FVector3d CellDimensions = GetCellDimensions();

	SetToolPropertySourceEnabled(VoxFormatOptions, false);
	SetToolPropertySourceEnabled(ImageFormatOptions, false);

	bool bSelected = DesktopPlatform->OpenFileDialog(TopWindowHandle,
		TEXT("Select File to Import"), DefaultPath, TEXT(""), FileTypeFilter, EFileDialogFlags::None, OutFiles, OutFilterIndex);
	if (!bSelected)
		return;

	bool bModified = false;
	if (ImportFormat == EImportModelGridImportType::MagicaVoxFormat)
	{
		CurrentImportFilePath = OutFiles[0];
		ImportType = EImportModelGridImportType::MagicaVoxFormat;
		bModified = UpdateImport();
		if ( bModified )
			SetToolPropertySourceEnabled(VoxFormatOptions, true);
	}
	else if (ImportFormat == EImportModelGridImportType::ImageFormat)
	{
		CurrentImportFilePath = OutFiles[0];
		ImportType = EImportModelGridImportType::ImageFormat;
		bModified = UpdateImport();
		if (bModified)
			SetToolPropertySourceEnabled(ImageFormatOptions, true);
	}

	if (bModified)
	{
		ActiveMaterial->SetVectorParameterValue(TEXT("CellDimensions"), GetCellDimensions());

		ImportFunctions->CurFileName = FPaths::GetCleanFilename(CurrentImportFilePath);
		OutputOptions->AssetName = FPaths::GetBaseFilename(ImportFunctions->CurFileName);

		bAutoPlacementPending = true;
	}
		
	UGradientspaceUEToolboxToolSettings::SetLastGridImportFolder(FPaths::GetPath(CurrentImportFilePath));
}



bool UModelGridImportTool::UpdateImport()
{
	bool bUpdated = false;
	if (ImportType == EImportModelGridImportType::MagicaVoxFormat)
	{
		bUpdated = UpdateImport_MagicaVox();
	}
	else if (ImportType == EImportModelGridImportType::ImageFormat)
	{
		bUpdated = UpdateImport_Image();
	}

	if (bUpdated)
	{
		EditCompute->InvalidateResult();

		AxisBox3i ModifiedRegion = this->OperatorFactory->SourceData->SourceGrid.GetModifiedRegionBounds(0);
		ImportFunctions->CellCounts = FString::Printf(TEXT("%d x %d x %d"),
			ModifiedRegion.CountX(), ModifiedRegion.CountY(), ModifiedRegion.CountZ());
	}
	
	return bUpdated;
}


bool UModelGridImportTool::UpdateImport_MagicaVox()
{
	std::string StdStringPath(TCHAR_TO_UTF8(*CurrentImportFilePath));

	MagicaVoxReader::VOXReadOptions VOXOptions;
	VOXOptions.bIgnoreColors = VoxFormatOptions->bIgnoreColors;
	VOXOptions.bCombineAllObjects = VoxFormatOptions->bCombineAllObjects;
	VOXOptions.bIgnoreTransforms = (VOXOptions.bCombineAllObjects == false)
		&& (VoxFormatOptions->ObjectOrigins == EVoxObjectPivotLocation::ObjectOrigin);

	TArray<TSharedPtr<MagicaVoxReader::VOXGridObject>> ReadObjects;
	ReadObjects.Reserve(16);
	auto AllocateFunc = [&]() {
		int i = ReadObjects.Num();
		ReadObjects.Add(MakeShared<MagicaVoxReader::VOXGridObject>());
		ReadObjects[i]->Grid.Initialize(GetCellDimensions());
		return ReadObjects[i].Get();
	};
	bool bReadOK = MagicaVoxReader::Read(StdStringPath, AllocateFunc, VOXOptions);
	if (bReadOK == false || ReadObjects.Num() == 0)
		return false;

	auto PostProcessGrid = [&](ModelGrid& Grid) {
		ModelGridEditor Editor(Grid);
		if (VoxFormatOptions->bFlipX)
			Editor.FlipX();
		return Grid;
	};
	for (int k = 0; k < ReadObjects.Num(); ++k)
		PostProcessGrid(ReadObjects[k]->Grid);

	TSharedPtr<FImportedModelGridData> ImportData = MakeShared<FImportedModelGridData>();

	// if we read multiple objects, we want set the preview to be of the union of all of them.
	// for now do this the dumb way, just read again...
	if (ReadObjects.Num() > 1)
	{
		ImportData->ObjectGrids.SetNum(ReadObjects.Num());
		ImportData->ObjectFrames.Init(FFrame3d(), ReadObjects.Num());
		for (int k = 0; k < ReadObjects.Num(); ++k) {
			ImportData->ObjectGrids[k] = MoveTemp(ReadObjects[k]->Grid);

			FVector3d Translation = (FVector3d)ReadObjects[k]->Transform.Translation;
			Translation *= GetCellDimensions();
			
			ImportData->ObjectFrames[k] = FFrame3d(Translation);
		}

		VOXOptions.bCombineAllObjects = true;
		VOXOptions.bIgnoreTransforms = false;
		MagicaVoxReader::VOXGridObject Combined;
		Combined.Grid.Initialize(GetCellDimensions());
		MagicaVoxReader::Read(StdStringPath, [&]() { return &Combined; }, VOXOptions);
		PostProcessGrid(Combined.Grid);
		ImportData->SourceGrid = MoveTemp(Combined.Grid);
	}
	else {
		ImportData->SourceGrid = MoveTemp(ReadObjects[0]->Grid);
	}

	this->OperatorFactory->SourceData = ImportData;
	return true;
}

bool UModelGridImportTool::UpdateImport_Image()
{
	//IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	FImage ResultImage;
	if (FImageUtils::LoadImage(*CurrentImportFilePath, ResultImage))
	{
		ModelGrid ReadGrid;
		ReadGrid.Initialize(GetCellDimensions());

		ModelGridEditMachine GridEditor;
		GridEditor.SetCurrentGrid(ReadGrid);
		UniformGridAdapter Adapter;
		GridEditor.InitializeUniformGridAdapter(Adapter);

		FImageDimensions ImageDims((int)ResultImage.GetWidth(), (int)ResultImage.GetHeight());
		int PixelDimsX = ImageDims.GetWidth();
		int PixelDimsY = ImageDims.GetHeight();

		Vector2i AxisMapping(0, 1);
		Vector3i GridMinCoords(0, 0, 0);
		if (ImageFormatOptions->ImagePlane == EImportModelGridImagePlane::YZPlane) {
			AxisMapping = Vector2i(1, 2);
		} else if (ImageFormatOptions->ImagePlane == EImportModelGridImagePlane::XZPlane) {
			AxisMapping = Vector2i(0, 2);
		}

		bool bFlipX = !ImageFormatOptions->bFlipX;
		bool bFlipY = !ImageFormatOptions->bFlipY;		// always flip Y to start
		bool bSRGBConversion = ImageFormatOptions->bSRGB;

		if (ImageFormatOptions->bRotate90)
		{
			int tmp = AxisMapping.X; AxisMapping.X = AxisMapping.Y; AxisMapping.Y = tmp;
			bFlipY = !bFlipY;
		}

		Vector2i MinCoord = Vector2i::Zero();
		Vector2i MaxCoord = Vector2i(PixelDimsX - 1, PixelDimsY - 1);
		Vector2i ImageOrigin = MinCoord;

		if (ImageFormatOptions->bAlphaCrop)
		{
			FAxisAlignedBox2d CropBox = FAxisAlignedBox2d::Empty();
			for (int yi = MinCoord.Y; yi <= MaxCoord.Y; yi++)
			{
				for (int xi = MinCoord.X; xi <= MaxCoord.X; xi++)
				{
					FLinearColor PixelLinear = ResultImage.GetOnePixelLinear(xi, yi);
					if (PixelLinear.A > 0)
						CropBox.Contain(FVector2d(xi, yi));
				}
			}
			ImageOrigin = (Vector2i)(Vector2d)CropBox.Min;
			MaxCoord = (Vector2i)(Vector2d)(CropBox.Max - CropBox.Min);
		}

		ImageToGrid Im2Grid;
		Im2Grid.SetGrid(&Adapter);
		Im2Grid.Rasterize(
			MinCoord, MaxCoord,
			GridMinCoords, AxisMapping,
			[&](const Vector2i& PixelCoordIn, Color4b& ColorOut)
		{
			Vector2i PixelCoord = PixelCoordIn + ImageOrigin;
			if (!ImageDims.IsValidCoords(PixelCoord)) { check(false); return false; }
			int PixelX = (bFlipX) ? (PixelDimsX - 1 - PixelCoord.X) : PixelCoord.X;
			int PixelY = (bFlipY) ? (PixelDimsY - 1 - PixelCoord.Y) : PixelCoord.Y;
			FLinearColor PixelLinear = ResultImage.GetOnePixelLinear(PixelX, PixelY);
			FColor C = PixelLinear.ToFColor(bSRGBConversion);
			//FVector4f Pixel = Image.GetPixel(PixelCoord);
			//FColor C = ToLinearColor(Pixel).ToFColor(true);
			ColorOut = (Color4b)C;
			return true;
		});

		TSharedPtr<FImportedModelGridData> ImportData = MakeShared<FImportedModelGridData>();
		ImportData->SourceGrid = MoveTemp(ReadGrid);
		this->OperatorFactory->SourceData = ImportData;
		return true;
	}
	return false;
}


void UModelGridImportTool::Shutdown(EToolShutdownType ShutdownType)
{
	ImportFunctions->SaveProperties(this);
	OutputOptions->SaveProperties(this);
	VoxFormatOptions->SaveProperties(this);

	if (ExistingActor.IsValid())
	{
		ExistingActor.Get()->SetIsTemporarilyHiddenInEditor(false);
	}

	if (EditCompute != nullptr)
	{
		EditCompute->ClearOpFactory();
		EditCompute->OnOpCompleted.RemoveAll(this);
		EditCompute->Shutdown();
		EditCompute = nullptr;

		auto CreateAsset = [&](FString BaseName, ModelGrid&& SourceGrid, int Index = -1)
		{
			// create new asset if requested
			UGSModelGridAsset* NewGridAsset = nullptr;
			UModelingObjectsCreationAPI* ModelingCreationAPI = GetToolManager()->GetContextObjectStore()->FindContext<UModelingObjectsCreationAPI>();
			UEditorModelingObjectsCreationAPI* EditorAPI = Cast<UEditorModelingObjectsCreationAPI>(ModelingCreationAPI);
			FString NewAssetPath;
			FString UseName = (Index == -1) ? BaseName : FString::Printf(TEXT("%s%d"), *BaseName, Index);
			ECreateModelingObjectResult PathResult = EditorAPI->GetNewAssetPath(NewAssetPath, UseName, nullptr, GetTargetWorld());
			if (PathResult == ECreateModelingObjectResult::Ok)
			{
				NewGridAsset = GS::CreateModelGridAssetFromGrid(
					NewAssetPath,
					MoveTemp(SourceGrid));
			}
			if (!ensure(NewGridAsset))
				UE_LOG(LogTemp, Warning, TEXT("UModelGridImportTool::Shutdown: failed to create new Grid Asset"));
			return NewGridAsset;
		};


		if (ShutdownType == EToolShutdownType::Accept)
		{
			FString UseName = OutputOptions->AssetName;
			if (UseName.Len() == 0)
				UseName = TEXT("ModelGrid");

			TSharedPtr<FImportedModelGridData> ResultGridData = OperatorFactory->SourceData;

			TArray<ModelGrid> SourceGrids;
			TArray<FFrame3d> SourceFrames;
			if (ResultGridData->ObjectGrids.Num() > 0) {
				SourceGrids = MoveTemp(ResultGridData->ObjectGrids);
				SourceFrames = MoveTemp(ResultGridData->ObjectFrames);
			}
			else
			{
				SourceGrids.Add(MoveTemp(ResultGridData->SourceGrid));
				SourceFrames.Add(FFrame3d());
			}

			int NumOutputObjects = SourceGrids.Num();

			TArray<UGSModelGridAsset*> NewGridAssets;
			NewGridAssets.Init(nullptr, NumOutputObjects);
			if (OutputOptions->bCreateAsset) {
				for (int k = 0; k < NumOutputObjects; ++k)
					NewGridAssets[k] = CreateAsset(UseName, MoveTemp(SourceGrids[k]), k+1);
			}

			GetToolManager()->BeginUndoTransaction(LOCTEXT("ShutdownEdit", "Import ModelGrid"));

			TArray<AActor*> NewActors;

			for (int k = 0; k < NumOutputObjects; ++k)
			{
				FActorSpawnParameters SpawnInfo;
				AGSModelGridActor* NewActor = GetTargetWorld()->SpawnActor<AGSModelGridActor>(SpawnInfo);
				if (NewActor)
				{
					if (NewGridAssets[k]) {
						NewActor->GetGridComponent()->SetGridAsset(NewGridAssets[k]);
					} else {
						NewActor->GetGrid()->EditGrid([&](ModelGrid& EditGrid) {
							EditGrid = MoveTemp(SourceGrids[k]);
						});
					}

					FFrame3d LocalFrame = SourceFrames[k];
					FFrame3d ActorFrame = GridFrame.FromFrame(LocalFrame);
					NewActor->SetActorTransform(ActorFrame.ToFTransform());
					FActorLabelUtilities::SetActorLabelUnique(NewActor, UseName);

					NewActor->PostEditChange();
					NewActors.Add(NewActor);
				}
			}

			ToolSelectionUtil::SetNewActorSelection(GetToolManager(), NewActors);

			GetToolManager()->EndUndoTransaction();
		}
	}


}



bool UModelGridImportTool::SupportsWorldSpaceFocusBox()
{
	return true;
}


FBox UModelGridImportTool::GetWorldSpaceFocusBox()
{
	FAxisAlignedBox3d LocalBounds = FAxisAlignedBox3d::Empty();
	EditCompute->PreviewMesh->ProcessMesh([&](const FDynamicMesh3& Mesh)
	{
		LocalBounds = Mesh.GetBounds();
	});
	if (LocalBounds.IsEmpty())
		LocalBounds = FAxisAlignedBox3d(FVector::Zero(), FVector::Zero());
	FAxisAlignedBox3d WorldBox = FAxisAlignedBox3d::Empty();
	for (int j = 0; j < 8; ++j)
		WorldBox.Contain(GridFrame.FromFramePoint(LocalBounds.GetCorner(j)));
	return (FBox)WorldBox;
}


bool UModelGridImportTool::SupportsWorldSpaceFocusPoint()
{
	return false;

}
bool UModelGridImportTool::GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut)
{
	return false;
}



#undef LOCTEXT_NAMESPACE
