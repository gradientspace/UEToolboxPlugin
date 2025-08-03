// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolQueryInterfaces.h" // IInteractiveToolExclusiveToolAPI
#include "PropertyTypes/ActionButtonGroup.h"

#include "ExportMeshTool.generated.h"

namespace GS { class DenseMesh; }
namespace GS { class PolyMesh; }
namespace GS { class GSErrorSet; }


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UGSExportMeshesToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};

UENUM()
enum class EGSExportMeshesToolFormat : uint8
{
	OBJ,
	STL
};


UENUM()
enum class EExportMeshToolUpMode : uint8
{
	/** do not apply any up-axis transform, export the file Z-up */
	Unmodified = 0,
	/** convert from Unreal Z-up to Y-up before exporting */
	ZUpToYUp = 1
};


UENUM()
enum class EExportMeshHandedMode : uint8
{
	/** Do not apply any handedness-transform, export the file as Left-handed (LHS) */
	Unmodified = 0,
	/** Convert from Unreal Left-Handed (LHS) to Right-handed (RHS) before exporting */
	LHStoRHS = 1
};

UENUM()
enum class EExportMeshDestUnits : uint8
{
	Centimeters = 0,
	Meters = 1,
	Millimeters = 2,
	Inches = 3,
	Feet = 4
};


USTRUCT()
struct GRADIENTSPACEUETOOLCORE_API FGSFolderProperty
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Path)
	FString Path;

	FGSFolderProperty() {}
	FGSFolderProperty(FString DefaultPath) : Path(DefaultPath) {}
};


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UGSExportMeshesToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (DisplayName = "Mesh File Format"))
	EGSExportMeshesToolFormat Format = EGSExportMeshesToolFormat::OBJ;

	/** Append all the input meshes into a single mesh and write a single file */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (EditCondition = "NumTargetObjects>1", EditConditionHides))
	bool bCombineInputs = false;

	/** Transform the meshes into World Space before exporting */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (EditCondition = "NumTargetObjects==1 || bCombineInputs == false"))
	bool bWorldSpace = false;

	/** Up-Axis Transform applied before exporting (Unreal is Z-up) */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DisplayName = "Up Axis"))
	EExportMeshToolUpMode UpTransform = EExportMeshToolUpMode::Unmodified;

	/** Handedness Transform applied before exporting (Unreal is Left-Handed, most other 3D software is Right-Handed) */
	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DisplayName = "Handedness"))
	EExportMeshHandedMode HandedTransform = EExportMeshHandedMode::LHStoRHS;

	/** Unreal units (centimeters) will be converted to these Units in the desination file */
	UPROPERTY(EditAnywhere, Category = "Transform")
	EExportMeshDestUnits Units = EExportMeshDestUnits::Centimeters;

	/** Scale applied to the exported mesh */
	UPROPERTY(EditAnywhere, Category = "Transform")
	double Scale = 1.0;

	UPROPERTY(EditAnywhere, Category = "Transform", meta=(TransientToolProperty))
	FGSActionButtonGroup TransformPresets;

	/** Derive each exported mesh filename from the Actor name */
	UPROPERTY(EditAnywhere, Category = "File Naming", meta = (EditCondition = "NumTargetObjects==1 || bCombineInputs==false"))
	bool bNameFromActor = true;

	/** Include the Component name in the exported mesh filename */
	UPROPERTY(EditAnywhere, Category = "File Naming", meta = (EditCondition = "NumTargetObjects==1 || bCombineInputs==false"))
	bool bAddComponentName = false;

	/** Use this string as the base filename for the exported mesh */
	UPROPERTY(EditAnywhere, Category = "File Naming", meta = (EditCondition = "bNameFromActor == false || bCombineInputs == true", EditConditionHides))
	FString BaseName = TEXT("CombinedMesh");


	// used for edit conditions
	UPROPERTY()
	int NumTargetObjects = 1;

	/** When enabled, output folder is selected using popup dialog on Export */
	UPROPERTY(EditAnywhere, Category = "Output Folder", meta = (DisplayName = "Select on Export"))
	bool bSelectFolderOnExport = true;

	/** Output Folder Path */
	UPROPERTY(EditAnywhere, Category = "Output Folder", meta=(DisplayName="Folder Path", NoResetToDefault, EditCondition = "bSelectFolderOnExport==false", EditConditionHides))
	FGSFolderProperty PathStruct = FGSFolderProperty(TEXT("C:\\"));
};



