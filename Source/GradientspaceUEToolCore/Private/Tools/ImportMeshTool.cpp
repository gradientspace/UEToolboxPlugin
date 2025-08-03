// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/ImportMeshTool.h"

#include "HAL/Platform.h"
#include "Core/UEVersionCompat.h"


#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"
#include "ToolSetupUtil.h"
#include "Selection/ToolSelectionUtil.h"
#include "ModelingObjectsCreationAPI.h"
//#include "EditorModelingObjectsCreationAPI.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "ModelingOperators.h"
#include "MeshOpPreviewHelpers.h" // UMeshOpPreviewWithBackgroundCompute
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTransforms.h"
#include "Polygroups/PolygroupsGenerator.h"
#include "Selections/MeshConnectedComponents.h"
#include "CompGeom/PolygonTriangulation.h"
#include "SceneQueries/SceneSnappingManager.h"

#include "Image/ImageBuilder.h"
#include "ModelingOperators.h"

#include "MeshIO/OBJReader.h"
#include "MeshIO/OBJWriter.h"

#include "Utility/PlacementUtils.h"

#include "Engine/World.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"

// for file picker on import
#include "Platform/GSPlatformSubsystem.h"
#include "Settings/GSPersistentUEToolSettings.h"

#include "Math/GSIndex2.h"
#include "Core/FunctionRef.h"
#include <unordered_map>

using namespace GS;
using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UImportMeshTool"

bool UImportMeshToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return true;
}

UInteractiveTool* UImportMeshToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UImportMeshTool* Tool = NewObject<UImportMeshTool>(SceneState.ToolManager);
	Tool->SetTargetWorld(SceneState.World);
	return Tool;
}



struct FImportedMeshData
{
	//FDynamicMesh3 SourceMesh;
	EImportMeshToolFormatType FileFormat;
	GS::OBJFormatData OBJData;
};



struct TriVertexAttribBuilder
{
	// maps (source_attrib_index, vertex_index) to overlay_element_index
	std::unordered_map<Index2i, int> AttribVertMap;

	FunctionRef<int(int)> AppendSourceAttribToOverlayFunc;

	TriVertexAttribBuilder(FunctionRef<int(int)> AppendSourceAttribToOverlayFuncIn)
		: AppendSourceAttribToOverlayFunc(AppendSourceAttribToOverlayFuncIn) {}


	int GetElementID(int SourceAttribIndex, int VertexIndex)
	{
		Index2i Key(SourceAttribIndex, VertexIndex);
		auto found = AttribVertMap.find(Key);
		if (found != AttribVertMap.end())
			return found->second;

		int NewElementIndex = AppendSourceAttribToOverlayFunc(SourceAttribIndex);
		AttribVertMap.insert(
			std::pair<Index2i, int>(Index2i(SourceAttribIndex, VertexIndex), NewElementIndex));
		return NewElementIndex;
	}
	
};


class ImportMeshOp : public FDynamicMeshOperator
{
public:
	virtual ~ImportMeshOp() {}

	enum class EUpTransformMode
	{
		Unmodified = 0,
		YUpToZUp = 1
	};

	enum class EHandedTransformMode
	{
		Unmodified = 0,
		RHStoLHS = 1
	};


	enum class EPivotMode
	{
		Unmodified = 0,
		Center = 1,
		Base = 2
	};

	enum class EGroupLayerMode
	{
		FromFile = 0,
		PerPolygon = 1,
		ConnectedComponents = 2,
		UVIslands = 3
	};

	TSharedPtr<FImportedMeshData> SourceData;
	EUpTransformMode UpTransform = EUpTransformMode::Unmodified;
	EHandedTransformMode HandedTransform = EHandedTransformMode::Unmodified;
	double Scale = 1.0;
	bool bReverseOrientation = false;
	EPivotMode PivotMode = EPivotMode::Unmodified;
	EGroupLayerMode DefaultGroupLayerMode = EGroupLayerMode::FromFile;

