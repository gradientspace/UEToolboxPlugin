// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolQueryInterfaces.h"
#include "PropertySets/CreateMeshObjectTypeProperties.h"
#include "DynamicMesh/MeshSharingUtil.h"
#include "Drawing/MeshElementsVisualizer.h"
#include "FrameTypes.h"
#include "Utility/CameraUtils.h"

#include "ImportMeshTool.generated.h"

class UWorld;
class UPreviewMesh;
class UMeshOpPreviewWithBackgroundCompute;
class FImportMeshOpFactory;
class UMaterialInstanceDynamic;
class UMaterialInterface;


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UImportMeshToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};


UENUM()
enum class EImportMeshToolFormatType : uint8
{
	OBJFormat,
	STLFormat
};

UENUM()
enum class EImportMeshToolGroupLayerType : uint8
{
	/** Derive Polygroups from grouping information detected in the imported file */
	FromFile = 0,
	/** Set a new Polygroup for each input Polygon */
	PerPolygon = 1,
	/** Set a new Polygroup for each Connected Component */
	ConnectedComponents = 2,
	/** Set a new Polygroup for each UV Island */
	UVIslands = 3
};

UENUM()
enum class EImportMeshToolVisualizationMode : uint8
{
	Shaded,
	VertexColors,
	Polygroups
};

UENUM()
enum class EImportMeshToolSourceUnits : uint8
{
	Centimeters = 0,
	Meters = 1,
	Millimeters = 2,
	Inches = 3,
	Feet = 4
};



UENUM()
enum class EImportMeshToolPivotMode : uint8
{
	/** Keep the pivot location from the source file */
	FromFile = 0,
	/** Transform the mesh so that the pivot location is at the bounding-box center */
	Center = 1,
	/** Transform the mesh so that the pivot location is at the bounding-box center-base */
	Base = 2
};


UENUM()
enum class EImportMeshToolUpMode : uint8
{
	/** do not apply any up-axis transform, assume the source file is Z-up */
	Unmodified = 0,
	/** Assume the source file is Y-up and convert to Z-up */
	YUpToZUp = 1
};


UENUM()
enum class EImportMeshHandedMode : uint8
{
	/** Do not apply any handedness-transform, assume the source file is Left-Handed (LHS) */
	Unmodified = 0,
	/** Assume the source file is Right-handed (RHS) and convert to Unreal Left-Handed (LHS) */
	RHStoLHS = 1
};




UCLASS()
class GRADIENTSPACEUETOOLCORE_API UImportMeshTool_ImportFunctions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UFUNCTION(CallInEditor, Category = "Select File", meta = (DisplayPriority = 1))
	void SelectMesh();

	UPROPERTY(VisibleAnywhere, Category="Select File", meta=(TransientToolProperty, NoResetToDefault))
	FString CurFileName;

	TWeakObjectPtr<UImportMeshTool> Tool;
};



UCLASS()
class GRADIENTSPACEUETOOLCORE_API UImportMeshTool_OBJOptions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Discard per-vertex colors in the OBJ file, if they are available */
	UPROPERTY(EditAnywhere, Category = "OBJ Options")
	bool bIgnoreColors = false;

	/** How should the Polygroups on the mesh asset be determined */
	UPROPERTY(EditAnywhere, Category = "OBJ Options")
	EImportMeshToolGroupLayerType GroupMode = EImportMeshToolGroupLayerType::FromFile;
};


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UImportMeshTool_TransformOptions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Up-Axis Transform applied to the imported file (Unreal is Z-up) */
	UPROPERTY(EditAnywhere, Category = "Transform", meta=(DisplayName="Up Axis"))
	EImportMeshToolUpMode UpTransform = EImportMeshToolUpMode::Unmodified;

	/** Handedness Transform applied to the imported file (Unreal is Left-Handed, most other 3D software is Right-Handed) */
	UPROPERTY(EditAnywhere, Category = "Transform", meta=(DisplayName="Handedness"))
	EImportMeshHandedMode HandedTransform = EImportMeshHandedMode::RHStoLHS;

	/** 
	 * Invert the orientation of the imported mesh (vertex order in each triangle, and direction of normals).
	 * This will be necessary if external file has Right-Handed orientation.
	 * However, the Handedness setting will invert the interpretaion of this flag
	 */
	UPROPERTY(EditAnywhere, Category = "Transform")
	bool bInvert = true;

	/** Units of source file, will be converted to UE units (centimeters) */
	UPROPERTY(EditAnywhere, Category = "Transform")
	EImportMeshToolSourceUnits SourceUnits = EImportMeshToolSourceUnits::Centimeters;

	/** Scale applied to the imported mesh */
	UPROPERTY(EditAnywhere, Category = "Transform")
	double Scale = 1.0;

	/** Set the Pivot Location of the imported mesh */
	UPROPERTY(EditAnywhere, Category = "Transform")
	EImportMeshToolPivotMode PivotLocation = EImportMeshToolPivotMode::FromFile;

	/**
	 * Automatically position the grid relative to the current View.
	 * Automatic placement will be locked if the position is manually set using the 3D gizmo.
	 **/
	//UPROPERTY(EditAnywhere, Category = "Placement", meta = (EditCondition = "!bPlacementIsLocked", HideEditConditionToggle))
	UPROPERTY(meta = (TransientToolProperty))
	bool bAutoPosition = false;

	UPROPERTY(meta = (TransientToolProperty))
	bool bPlacementIsLocked = false;
};