UENUM()
enum class EGSExportMeshesToolTopologyMode : uint8
{
	/** Export the mesh triangles */
	Triangles,
	/** Export PolyGroups as Polygons. UVs, Colors, and Normals may be lost. */
	GroupsAsPolygons
};



UCLASS()
class GRADIENTSPACEUETOOLCORE_API UGSExportMeshesSettings_OBJFormat : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** How to convert the source mesh to OBJ faces */
	UPROPERTY(EditAnywhere, Category = "OBJ Format Options")
	EGSExportMeshesToolTopologyMode Faces = EGSExportMeshesToolTopologyMode::Triangles;

	/** Include per-vertex colors. Split vertex colors will be uniformly averaged. */
	UPROPERTY(EditAnywhere, Category = "OBJ Format Options")
	bool bVertexColors = true;

	/** Include Split normals */
	UPROPERTY(EditAnywhere, Category = "OBJ Format Options")
	bool bNormals = true;

	/** Include UV Channel 0 */
	UPROPERTY(EditAnywhere, Category = "OBJ Format Options")
	bool bUVs = true;
};



UCLASS()
class GRADIENTSPACEUETOOLCORE_API UGSExportMeshesToolActions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UGSExportMeshesTool> ParentTool;

	///** Export the selected mesh objects using the current Options above. Errors/Warnings will be printed to the Output Log. */
	//UFUNCTION(CallInEditor, Category = "Execute File Export", meta=(DisplayName="Export!"))
	//void ClickToExport();

	UPROPERTY(EditAnywhere, Category = "Execute")
	FGSActionButtonGroup ExportButtons;

};


UENUM()
enum class EExportPreset
{
	Maya,		// RHS YUp
	Max,		// RHS Zup
	ZBrush		// LHS Yup
};


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UGSExportMeshesTool 
	: public UMultiSelectionMeshEditingTool, public IInteractiveToolExclusiveToolAPI
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override { return false; }
	virtual bool CanAccept() const override { return false; }

	virtual void ExecuteExport_Folder();
	virtual void ExecuteExport_File();

	virtual void ConfigurePreset(EExportPreset Preset);

public:
	UPROPERTY()
	TObjectPtr<UGSExportMeshesToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UGSExportMeshesSettings_OBJFormat> OBJSettings;

protected:

	void UpdateVisibleSettingsForFormat();
	

	UPROPERTY()
	TObjectPtr<UGSExportMeshesToolActions> ToolActions;


	virtual void ExecuteExport_Combined(const FString& OutputBaseFolder, FString* UseFilePath, GS::GSErrorSet& Errors);
	virtual void ExecuteExport_MultiFile(const FString& OutputBaseFolder, GS::GSErrorSet& Errors);
	const FString& GetFormatExtension() const;
	const FString& GetFormatFileFilter() const;


	bool WritePolyMeshToPath(GS::PolyMesh& Mesh, const FString& FilePath, GS::GSErrorSet& Errors);
	bool WriteMeshToPath_OBJ(GS::PolyMesh& Mesh, const FString& FilePath, GS::GSErrorSet& Errors);
	bool WriteMeshToPath_STL(GS::PolyMesh& Mesh, const FString& FilePath, GS::GSErrorSet& Errors);

	bool WriteDenseMeshToPath(GS::DenseMesh& Mesh, const FString& FilePath, GS::GSErrorSet& Errors);
	bool WriteMeshToPath_OBJ(GS::DenseMesh& Mesh, const FString& FilePath, GS::GSErrorSet& Errors);
	bool WriteMeshToPath_STL(GS::DenseMesh& Mesh, const FString& FilePath, GS::GSErrorSet& Errors);


	TSharedPtr<FGSActionButtonTarget> ExportButtonsTarget;
	TSharedPtr<FGSActionButtonTarget> PresetButtonsTarget;
	virtual void OnExportButtonClicked(FString CommandName);
	virtual void OnPresetButtonClicked(FString CommandName);

};