	virtual void CalculateResult(FProgressCancel* Progress) override
	{
		*ResultMesh = FDynamicMesh3();
		CalculateResultInPlace(*ResultMesh, Progress);
	}

	virtual bool CalculateResultInPlace(FDynamicMesh3& EditMesh, FProgressCancel* Progress)
	{
		if (SourceData == nullptr) return false;

		const GS::OBJFormatData& OBJData = SourceData->OBJData;

		// update mesh...
		FDynamicMesh3 Mesh;
		Mesh.EnableTriangleGroups();

		Mesh.EnableAttributes();
		FDynamicMeshUVOverlay* UVLayer = Mesh.Attributes()->GetUVLayer(0);
		FDynamicMeshNormalOverlay* NormalLayer = Mesh.Attributes()->PrimaryNormals();

		bool bHaveColors = (OBJData.VertexColors.size() == OBJData.VertexPositions.size());
		if ( bHaveColors )
			Mesh.Attributes()->EnablePrimaryColors();

		// todo material support...
		//Mesh.Attributes()->EnableMaterialID();

		int NumVertices = OBJData.VertexPositions.size();
		for (Vector3d v : OBJData.VertexPositions)
		{
			Mesh.AppendVertex( Scale * v );
		}
		
		int NumNormals = OBJData.Normals.size();
		bool bHaveNormals = (NumNormals > 0);
		TriVertexAttribBuilder NormalBuilder(
			[&](int SourceAttribIndex) {
				if (SourceAttribIndex < 0 || SourceAttribIndex >= NumNormals) return -1;
				Vector3d n = OBJData.Normals[SourceAttribIndex];
				return NormalLayer->AppendElement((Vector3f)n);
		});

		int NumUVs = OBJData.UVs.size();
		bool bHaveUVs = (OBJData.UVs.size() > 0);
		TriVertexAttribBuilder UVBuilder(
			[&](int SourceAttribIndex) {
				if (SourceAttribIndex < 0 || SourceAttribIndex >= NumUVs) return -1;
				Vector2d uv = OBJData.UVs[SourceAttribIndex];
				return UVLayer->AppendElement((Vector2f)uv);
		});

		std::vector<int> NormalElements, UVElements;
		NormalElements.reserve(64); UVElements.reserve(64);
		TArray<FVector3d> Polygon;
		Polygon.Reserve(64);
		TArray<FIndex3i> PolygonTris;
		PolygonTris.Reserve(64);

		int FaceIndex = 0;
		for (const OBJFace& FaceInfo : OBJData.FaceStream)
		{
			const int* Positions = nullptr;
			const int* Normals = nullptr;
			const int* UVs = nullptr;
			int NV = 0;
			if (FaceInfo.FaceType == 0)
			{
				const OBJTriangle& TriData = OBJData.Triangles[FaceInfo.FaceIndex];
				Positions = TriData.Positions.AsPointer();
				Normals = TriData.Normals.AsPointer();
				UVs = TriData.UVs.AsPointer();
				NV = 3;
			}
			else if (FaceInfo.FaceType == 1)
			{
				const OBJQuad& QuadData = OBJData.Quads[FaceInfo.FaceIndex];
				Positions = QuadData.Positions.AsPointer();
				Normals = QuadData.Normals.AsPointer();
				UVs = QuadData.UVs.AsPointer();
				NV = 4;
			}
			else if (FaceInfo.FaceType == 2)
			{
				const OBJPolygon& PolyData = OBJData.Polygons[FaceInfo.FaceIndex];
				Positions = (const int*)&PolyData.Positions[0];
				Normals = (const int*)&PolyData.Normals[0];
				UVs = (const int*)&PolyData.UVs[0];
				NV = PolyData.Positions.size();
			}

			bool bPositionsOK = true;
			for (int j = 0; j < NV && bPositionsOK; ++j) 
				bPositionsOK = bPositionsOK && Mesh.IsVertex(Positions[j]);
			if (!bPositionsOK) continue;

			NormalElements.resize(NV);
			bool bNormalsOK = bHaveNormals;
			for (int j = 0; j < NV && bNormalsOK; ++j)
			{
				NormalElements[j] = NormalBuilder.GetElementID(Normals[j], Positions[j]);
				bNormalsOK = bNormalsOK && NormalElements[j] >= 0;
			}

			UVElements.resize(NV);
			bool bUVsOK = bHaveUVs;
			for (int j = 0; j < NV && bUVsOK; ++j)
			{
				UVElements[j] = UVBuilder.GetElementID(UVs[j], Positions[j]);
				bUVsOK = bUVsOK && UVElements[j] >= 0;
			}

			// todo clever-er group support...
			int UseGroupID = (DefaultGroupLayerMode == EGroupLayerMode::PerPolygon) ? FaceIndex : FaceInfo.GroupID;

			if (NV == 3)
			{
				int tid = Mesh.AppendTriangle(FIndex3i(Positions[0], Positions[1], Positions[2]), UseGroupID);
				if (tid >= 0)
				{
					if (bNormalsOK)
						NormalLayer->SetTriangle(tid, FIndex3i(NormalElements[0], NormalElements[1], NormalElements[2]));
					if (bUVsOK)
						UVLayer->SetTriangle(tid, FIndex3i(UVElements[0], UVElements[1], UVElements[2]));
				}
			}
			else if (NV == 4)
			{
				int ta = Mesh.AppendTriangle(FIndex3i(Positions[0], Positions[1], Positions[2]), UseGroupID);
				if (ta >= 0)
				{
					if (bNormalsOK)
						NormalLayer->SetTriangle(ta, FIndex3i(NormalElements[0], NormalElements[1], NormalElements[2]));
					if (bUVsOK)
						UVLayer->SetTriangle(ta, FIndex3i(UVElements[0], UVElements[1], UVElements[2]));
				}
				int tb = Mesh.AppendTriangle(FIndex3i(Positions[0], Positions[2], Positions[3]), UseGroupID);
				if (ta >= 0)
				{
					if (bNormalsOK)
						NormalLayer->SetTriangle(tb, FIndex3i(NormalElements[0], NormalElements[2], NormalElements[3]));
					if (bUVsOK)
						UVLayer->SetTriangle(tb, FIndex3i(UVElements[0], UVElements[2], UVElements[3]));
				}
			}
			else if (NV > 4)
			{
				GSUE::TArraySetNum(Polygon, NV, false);
				//Polygon.SetNum(NV, false);
				for (int i = 0; i < NV; ++i)
					Polygon[i] = Mesh.GetVertex(Positions[i]);
				
				PolygonTris.Reset();
				PolygonTriangulation::TriangulateSimplePolygon<double>(Polygon, PolygonTris, false);
				int NT = PolygonTris.Num();
				for (int ti = 0; ti < NT; ++ti)
				{
					FIndex3i PolygonTri = PolygonTris[ti];
					
					FIndex3i MeshTri(Positions[PolygonTri.A], Positions[PolygonTri.B], Positions[PolygonTri.C]);
					int tid = Mesh.AppendTriangle(MeshTri, UseGroupID);
					if (tid >= 0)
					{
						if (bNormalsOK)
							NormalLayer->SetTriangle(tid, FIndex3i(NormalElements[PolygonTri.A], NormalElements[PolygonTri.B], NormalElements[PolygonTri.C]));
						if (bUVsOK)
							UVLayer->SetTriangle(tid, FIndex3i(UVElements[PolygonTri.A], UVElements[PolygonTri.B], UVElements[PolygonTri.C]));
					}
				}
			}

			FaceIndex++;
		}

		// colors are per-vertex so they can be mapped directly
		if (bHaveColors)
		{
			FDynamicMeshColorOverlay* PrimaryColors = Mesh.Attributes()->PrimaryColors();
			for (int k = 0; k < NumVertices; ++k)
				PrimaryColors->AppendElement( (FVector3f)OBJData.VertexColors[k] );
			for (int tid : Mesh.TriangleIndicesItr())
			{
				FIndex3i Tri = Mesh.GetTriangle(tid);
				PrimaryColors->SetTriangle(tid, Tri);
			}
		}

		// generate polygroups if necessary
		if (DefaultGroupLayerMode != EGroupLayerMode::FromFile && DefaultGroupLayerMode != EGroupLayerMode::PerPolygon)
		{
			if (DefaultGroupLayerMode == EGroupLayerMode::ConnectedComponents)
			{
				FPolygroupsGenerator Generator(&Mesh);
				Generator.bApplyPostProcessing = false;
				Generator.FindPolygroupsFromConnectedTris();
			}
			else if (DefaultGroupLayerMode == EGroupLayerMode::UVIslands)
			{
				FPolygroupsGenerator Generator(&Mesh);
				Generator.bApplyPostProcessing = false;
				if (bHaveUVs)
					Generator.FindPolygroupsFromUVIslands(0);
				else
					Generator.FindPolygroupsFromConnectedTris();
			}
		}

		if (UpTransform != EUpTransformMode::Unmodified)
		{
			FAxisAlignedBox3d Bounds = Mesh.GetBounds();
			Vector3d Center = Bounds.Center();
			FTransformSRT3d Rotation = FTransformSRT3d::Identity();
			if (UpTransform == EUpTransformMode::YUpToZUp)
			{
				Rotation.SetRotation(FQuaterniond(FVector3d::UnitX(), 90.0, true));
			}
			MeshTransforms::ApplyTransform(Mesh, Rotation);
		}

		bool bFlip = bReverseOrientation;

		if (HandedTransform != EHandedTransformMode::Unmodified)
		{
			MeshTransforms::ApplyTransform(Mesh,
				[](FVector3d V) { return FVector3d(-V.X, V.Y, V.Z); },
				[](FVector3f N) { return FVector3f(-N.X, N.Y, N.Z); });
			// swapping handed-ness also swaps orientation, so we can skip invert
			bFlip = !bFlip;
		}

		if (PivotMode != EPivotMode::Unmodified)
		{
			FAxisAlignedBox3d Bounds = Mesh.GetBounds();
			Vector3d PivotPos = Bounds.Center();
			if (PivotMode == EPivotMode::Base)
				PivotPos.Z = Bounds.Min.Z;
			MeshTransforms::Translate(Mesh, -PivotPos);
		}

		if (bFlip)
			Mesh.ReverseOrientation(true);

		// should only compute unset normals...
		if (!bHaveNormals)
			FMeshNormals::QuickComputeVertexNormals(Mesh);

		EditMesh = MoveTemp(Mesh);

		return true;
	}
};



