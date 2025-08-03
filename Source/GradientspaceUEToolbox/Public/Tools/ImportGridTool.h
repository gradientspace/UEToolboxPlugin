// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolQueryInterfaces.h"
#include "DynamicMesh/MeshSharingUtil.h"
#include "FrameTypes.h"
#include "Utility/CameraUtils.h"

#include "ImportGridTool.generated.h"

class UWorld;
class AGSModelGridActor;
class UPreviewMesh;
class UMeshOpPreviewWithBackgroundCompute;
class FGridImportPreviewOpFactory;
class UMaterialInstanceDynamic;
class UMaterialInterface;


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridImportToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};


UENUM()
enum class EImportModelGridOrigin : uint8
{
	FromFile,
	Center,
	BaseZ,
};


UENUM()
enum class EImportModelGridImportType : uint8
{
	MagicaVoxFormat,
	ImageFormat
};

UENUM()
enum class EImportModelGridImagePlane : uint8
{
	XYPlane,
	XZPlane,
	YZPlane
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridImportTool_ImportFunctions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UFUNCTION(CallInEditor, Category = "Select File", meta = (DisplayPriority = 1))
	void SelectMagicaVOX();

	UFUNCTION(CallInEditor, Category = "Select File", meta = (DisplayPriority = 2))
	void SelectImage();

	UPROPERTY(VisibleAnywhere, Category="Select File", meta=(TransientToolProperty, NoResetToDefault))
	FString CurFileName;

	UPROPERTY(VisibleAnywhere, Category = "Select File", meta = (TransientToolProperty, NoResetToDefault))
	FString CellCounts;

	//UPROPERTY(EditAnywhere, Category = "Grid Settings")
	//EImportModelGridOrigin Origin = EImportModelGridOrigin::BaseZ;

	/** Allow separate dimensions along the grid X/Y/Z dimensions */
	UPROPERTY(EditAnywhere, Category = "Grid Settings")
	bool bNonUniform = false;

	/** Fixed world-space grid cell dimension */
	UPROPERTY(EditAnywhere, Category = "Grid Settings", meta = (UIMin = 5, UIMax = 250, ClampMin = 0.1, ClampMax = 10000, EditCondition = "bNonUniform==false", EditConditionHides))
	double Dimension = 50.0;

	/** Fixed world-space grid cell X dimension */
	UPROPERTY(EditAnywhere, Category = "Grid Settings", meta = (UIMin = 5, UIMax = 250, ClampMin = 0.1, ClampMax = 10000, EditCondition = "bNonUniform==true", EditConditionHides))
	double DimensionX = 50.0;
	/** Fixed world-space grid cell Y dimension */
	UPROPERTY(EditAnywhere, Category = "Grid Settings", meta = (UIMin = 5, UIMax = 250, ClampMin = 0.1, ClampMax = 10000, EditCondition = "bNonUniform==true", EditConditionHides))
	double DimensionY = 50.0;
	/** Fixed world-space grid cell Z dimension */
	UPROPERTY(EditAnywhere, Category = "Grid Settings", meta = (UIMin = 5, UIMax = 250, ClampMin = 0.1, ClampMax = 10000, EditCondition = "bNonUniform==true", EditConditionHides))
	double DimensionZ = 50.0;


	/**
	 * Automatically position the grid relative to the current View.
	 * Automatic placement will be locked if the position is manually set using the 3D gizmo.
	 **/
	//UPROPERTY(EditAnywhere, Category = "Placement", meta = (EditCondition = "!bPlacementIsLocked", HideEditConditionToggle))
	UPROPERTY(meta = (TransientToolProperty))
	bool bAutoPosition = false;

	UPROPERTY(meta = (TransientToolProperty))
	bool bPlacementIsLocked = false;


	TWeakObjectPtr<UModelGridImportTool> Tool;
};


UENUM()
enum class EVoxObjectPivotLocation
{
	/** object grids are placed relative to their object origins (ie all transforms in the scene hierarchy are ignored) */
	ObjectOrigin = 0,
	/** All object grids are placed relative to a the shared file-relative origin cell (0,0,0) */
	FileSceneOrigin = 1,
};

UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridImportTool_VoxOptions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "VOX Options")
	bool bIgnoreColors = false;

	/** 
	 * Merge all the Objects in the input file into a single combined grid.
	 * If false, multiple Assets/Actors will be generated on Accept.
	 */
	UPROPERTY(EditAnywhere, Category = "VOX Options")
	bool bCombineAllObjects = true;

	/** 
	 * Grid Origins of each output Object (only applied on Accept) 
	 * Using object-specific origins can avoid clipping of objects in a large grid.
	 */
	UPROPERTY(EditAnywhere, Category = "VOX Options", meta=( EditCondition="bCombineAllObjects == false", HideEditConditionToggle))
	EVoxObjectPivotLocation ObjectOrigins = EVoxObjectPivotLocation::ObjectOrigin;

	/** Magica Voxel coordinate system +X direction is the opposite of UE's */
	UPROPERTY(EditAnywhere, Category = "VOX Options")
	bool bFlipX = true;
};



UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridImportTool_ImageOptions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Set which plane the image will be imported along */
	UPROPERTY(EditAnywhere, Category = "Image Options")
	EImportModelGridImagePlane ImagePlane = EImportModelGridImagePlane::XYPlane;

	/** Flip the image along the X (left/right) axis */
	UPROPERTY(EditAnywhere, Category = "Image Options")
	bool bFlipX = false;

	/** Flip the image along the Y (up/down) axis */
	UPROPERTY(EditAnywhere, Category = "Image Options")
	bool bFlipY = false;

	/** Rotate the image by 90 degrees. Combine with FlipX/FlipY to construct other rotations. */
	UPROPERTY(EditAnywhere, Category = "Image Options")
	bool bRotate90 = false;

	/** Crop the image to the bounding box containing non-zero alpha values */
	UPROPERTY(EditAnywhere, Category = "Image Options")
	bool bAlphaCrop = true;

	/** Apply SRGB conversion when converting imported image color to grid color */
	UPROPERTY(EditAnywhere, Category = "Image Options")
	bool bSRGB = true;
};




UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridImportTool_OutputOptions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	/** Name of created Actor (and Asset, if desired) */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (TransientToolProperty))
	FString AssetName = TEXT("ModelGrid");

	/**
	 * If true, new ModelGrid Asset will be created and assigned to the new ModelGrid Actor.
	 * Otherwise, the created ModelGrid will be stored inside the new Actor's GridComponent
	 */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bCreateAsset = false;
};




UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridImportTool 
	: public UInteractiveTool, public IInteractiveToolCameraFocusAPI
{
	GENERATED_BODY()
public:

	UModelGridImportTool();

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

	void SetExistingActor(AGSModelGridActor* Actor);

	virtual void SelectFileToImport(EImportModelGridImportType ImportFormat);


	// IInteractiveToolCameraFocusAPI implementation
	virtual bool SupportsWorldSpaceFocusBox() override;
	virtual FBox GetWorldSpaceFocusBox() override;
	virtual bool SupportsWorldSpaceFocusPoint() override;
	virtual bool GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut) override;

protected:
	UPROPERTY()
	TObjectPtr<UModelGridImportTool_ImportFunctions> ImportFunctions;

	UPROPERTY()
	TObjectPtr<UModelGridImportTool_VoxOptions> VoxFormatOptions;
	UPROPERTY()
	TObjectPtr<UModelGridImportTool_ImageOptions> ImageFormatOptions;

	UPROPERTY()
	TObjectPtr<UModelGridImportTool_OutputOptions> OutputOptions;

protected:

	UPROPERTY()
	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	UPROPERTY()
	TWeakObjectPtr<AGSModelGridActor> ExistingActor = nullptr;


	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> EditCompute = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ActiveMaterial = nullptr;

	TArray<TObjectPtr<UMaterialInterface>> ActiveMaterials;

	UE::Geometry::FFrame3d GridFrame;

	TPimplPtr<FGridImportPreviewOpFactory> OperatorFactory;
	friend class FGridImportPreviewOpFactory;

	FString CurrentImportFilePath;
	EImportModelGridImportType ImportType;
	bool UpdateImport();
	bool UpdateImport_MagicaVox();
	bool UpdateImport_Image();

	virtual void OnGridCellDimensionsModified();
	FVector3d GetCellDimensions() const;


	FViewCameraState ViewState;
	GS::FViewProjectionInfo LastViewInfo;
	bool bHaveViewState = false;

	bool bAutoPlacementPending = false;
	bool bAutoPlacementLocked = false;
	void UpdateAutoPlacement();
};