UCLASS()
class GRADIENTSPACEUETOOLCORE_API UImportMeshTool_VisualizationOptions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Visualization")
	EImportMeshToolVisualizationMode VisualizationMode = EImportMeshToolVisualizationMode::Shaded;

	UPROPERTY(EditAnywhere, Category = "Visualization")
	bool bShowWireframe = false;

	UPROPERTY(EditAnywhere, Category = "Visualization")
	bool bShowBorders = false;

	UPROPERTY(EditAnywhere, Category = "Visualization")
	bool bShowUVSeams = false;

	UPROPERTY(EditAnywhere, Category = "Visualization")
	bool bShowNormalSeams = false;
};



UCLASS()
class GRADIENTSPACEUETOOLCORE_API UImportMeshTool_CreateMeshProperties : public UCreateMeshObjectTypeProperties
{
	GENERATED_BODY()
public:

	/** Name of created Actor (and Asset, if an Asset will be created) */
	UPROPERTY(EditAnywhere, Category = OutputType, meta = (TransientToolProperty))
	FString ActorName = TEXT("Mesh");

	/** Name of created Asset */
	//UPROPERTY(EditAnywhere, Category = OutputType, meta = (TransientToolProperty, EditCondition = "bShowAssetName == true", EditConditionHides, HideEditConditionToggle))
	UPROPERTY()
	FString AssetName = TEXT("SM_Mesh");
	// above doesn't work currently because we cannot provide a separate asset name to the ModelingObjectsAPI...

	UPROPERTY()
	bool bShowAssetName = false;

	virtual void UpdatePropertyVisibility()
	{
		UCreateMeshObjectTypeProperties::UpdatePropertyVisibility();
		bShowAssetName = (GetCurrentCreateMeshType() == ECreateObjectTypeHint::StaticMesh);
	}
};


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UMeshElementsVisualizerExt : public UMeshElementsVisualizer
{
	GENERATED_BODY()
public:
	void ForceSettingsModified() { bSettingsModified = true; }
};


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UImportMeshTool 
	: public UInteractiveTool, public IInteractiveToolCameraFocusAPI
{
	GENERATED_BODY()
public:

	UImportMeshTool();

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;


	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

public:
	void SetTargetWorld(UWorld* World);
	UWorld* GetTargetWorld();

	//void SetExistingActor(AGSModelGridActor* Actor);

	virtual void SelectFileToImport(FString UseSpecificFile = TEXT(""));

	// IInteractiveToolCameraFocusAPI implementation
	virtual bool SupportsWorldSpaceFocusBox() override;
	virtual FBox GetWorldSpaceFocusBox() override;
	virtual bool SupportsWorldSpaceFocusPoint() override;
	virtual bool GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut) override;

	virtual void SetStartupImportFilePath(FString Path) { StartupImportFilePath = Path; }
	virtual void SetEnableAutoPlacementOnLoad(bool bEnable) { bEnableAutoPlacementOnLoad = bEnable; }

protected:

	// move this back after separating scale hack
	UPROPERTY()
	TObjectPtr<UImportMeshTool_ImportFunctions> ImportFunctions;

	/** Property set for type of output object (StaticMesh, Volume, etc) */
	UPROPERTY()
	TObjectPtr<UImportMeshTool_CreateMeshProperties> OutputTypeProperties;

public:

	UPROPERTY()
	TObjectPtr<UImportMeshTool_TransformOptions> TransformOptions;

	UPROPERTY()
	TObjectPtr<UImportMeshTool_OBJOptions> OBJFormatOptions;


protected:

	UPROPERTY()
	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	//UPROPERTY()
	//TWeakObjectPtr<AGSModelGridActor> ExistingActor = nullptr;

	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> EditCompute = nullptr;

	UPROPERTY()
	TObjectPtr<UMeshElementsVisualizerExt> MeshElementsDisplay;

	UPROPERTY()
	TObjectPtr<UImportMeshTool_VisualizationOptions> VisualizationOptions;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ActiveMaterial = nullptr;

	TArray<TObjectPtr<UMaterialInterface>> ActiveMaterials;

	EImportMeshToolVisualizationMode CurVisualizationMode = EImportMeshToolVisualizationMode::Shaded;
	virtual void UpdateVisualizationMode();

	UE::Geometry::FFrame3d GridFrame;

	UE::Geometry::FAxisAlignedBox3d CurrentLocalBounds;

	TPimplPtr<FImportMeshOpFactory> OperatorFactory;
	friend class FImportMeshOpFactory;

	FString StartupImportFilePath;
	FString CurrentImportFilePath;
	EImportMeshToolFormatType ImportType;
	bool UpdateImport();
	bool UpdateImport_OBJ();


	FViewCameraState ViewState;
	GS::FViewProjectionInfo LastViewInfo;
	bool bHaveViewState = false;

	bool bEnableAutoPlacementOnLoad = true;
	bool bAutoPlacementPending = false;
	bool bAutoPlacementLocked = false;
	void UpdateAutoPlacement();

};