class FImportMeshOpFactory : public UE::Geometry::IDynamicMeshOperatorFactory
{
public:
	UImportMeshTool* SourceTool;
	TSharedPtr<FImportedMeshData> SourceData;

	virtual TUniquePtr<FDynamicMeshOperator> MakeNewOperator() override
	{
		TUniquePtr<ImportMeshOp> NewOperator = MakeUnique<ImportMeshOp>();
		NewOperator->SourceData = SourceData;
		double UnitsScale = 1.0;
		switch (SourceTool->TransformOptions->SourceUnits) {
			case EImportMeshToolSourceUnits::Meters: UnitsScale = 100.0; break;
			case EImportMeshToolSourceUnits::Millimeters: UnitsScale = 0.1; break;
			case EImportMeshToolSourceUnits::Inches: UnitsScale = 2.54; break;
			case EImportMeshToolSourceUnits::Feet: UnitsScale = 30.48; break;
		}
		NewOperator->Scale = SourceTool->TransformOptions->Scale * UnitsScale;
		NewOperator->bReverseOrientation = SourceTool->TransformOptions->bInvert;
		NewOperator->PivotMode = (ImportMeshOp::EPivotMode)(int)SourceTool->TransformOptions->PivotLocation;
		NewOperator->UpTransform = (ImportMeshOp::EUpTransformMode)(int)SourceTool->TransformOptions->UpTransform;
		NewOperator->HandedTransform = (ImportMeshOp::EHandedTransformMode)(int)SourceTool->TransformOptions->HandedTransform;
		NewOperator->DefaultGroupLayerMode = (ImportMeshOp::EGroupLayerMode)(int)SourceTool->OBJFormatOptions->GroupMode;
		NewOperator->SetResultTransform(FTransformSRT3d());
		return NewOperator;
	}
};



