// Copyright Gradientspace Corp. All Rights Reserved.

#include "Tools/ExportMeshTool.h"
#include "InteractiveToolManager.h"
#include "ToolTargetManager.h"
#include "ModelingToolTargetUtil.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"

#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMeshEditor.h"

#include "Utility/GSUEMeshUtil.h"
#include "Mesh/PolyMesh.h"
#include "Core/TextIO.h"
#include <fstream>
#include "MeshIO/OBJWriter.h"

#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

// for directory picker on export
#include "Platform/GSPlatformSubsystem.h"
#include "Settings/GSPersistentUEToolSettings.h"

#include "Settings/GSToolSubsystem.h"

#include "Misc/EngineVersionComparison.h"


using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UGSExportMeshesTool"

const FToolTargetTypeRequirements& UGSExportMeshesToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements({
		UMeshDescriptionProvider::StaticClass(),
		UPrimitiveComponentBackedTarget::StaticClass()
		});
	return TypeRequirements;
}

UMultiSelectionMeshEditingTool* UGSExportMeshesToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UGSExportMeshesTool>(SceneState.ToolManager);
}


void UGSExportMeshesTool::Setup()
{
	UMultiSelectionMeshEditingTool::Setup();

	Settings = NewObject<UGSExportMeshesToolProperties>(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);
	Settings->WatchProperty(Settings->Format, [this](EGSExportMeshesToolFormat) { UpdateVisibleSettingsForFormat(); });
	Settings->NumTargetObjects = Targets.Num();

	OBJSettings = NewObject<UGSExportMeshesSettings_OBJFormat>(this);
	OBJSettings->RestoreProperties(this);
	AddToolPropertySource(OBJSettings);
	SetToolPropertySourceEnabled(OBJSettings, false);

	ToolActions = NewObject<UGSExportMeshesToolActions>(this);
	ToolActions->ParentTool = this;
	AddToolPropertySource(ToolActions);

	ExportButtonsTarget = MakeShared<FGSActionButtonTarget>();
	ExportButtonsTarget->ExecuteCommand = [this](FString CommandName) { OnExportButtonClicked(CommandName); };
	ToolActions->ExportButtons.Target = ExportButtonsTarget;

	ToolActions->ExportButtons.SetUIPreset_LargeSize();
	ToolActions->ExportButtons.ButtonSpacingHorz = 4.0;
	ToolActions->ExportButtons.AddAction("ExportToFolder",
		LOCTEXT("ExportToFolder", "Export To Folder"),
		LOCTEXT("ExportToFolderTooltip", "Export the meshes to a folder, with filenames based on Actor/Component names"));
	ToolActions->ExportButtons.AddAction("ExportToFile",
		LOCTEXT("ExportToFile", "Export To File"),
		LOCTEXT("ExportToFileTooltip", "Export the meshes to a specific Filename. Multiple meshes will automatically be combined if necessary"));

	PresetButtonsTarget = MakeShared<FGSActionButtonTarget>();
	PresetButtonsTarget->ExecuteCommand = [this](FString CommandName) { OnPresetButtonClicked(CommandName); };
	Settings->TransformPresets.Target = PresetButtonsTarget;

	Settings->TransformPresets.SetUIPreset_DetailsSize();
	Settings->TransformPresets.ButtonSpacingHorz = 2.0; Settings->TransformPresets.ButtonSpacingVert = 1.0;
	Settings->TransformPresets.AddAction("Maya",
		LOCTEXT("MayaPreset", "Maya/Houdini"),
		LOCTEXT("MayaPresetTooltip", "Configure for Maya, Houdini, Marmoset, Substance, Godot, Roblox, Minecraft (RHS/YUp)"));
	Settings->TransformPresets.AddAction("Max",
		LOCTEXT("MaxPreset", "Max/Blender"),
		LOCTEXT("MaxPresetTooltip", "Configure for Max, Blender, Sketchup (RHS/ZUp)"));
	Settings->TransformPresets.AddAction("ZBrush",
		LOCTEXT("ZBrushPreset", "ZBrush"),
		LOCTEXT("ZBrushPresetTooltip", "Configure for ZBrush, Unity, C4D, Lightwave (LHS/ZUp)"));


	SetToolDisplayName(LOCTEXT("ToolName", "Export Meshes"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Export Meshes (use the Export button in the settings panel)"),
		EToolMessageLevel::UserNotification);

	UpdateVisibleSettingsForFormat();

	if (UGSToolSubsystem* Subsystem = UGSToolSubsystem::Get())
		Subsystem->EnableToolFeedbackWidget(this);
}

void UGSExportMeshesTool::OnShutdown(EToolShutdownType ShutdownType)
{
	Settings->SaveProperties(this);
	OBJSettings->SaveProperties(this);

	if (UGSToolSubsystem* Subsystem = UGSToolSubsystem::Get())
		Subsystem->DisableToolFeedbackWidget(this);
}


void UGSExportMeshesTool::UpdateVisibleSettingsForFormat()
{
	SetToolPropertySourceEnabled(OBJSettings, false);

	if (Settings->Format == EGSExportMeshesToolFormat::OBJ)
	{
		SetToolPropertySourceEnabled(OBJSettings, true);
	}
}


void UGSExportMeshesTool::OnExportButtonClicked(FString CommandName)
{
	if (CommandName == TEXT("ExportToFolder")) {
		ExecuteExport_Folder();
	}
	else if (CommandName == TEXT("ExportToFile")) {
		ExecuteExport_File();
	}
}
void UGSExportMeshesTool::OnPresetButtonClicked(FString CommandName)
{
	if (CommandName == TEXT("Maya")) {
		ConfigurePreset(EExportPreset::Maya);
	}
	else if (CommandName == TEXT("Max")) {
		ConfigurePreset(EExportPreset::Max);
	}
	else if (CommandName == TEXT("ZBrush")) {
		ConfigurePreset(EExportPreset::ZBrush);
	}
}


void UGSExportMeshesTool::ConfigurePreset(EExportPreset Preset)
{
	if (Preset == EExportPreset::Maya)
	{
		Settings->UpTransform = EExportMeshToolUpMode::ZUpToYUp;
		Settings->HandedTransform = EExportMeshHandedMode::LHStoRHS;
	}
	else if (Preset == EExportPreset::Max)
	{
		Settings->UpTransform = EExportMeshToolUpMode::Unmodified;
		Settings->HandedTransform = EExportMeshHandedMode::LHStoRHS;
	}
	else if (Preset == EExportPreset::ZBrush)
	{
		Settings->UpTransform = EExportMeshToolUpMode::ZUpToYUp;
		Settings->HandedTransform = EExportMeshHandedMode::Unmodified;
	}
}


void UGSExportMeshesTool::ExecuteExport_Folder()
{
	FString LastExportFolder = UGSPersistentUEToolSettings::GetLastMeshExportFolder();
	GS::GSErrorSet Errors;

	if (Settings->bSelectFolderOnExport)
	{
		bool bFolderSelected = false;
		FString FolderName;
		bFolderSelected = UGSPlatformSubsystem::GetPlatformAPI().ShowModalSelectFolderDialog(
			TEXT("Select Output Folder..."),
			LastExportFolder,
			FolderName
		);

		if (!bFolderSelected) return;

		LastExportFolder = FolderName;
	}
	else
	{
		FString OutputFolder = Settings->PathStruct.Path;
		if (FPaths::DirectoryExists(OutputFolder) == false)
		{
			UE_LOG(LogTemp, Error, TEXT("GSExportMeshesTool:: Tried to export to path %s which does not exist"), *OutputFolder);
			GetToolManager()->DisplayMessage(
				FText::Format(LOCTEXT("InvalidOutputFolderError", "Folder {0} does not exist"), FText::FromString(OutputFolder) ),
				EToolMessageLevel::UserError);
			return;
		}

		LastExportFolder = OutputFolder;
	}

	FPaths::NormalizeDirectoryName(LastExportFolder);
	FString BaseExportPath = LastExportFolder;
	if (Targets.Num() > 1 && Settings->bCombineInputs)
	{
		ExecuteExport_Combined(BaseExportPath, nullptr, Errors);
	}
	else
	{
		ExecuteExport_MultiFile(BaseExportPath, Errors);
	}

	UGSPersistentUEToolSettings::SetLastMeshExportFolder(LastExportFolder);
	if (UGSToolSubsystem* Subsystem = UGSToolSubsystem::Get())
		Subsystem->SetToolFeedbackStrings(Errors.SortByLevel());
}




void UGSExportMeshesTool::ExecuteExport_File()
{
	FString LastExportFolder = UGSPersistentUEToolSettings::GetLastMeshExportFolder();
	GS::GSErrorSet Errors;

	FString DefaultFilename = TEXT("mesh") + GetFormatExtension();
	FString MeshFilterType = GetFormatFileFilter();
	FString FileTypeFilter = MeshFilterType;

	FString SaveFilename;
	bool bFileSelected = UGSPlatformSubsystem::GetPlatformAPI().ShowModalSaveFileDialog(
		TEXT("Select Output File..."),
		LastExportFolder,
		DefaultFilename,
		MeshFilterType,
		SaveFilename
	);
	if (!bFileSelected)
		return;

	FPaths::NormalizeDirectoryName(SaveFilename);
	LastExportFolder = FPaths::GetPath(SaveFilename);

	ExecuteExport_Combined(LastExportFolder, &SaveFilename, Errors);

	UGSPersistentUEToolSettings::SetLastMeshExportFolder(LastExportFolder);
	if (UGSToolSubsystem* Subsystem = UGSToolSubsystem::Get())
		Subsystem->SetToolFeedbackStrings(Errors.SortByLevel());
}


static void ApplyMeshTransforms(FDynamicMesh3& Mesh, bool bFlipHandedness, bool bConvertToYUp, 
	EExportMeshDestUnits UnitsConversion, double UniformScale)
{
	if (bFlipHandedness)
	{
		MeshTransforms::ApplyTransform(Mesh,
			[](FVector3d V) { return FVector3d(-V.X, V.Y, V.Z); },
			[](FVector3f N) { return FVector3f(-N.X, N.Y, N.Z); });
	}

	if (bConvertToYUp)
	{
		FTransformSRT3d Rotation = FTransformSRT3d::Identity();
		Rotation.SetRotation(FQuaterniond(FVector3d::UnitX(), -90.0, true));
		MeshTransforms::ApplyTransform(Mesh, Rotation);
	}
	
	
	if (UnitsConversion != EExportMeshDestUnits::Centimeters)
	{
		double UnitsScale = 1.0;
		switch (UnitsConversion) {
			case EExportMeshDestUnits::Meters: UnitsScale = 1.0/100.0; break;
			case EExportMeshDestUnits::Millimeters: UnitsScale = 1.0/0.1; break;
			case EExportMeshDestUnits::Inches: UnitsScale = 1.0/2.54; break;
			case EExportMeshDestUnits::Feet: UnitsScale = 1.0/30.48; break;
		}
		UniformScale *= UnitsScale;
	}

	if ( FMathd::Abs(UniformScale-1.0) > FMathd::ZeroTolerance )
		MeshTransforms::Scale(Mesh, FVector3d::One()*UniformScale, FVector3d::Zero());
}


static void ConvertMeshToPolyMesh(const FDynamicMesh3& SourceMesh, 
	EGSExportMeshesToolTopologyMode TopologyMode, int GroupLayer,
	GS::PolyMesh& PolyMeshOut, GS::GSErrorSet& ErrorsOut)
{
	if ( TopologyMode == EGSExportMeshesToolTopologyMode::GroupsAsPolygons )
		GS::ConvertDynamicMeshGroupsToPolyMesh(SourceMesh, GroupLayer, PolyMeshOut, &ErrorsOut);
	else
		GS::ConvertDynamicMeshToPolyMesh(SourceMesh, PolyMeshOut);

	if (ErrorsOut.ContainsErrorOfType(EGSStandardErrors::InvalidTopology_DegenerateFace)
		|| ErrorsOut.ContainsErrorOfType(EGSStandardErrors::InvalidTopology_MultipleGroupBoundaries))
	{
		ErrorsOut.AppendError(0, GS::EGSErrorLevel::Warning, "Some faces in the source mesh may be modified or missing in the output");
	}
}



void UGSExportMeshesTool::ExecuteExport_Combined(const FString& OutputBaseFolder, FString* UseFilePath, GS::GSErrorSet& Errors)
{
	bool bTransformToWorldSpace = (Settings->bWorldSpace || Targets.Num() > 1);

	FDynamicMesh3 CombinedMesh;
	int NumValid = 0;
	for (int32 k = 0; k < Targets.Num(); ++k)
	{
		AActor* TargetActor = UE::ToolTarget::GetTargetActor(Targets[k]);
		UPrimitiveComponent* TargetComponent = UE::ToolTarget::GetTargetComponent(Targets[k]);
		if (TargetActor == nullptr || TargetComponent == nullptr)
			continue;

		FTransform SourceTransform = (FTransform)UE::ToolTarget::GetLocalToWorldTransform(Targets[k]);

#if UE_VERSION_OLDER_THAN(5,5,0)
		FDynamicMesh3 SourceMesh = UE::ToolTarget::GetDynamicMeshCopy(Targets[k], true);
#else
		FGetMeshParameters Params;
		Params.bWantMeshTangents = false;
		FDynamicMesh3 SourceMesh = UE::ToolTarget::GetDynamicMeshCopy(Targets[k], Params);
#endif

		if (bTransformToWorldSpace)
		{
			MeshTransforms::ApplyTransform(SourceMesh, (FTransformSRT3d)SourceTransform, true);
		}

		if (CombinedMesh.TriangleCount() == 0)
		{
			CombinedMesh = MoveTemp(SourceMesh);
		}
		else
		{
			FDynamicMeshEditor Editor(&CombinedMesh);
			FMeshIndexMappings Tmp;
			Editor.AppendMesh(&SourceMesh, Tmp);
		}
		NumValid++;
	}

	FString OutputPath;
	if (UseFilePath != nullptr)
	{
		OutputPath = *UseFilePath;
	}
	else
	{
		FString FileName(Settings->BaseName);
		if (FileName.IsEmpty())
			FileName = TEXT("CombinedMesh");
		OutputPath = FPaths::Combine(OutputBaseFolder, FileName + GetFormatExtension());
	}

	EGSExportMeshesToolTopologyMode TopologyMode = EGSExportMeshesToolTopologyMode::Triangles;
	if (Settings->Format == EGSExportMeshesToolFormat::OBJ)
		TopologyMode = OBJSettings->Faces;

	ApplyMeshTransforms(CombinedMesh,
		Settings->HandedTransform == EExportMeshHandedMode::LHStoRHS,
		Settings->UpTransform == EExportMeshToolUpMode::ZUpToYUp,
		Settings->Units, Settings->Scale);

	bool bWriteOK = false;

	// TODO: polymesh construction is perhaps not the most efficient for large meshes...
	//bool bWritePolyMesh = (TopologyMode == EGSExportMeshesToolTopologyMode::GroupsAsPolygons) ||
	//	(CombinedMesh.TriangleCount() < 50000);
	bool bWritePolyMesh = true;
	if (bWritePolyMesh)
	{
		GS::PolyMesh PolyMesh;
		ConvertMeshToPolyMesh(CombinedMesh, TopologyMode, 0, PolyMesh, Errors);
		bWriteOK = WritePolyMeshToPath(PolyMesh, OutputPath, Errors);
	}
	else {
		GS::DenseMesh DenseMesh;
		GS::ConvertDynamicMeshToDenseMesh(CombinedMesh, DenseMesh);
		bWriteOK = WriteDenseMeshToPath(DenseMesh, OutputPath, Errors);
	}

	if (!bWriteOK)
	{
		Errors.AppendError(0, EGSErrorLevel::Error, TCHAR_TO_UTF8(*FString::Printf(TEXT("Failed to write Mesh to Path %s"), *OutputPath)));
	}
	else
	{
		GetToolManager()->DisplayMessage(
			FText::Format(LOCTEXT("CombinedFileExportMessage", "Exported {0} meshes to {0}"), NumValid, FText::FromString(OutputPath)),
			EToolMessageLevel::UserNotification);
	}
}


void UGSExportMeshesTool::ExecuteExport_MultiFile(const FString& OutputBaseFolder, GS::GSErrorSet& Errors)
{
	TArray<FString> FilePaths;
	FilePaths.SetNum(Targets.Num());
	for (int32 k = 0; k < Targets.Num(); ++k)
	{
		AActor* TargetActor = UE::ToolTarget::GetTargetActor(Targets[k]);
		UPrimitiveComponent* TargetComponent = UE::ToolTarget::GetTargetComponent(Targets[k]);
		if (TargetActor == nullptr || TargetComponent == nullptr)
			continue;

		FString FileName(Settings->BaseName);
		if (Settings->bNameFromActor)
		{
			FileName = (Settings->bAddComponentName) ?
				FString::Printf(TEXT("%s_%s"), *TargetActor->GetActorNameOrLabel(), *TargetComponent->GetName())
				: FString::Printf(TEXT("%s"), *TargetActor->GetActorNameOrLabel());
		}
		if (FileName.IsEmpty())
			FileName = TEXT("Mesh");
		FString OutputPath = FPaths::Combine(OutputBaseFolder, FileName + GetFormatExtension());

		if (FilePaths.Contains(OutputPath))
		{
			// todo: could check file existence here too
			int Counter = 0;
			while (FilePaths.Contains(OutputPath))
			{
				Counter++;
				FString UniqueSuffix = FString::Printf(TEXT("_%d%s"), Counter, *GetFormatExtension());
				OutputPath = FPaths::Combine(OutputBaseFolder, FileName + UniqueSuffix);
			}
		}
		FilePaths[k] = OutputPath;
	}


	int NumValid = 0;
	for (int32 k = 0; k < Targets.Num(); ++k)
	{
		AActor* TargetActor = UE::ToolTarget::GetTargetActor(Targets[k]);
		UPrimitiveComponent* TargetComponent = UE::ToolTarget::GetTargetComponent(Targets[k]);
		if (TargetActor == nullptr || TargetComponent == nullptr)
			continue;

		FString OutputPath = FilePaths[k];

		FTransform SourceTransform = (FTransform)UE::ToolTarget::GetLocalToWorldTransform(Targets[k]);
		
#if UE_VERSION_OLDER_THAN(5,5,0)
		FDynamicMesh3 SourceMesh = UE::ToolTarget::GetDynamicMeshCopy(Targets[k], true);
#else
		FGetMeshParameters Params;
		Params.bWantMeshTangents = false;
		FDynamicMesh3 SourceMesh = UE::ToolTarget::GetDynamicMeshCopy(Targets[k], Params);
#endif

		if (Settings->bWorldSpace)
		{
			MeshTransforms::ApplyTransform(SourceMesh, (FTransformSRT3d)SourceTransform, true);
		}

		ApplyMeshTransforms(SourceMesh,
			Settings->HandedTransform == EExportMeshHandedMode::LHStoRHS,
			Settings->UpTransform == EExportMeshToolUpMode::ZUpToYUp,
			Settings->Units, Settings->Scale);

		EGSExportMeshesToolTopologyMode TopologyMode = EGSExportMeshesToolTopologyMode::Triangles;
		if (Settings->Format == EGSExportMeshesToolFormat::OBJ)
			TopologyMode = OBJSettings->Faces;

		GS::PolyMesh PolyMesh;
		ConvertMeshToPolyMesh(SourceMesh, TopologyMode, 0, PolyMesh, Errors);

		if (!WritePolyMeshToPath(PolyMesh, OutputPath, Errors)) {
			Errors.AppendError(0, EGSErrorLevel::Error, TCHAR_TO_UTF8(*FString::Printf(TEXT("Failed to write Actor %s Component %s to Path %s"),
					*TargetActor->GetActorNameOrLabel(), *TargetComponent->GetName(), *OutputPath)));
		}
		else
			NumValid++;
	}

	if (NumValid > 0) {
		if (Targets.Num() == 1)
		{
			GetToolManager()->DisplayMessage(
				FText::Format(LOCTEXT("SingleFileExportMessage", "Exported mesh to {0}"), FText::FromString(FilePaths[0])),
				EToolMessageLevel::UserNotification);
		}
		else
		{
			GetToolManager()->DisplayMessage(
				FText::Format(LOCTEXT("MultiFileExportMessage", "Exported {0} meshes"), NumValid),
				EToolMessageLevel::UserNotification);
		}
	}
}




bool UGSExportMeshesTool::WritePolyMeshToPath(GS::PolyMesh& Mesh, const FString& FilePath, GS::GSErrorSet& Errors)
{
	bool bWriteOK = false;
	switch (Settings->Format)
	{
	case EGSExportMeshesToolFormat::STL:
		bWriteOK = WriteMeshToPath_STL(Mesh, FilePath, Errors); break;
	case EGSExportMeshesToolFormat::OBJ:
	default:
		bWriteOK = WriteMeshToPath_OBJ(Mesh, FilePath, Errors); break;
	}
	return bWriteOK;
}

bool UGSExportMeshesTool::WriteMeshToPath_STL(GS::PolyMesh& Mesh, const FString& FilePath, GS::GSErrorSet & Errors)
{
	Errors.AppendError("STL format not yet supported");
	return false;
}

bool UGSExportMeshesTool::WriteMeshToPath_OBJ(GS::PolyMesh& Mesh, const FString& FilePath, GS::GSErrorSet& Errors)
{
	GS::OBJFormatData OBJData;
	GS::PolyMeshToOBJFormatData(Mesh, OBJData);

	GS::OBJWriter::WriteOptions OBJOptions;
	OBJOptions.bVertexColors = OBJSettings->bVertexColors;
	OBJOptions.bNormals = OBJSettings->bNormals;
	OBJOptions.bUVs = OBJSettings->bUVs;

	std::string StdStringPath(TCHAR_TO_UTF8(*FilePath));
	GS::FileTextWriter Writer = GS::FileTextWriter::OpenFile(StdStringPath);
	if (!Writer)
		return false;
	bool bWriteOK = GS::OBJWriter::WriteOBJ(Writer, OBJData, OBJOptions);
	Writer.CloseFile();
	return bWriteOK;
}




bool UGSExportMeshesTool::WriteDenseMeshToPath(GS::DenseMesh& Mesh, const FString& FilePath, GS::GSErrorSet& Errors)
{
	bool bWriteOK = false;
	switch (Settings->Format)
	{
	case EGSExportMeshesToolFormat::STL:
		bWriteOK = WriteMeshToPath_STL(Mesh, FilePath, Errors); break;
	case EGSExportMeshesToolFormat::OBJ:
	default:
		bWriteOK = WriteMeshToPath_OBJ(Mesh, FilePath, Errors); break;
	}
	return bWriteOK;
}

bool UGSExportMeshesTool::WriteMeshToPath_STL(GS::DenseMesh& Mesh, const FString& FilePath, GS::GSErrorSet& Errors)
{
	Errors.AppendError("STL format not yet supported");
	return false;
}

bool UGSExportMeshesTool::WriteMeshToPath_OBJ(GS::DenseMesh& Mesh, const FString& FilePath, GS::GSErrorSet& Errors)
{
	GS::OBJFormatData OBJData;
	GS::DenseMeshToOBJFormatData(Mesh, OBJData);

	GS::OBJWriter::WriteOptions OBJOptions;
	OBJOptions.bVertexColors = OBJSettings->bVertexColors;
	OBJOptions.bNormals = OBJSettings->bNormals;
	OBJOptions.bUVs = OBJSettings->bUVs;

	std::string StdStringPath(TCHAR_TO_UTF8(*FilePath));
	GS::FileTextWriter Writer = GS::FileTextWriter::OpenFile(StdStringPath);
	if (!Writer)
		return false;
	bool bWriteOK = GS::OBJWriter::WriteOBJ(Writer, OBJData, OBJOptions);
	Writer.CloseFile();
	return bWriteOK;
}



const FString& UGSExportMeshesTool::GetFormatExtension() const
{
	static const FString Extension_STL(TEXT(".stl"));
	static const FString Extension_OBJ(TEXT(".obj"));

	switch (Settings->Format)
	{
	case EGSExportMeshesToolFormat::STL: return Extension_STL;
	default:
	case EGSExportMeshesToolFormat::OBJ: return Extension_OBJ;
	}
}

const FString& UGSExportMeshesTool::GetFormatFileFilter() const
{
	static const FString Filter_STL(TEXT("STL File (*.stl)|*.stl"));
	static const FString Filter_OBJ(TEXT("OBJ File (*.obj)|*.obj"));

	switch (Settings->Format)
	{
	case EGSExportMeshesToolFormat::STL: return Filter_STL;
	default:
	case EGSExportMeshesToolFormat::OBJ: return Filter_OBJ;
	}
}



#undef LOCTEXT_NAMESPACE