void UImportMeshTool_ImportFunctions::SelectMesh()
{
	Tool.Get()->SelectFileToImport();
}


UImportMeshTool::UImportMeshTool()
{
	UInteractiveTool::SetToolDisplayName(LOCTEXT("ToolName", "Import Mesh"));
}

void UImportMeshTool::SetTargetWorld(UWorld* World)
{
	TargetWorld = World;
}

UWorld* UImportMeshTool::GetTargetWorld()
{
	return TargetWorld.Get();
}

//void UImportMeshTool::SetExistingActor(AGSModelGridActor* Actor)
//{
//	ExistingActor = Actor;
//}



void UImportMeshTool::Setup()
{
	this->OperatorFactory = MakePimpl<FImportMeshOpFactory>();
	this->OperatorFactory->SourceTool = this;

	ImportFunctions = NewObject<UImportMeshTool_ImportFunctions>(this);
	ImportFunctions->Tool = this;
	ImportFunctions->RestoreProperties(this);
	AddToolPropertySource(ImportFunctions);

	TransformOptions = NewObject<UImportMeshTool_TransformOptions>(this);
	TransformOptions->RestoreProperties(this);
	TransformOptions->WatchProperty(TransformOptions->Scale, [&](double) { EditCompute->InvalidateResult(); });
	TransformOptions->WatchProperty(TransformOptions->bInvert, [&](bool) { EditCompute->InvalidateResult(); });
	TransformOptions->WatchProperty(TransformOptions->PivotLocation, [&](EImportMeshToolPivotMode) { EditCompute->InvalidateResult(); });
	TransformOptions->WatchProperty(TransformOptions->UpTransform, [&](EImportMeshToolUpMode) { EditCompute->InvalidateResult(); });
	TransformOptions->WatchProperty(TransformOptions->HandedTransform, [&](EImportMeshHandedMode) { EditCompute->InvalidateResult(); });
	TransformOptions->WatchProperty(TransformOptions->SourceUnits, [&](EImportMeshToolSourceUnits) { EditCompute->InvalidateResult(); });
	AddToolPropertySource(TransformOptions);

	OBJFormatOptions = NewObject<UImportMeshTool_OBJOptions>(this);
	OBJFormatOptions->RestoreProperties(this);
	OBJFormatOptions->WatchProperty(OBJFormatOptions->bIgnoreColors, [&](bool) { UpdateImport(); });
	OBJFormatOptions->WatchProperty(OBJFormatOptions->GroupMode, [&](EImportMeshToolGroupLayerType) { EditCompute->InvalidateResult(); });
	AddToolPropertySource(OBJFormatOptions);
	SetToolPropertySourceEnabled(OBJFormatOptions, false);

	OutputTypeProperties = NewObject<UImportMeshTool_CreateMeshProperties>(this);
	OutputTypeProperties->RestoreProperties(this);
	OutputTypeProperties->InitializeDefault();
	OutputTypeProperties->WatchProperty(OutputTypeProperties->OutputType, [this](FString) { OutputTypeProperties->UpdatePropertyVisibility(); });
	if (OutputTypeProperties->ShouldShowPropertySet())
	{
		AddToolPropertySource(OutputTypeProperties);
	}

	// Create the preview compute for the meshing operation
	EditCompute = NewObject<UMeshOpPreviewWithBackgroundCompute>(this);
	EditCompute->Setup(GetTargetWorld(), this->OperatorFactory.Get());
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(EditCompute->PreviewMesh);

	if (UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
	{
		ActiveMaterial = UMaterialInstanceDynamic::Create(DefaultMaterial, this);
	}
	ActiveMaterials.Add(ActiveMaterial);
	EditCompute->ConfigureMaterials(ActiveMaterials, nullptr);

	GridFrame = FFrame3d();
	EditCompute->PreviewMesh->SetTransform(GridFrame.ToFTransform());

	// is this the right tangents behavior?
	EditCompute->PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::NoTangents);
	EditCompute->SetVisibility(true);

	MeshElementsDisplay = NewObject<UMeshElementsVisualizerExt>(this);
	MeshElementsDisplay->CreateInWorld(GetTargetWorld(), GridFrame.ToFTransform());
	MeshElementsDisplay->SetMeshAccessFunction([this](UMeshElementsVisualizer::ProcessDynamicMeshFunc ProcessFunc) {
		EditCompute->PreviewMesh->ProcessMesh([this, &ProcessFunc](const FDynamicMesh3& Mesh)
		{
			ProcessFunc(Mesh);
		});
	});
	if (MeshElementsDisplay->Settings)
		MeshElementsDisplay->Settings->WireframeColor = FColor::Black;

	CurrentLocalBounds = FAxisAlignedBox3d(FVector::Zero(), 200);
	EditCompute->OnMeshUpdated.AddLambda([this](UMeshOpPreviewWithBackgroundCompute*) {
		MeshElementsDisplay->NotifyMeshChanged(); 

		CurrentLocalBounds = FAxisAlignedBox3d(FVector::Zero(), 200);
		EditCompute->PreviewMesh->ProcessMesh([&](const FDynamicMesh3& Mesh) {
			CurrentLocalBounds = Mesh.GetBounds();
		});
	});


	VisualizationOptions = NewObject<UImportMeshTool_VisualizationOptions>(this);
	VisualizationOptions->RestoreProperties(this);
	AddToolPropertySource(VisualizationOptions);
	VisualizationOptions->WatchProperty(VisualizationOptions->VisualizationMode, [this](EImportMeshToolVisualizationMode) { UpdateVisualizationMode(); });
	VisualizationOptions->WatchProperty(VisualizationOptions->bShowWireframe, [this](bool) { UpdateVisualizationMode(); });
	VisualizationOptions->WatchProperty(VisualizationOptions->bShowBorders, [this](bool) { UpdateVisualizationMode(); });
	VisualizationOptions->WatchProperty(VisualizationOptions->bShowUVSeams, [this](bool) { UpdateVisualizationMode(); });
	VisualizationOptions->WatchProperty(VisualizationOptions->bShowNormalSeams, [this](bool) { UpdateVisualizationMode(); });
	UpdateVisualizationMode();

	GetToolManager()->DisplayMessage(
		LOCTEXT("StartupMessage", "Click the Select File button to choose a file to import"),
		EToolMessageLevel::UserNotification);

	SelectFileToImport(StartupImportFilePath);
}

bool UImportMeshTool::CanAccept() const
{
	return OperatorFactory->SourceData.IsValid();
}

void UImportMeshTool::OnTick(float DeltaTime)
{
	EditCompute->Tick(DeltaTime);

	if (bAutoPlacementPending)
		UpdateAutoPlacement();

	EditCompute->PreviewMesh->SetTransform(GridFrame.ToFTransform());

	MeshElementsDisplay->OnTick(DeltaTime);
	MeshElementsDisplay->SetTransform(GridFrame.ToFTransform());
}


void UImportMeshTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	ViewState = RenderAPI->GetCameraState();
	LastViewInfo.Initialize(RenderAPI);
	bHaveViewState = true;

	if (!OperatorFactory || OperatorFactory->SourceData.IsValid() == false) return;

	//GS::GridToolUtil::DrawGridOriginCell(RenderAPI, GridFrame, OperatorFactory->SourceData->SourceGrid);
}


void UImportMeshTool::UpdateVisualizationMode()
{
	if (MeshElementsDisplay->Settings)
	{
		MeshElementsDisplay->Settings->bShowWireframe = VisualizationOptions->bShowWireframe;
		MeshElementsDisplay->Settings->bShowBorders = VisualizationOptions->bShowBorders;
		MeshElementsDisplay->Settings->bShowUVSeams = VisualizationOptions->bShowUVSeams;
		MeshElementsDisplay->Settings->bShowNormalSeams = VisualizationOptions->bShowNormalSeams;
		MeshElementsDisplay->ForceSettingsModified();
	}

	if (CurVisualizationMode == VisualizationOptions->VisualizationMode) return;

	UDynamicMeshComponent* DMComponent = Cast<UDynamicMeshComponent>(EditCompute->PreviewMesh->GetRootComponent());

	if (CurVisualizationMode != EImportMeshToolVisualizationMode::Shaded)
	{
		DMComponent->SetColorOverrideMode(EDynamicMeshComponentColorOverrideMode::None);
	}

	CurVisualizationMode = VisualizationOptions->VisualizationMode;
	if (CurVisualizationMode == EImportMeshToolVisualizationMode::VertexColors)
	{
		DMComponent->SetColorOverrideMode(EDynamicMeshComponentColorOverrideMode::VertexColors);
	}
	else if (CurVisualizationMode == EImportMeshToolVisualizationMode::Polygroups)
	{
		DMComponent->SetColorOverrideMode(EDynamicMeshComponentColorOverrideMode::Polygroups);
	}
}




void UImportMeshTool::UpdateAutoPlacement()
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

	AxisBox3d PlaceBounds = (GS::AxisBox3d)CurrentLocalBounds;
	Vector3d PlaceOrigin = PlaceBounds.BaseZ();
	FVector InitialPosition = GS::FindInitialPlacementPositionFromViewInfo(ViewState, LastViewInfo, GetTargetWorld(), PlaceBounds, PlaceOrigin, SnapFunc);

	GridFrame = FFrame3d(InitialPosition);

	if (TransformOptions->bAutoPosition == false)
		bAutoPlacementPending = false;
}



void UImportMeshTool::SelectFileToImport(FString UseSpecificFile)
{
	TArray<FString> OpenFiles;
	if (UseSpecificFile.IsEmpty() || FPaths::FileExists(UseSpecificFile) == false)
	{
		FString DefaultPath = UGSPersistentUEToolSettings::GetLastMeshImportFolder();
		FString MeshFilterType = TEXT("OBJ File (*.obj)|*.obj");
		FString FileTypeFilter = MeshFilterType;

		int OutFilterIndex = -1;

		SetToolPropertySourceEnabled(OBJFormatOptions, false);

		bool bSelected = UGSPlatformSubsystem::GetPlatformAPI().ShowModalOpenFilesDialog(
			TEXT("Select File to Import"), DefaultPath, TEXT(""), FileTypeFilter, OpenFiles, OutFilterIndex);
		if (!bSelected)
			return;
	}
	else
		OpenFiles.Add(UseSpecificFile);


	bool bModified = false;
	CurrentImportFilePath = OpenFiles[0];
	FString Extension = FPaths::GetExtension(CurrentImportFilePath);
	if (Extension.Compare(TEXT("obj"), ESearchCase::IgnoreCase) == 0)
	{
		ImportType = EImportMeshToolFormatType::OBJFormat;
		bModified = UpdateImport();
		if (bModified)
			SetToolPropertySourceEnabled(OBJFormatOptions, true);
	}

	if (bModified)
	{
		ImportFunctions->CurFileName = FPaths::GetCleanFilename(CurrentImportFilePath);
		OutputTypeProperties->ActorName = FPaths::GetBaseFilename(ImportFunctions->CurFileName);
		OutputTypeProperties->AssetName = FString(TEXT("SM_")) + OutputTypeProperties->ActorName;

		if (bEnableAutoPlacementOnLoad)
			bAutoPlacementPending = true;

		UGSPersistentUEToolSettings::AddFileToRecentMeshImportsList(CurrentImportFilePath);
	}

	if (UseSpecificFile.IsEmpty())
		UGSPersistentUEToolSettings::SetLastMeshImportFolder(FPaths::GetPath(CurrentImportFilePath));
}



bool UImportMeshTool::UpdateImport()
{
	bool bUpdated = false;
	if (ImportType == EImportMeshToolFormatType::OBJFormat)
	{
		bUpdated = UpdateImport_OBJ();
	}

	if (bUpdated)
	{
		EditCompute->InvalidateResult();
	}
	
	return bUpdated;
}


bool UImportMeshTool::UpdateImport_OBJ()
{
	std::string StdStringPath(TCHAR_TO_UTF8(*CurrentImportFilePath));

	GS::OBJReader::ReadOptions Options;
	GS::OBJFormatData OBJData;
	bool bReadOK = GS::OBJReader::ReadOBJ(StdStringPath, OBJData, Options);

	if (bReadOK)
	{
		TSharedPtr<FImportedMeshData> ImportData = MakeShared<FImportedMeshData>();
		ImportData->OBJData = MoveTemp(OBJData);
		ImportData->FileFormat = EImportMeshToolFormatType::OBJFormat;
		this->OperatorFactory->SourceData = ImportData;

		//GS::FileTextWriter Writer = GS::FileTextWriter::OpenFile("c:\\scratch\\TEST_WRITE_BACK.obj");
		//GS::OBJWriter::WriteOBJ(Writer, ImportData->OBJData);

		return true;
	}

	return false;
}


void UImportMeshTool::Shutdown(EToolShutdownType ShutdownType)
{
	ImportFunctions->SaveProperties(this);
	OutputTypeProperties->SaveProperties(this);
	TransformOptions->SaveProperties(this);
	OBJFormatOptions->SaveProperties(this);
	VisualizationOptions->SaveProperties(this);

	MeshElementsDisplay->Disconnect();

	//if (ExistingActor.IsValid())
	//{
	//	ExistingActor.Get()->SetIsTemporarilyHiddenInEditor(false);
	//}

	if (EditCompute != nullptr)
	{
		FDynamicMeshOpResult ComputeResult = EditCompute->Shutdown();
		EditCompute->ClearOpFactory();
		EditCompute->OnOpCompleted.RemoveAll(this);
		EditCompute = nullptr;

		if (ShutdownType == EToolShutdownType::Accept)
		{
			FString UseName = OutputTypeProperties->ActorName;
			if (UseName.Len() == 0)
				UseName = TEXT("Mesh");

			GetToolManager()->BeginUndoTransaction(LOCTEXT("ImportMesh", "Import Mesh"));

			FCreateMeshObjectParams NewMeshObjectParams;
			NewMeshObjectParams.TargetWorld = GetTargetWorld();
			NewMeshObjectParams.Transform = GridFrame.ToFTransform();
			NewMeshObjectParams.BaseName = UseName;
			//for (UMaterialInterface* Material : ActiveMaterialSet)
			//	NewMeshObjectParams.Materials.Add(Material);
			NewMeshObjectParams.SetMesh(MoveTemp(*ComputeResult.Mesh));
			OutputTypeProperties->ConfigureCreateMeshObjectParams(NewMeshObjectParams);
			FCreateMeshObjectResult Result = UE::Modeling::CreateMeshObject(GetToolManager(), MoveTemp(NewMeshObjectParams));
			if (Result.IsOK() && Result.NewActor != nullptr)
			{
				ToolSelectionUtil::SetNewActorSelection(GetToolManager(), Result.NewActor);
			}

			GetToolManager()->EndUndoTransaction();
		}
	}
}



bool UImportMeshTool::SupportsWorldSpaceFocusBox()
{
	return true;
}


FBox UImportMeshTool::GetWorldSpaceFocusBox()
{
	FAxisAlignedBox3d WorldBox = FAxisAlignedBox3d::Empty();
	for (int j = 0; j < 8; ++j)
		WorldBox.Contain( GridFrame.FromFramePoint(CurrentLocalBounds.GetCorner(j)) );
	return (FBox)WorldBox;
}


bool UImportMeshTool::SupportsWorldSpaceFocusPoint()
{
	return false;

}
bool UImportMeshTool::GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut)
{
	return false;
}



#undef LOCTEXT_NAMESPACE
