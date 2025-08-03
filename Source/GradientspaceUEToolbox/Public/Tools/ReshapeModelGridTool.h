// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolQueryInterfaces.h"
#include "FrameTypes.h"
#include "PropertyTypes/ActionButtonGroup.h"
#include "PropertySets/AxisFilterPropertyType.h"

#include "ReshapeModelGridTool.generated.h"

class UWorld;
class AGSModelGridActor;
class UPreviewMesh;
class UModelGrid;
class FGridPreviewOpFactory;
class UMaterialInstanceDynamic;
class UMaterialInterface;
struct FReshapeModelGridToolInternalData;
class UConstructionPlaneMechanic;
class UDragAlignmentMechanic;


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UReshapeModelGridToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UReshapeModelGridSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	/** Allow the grid cell dimensions to be non-uniform */
	UPROPERTY(EditAnywhere, Category = "Grid Cell Size", meta=(NoResetToDefault) )
	bool bNonUniform = false;

	/** Fixed world-space grid cell dimension */
	UPROPERTY(EditAnywhere, Category = "Grid Cell Size", meta = (NoResetToDefault, UIMin = 5, UIMax = 250, ClampMin = 0.1, ClampMax = 10000, EditCondition = "bNonUniform==false", EditConditionHides))
	double Dimension = 50.0;

	/** Fixed world-space grid cell X dimension */
	UPROPERTY(EditAnywhere, Category = "Grid Cell Size", meta = (NoResetToDefault, UIMin = 5, UIMax = 250, ClampMin = 0.1, ClampMax = 10000, EditCondition = "bNonUniform==true", EditConditionHides))
	double DimensionX = 50.0;
	/** Fixed world-space grid cell Y dimension */
	UPROPERTY(EditAnywhere, Category = "Grid Cell Size", meta = (NoResetToDefault, UIMin = 5, UIMax = 250, ClampMin = 0.1, ClampMax = 10000, EditCondition = "bNonUniform==true", EditConditionHides))
	double DimensionY = 50.0;
	/** Fixed world-space grid cell Z dimension */
	UPROPERTY(EditAnywhere, Category = "Grid Cell Size", meta = (NoResetToDefault, UIMin = 5, UIMax = 250, ClampMin = 0.1, ClampMax = 10000, EditCondition = "bNonUniform==true", EditConditionHides))
	double DimensionZ = 50.0;


	/** Number of cells to crop from the Min/Max sides of the grid in the X dimension (negative values ignored) */
	UPROPERTY(EditAnywhere, Category = "Grid Cropping")
	FIntVector2 CropRangeX = FIntVector2(0, 0);

	/** Number of cells to crop from the Min/Max sides of the grid in the Y dimension (negative values ignored) */
	UPROPERTY(EditAnywhere, Category = "Grid Cropping")
	FIntVector2 CropRangeY = FIntVector2(0, 0);

	/** Number of cells to crop from the Min/Max sides of the grid in the Z dimension (negative values ignored) */
	UPROPERTY(EditAnywhere, Category = "Grid Cropping")
	FIntVector2 CropRangeZ = FIntVector2(0, 0);

	UPROPERTY(EditAnywhere, Category = "Grid Cropping")
	FGSActionButtonGroup CropButtons;


	/** Translation of the grid cells, can be used to reposition the grid origin/pivot cell (0,0,0) */
	UPROPERTY(EditAnywhere, Category = "Grid Transform")
	FIntVector Translation = FIntVector(0,0,0);

	UPROPERTY(EditAnywhere, Category = "Grid Transform")
	FGSActionButtonGroup PivotButtons;

	/** Flip the grid along the X axis by negating the X coordinates */
	UPROPERTY(EditAnywhere, Category = "Grid Transform")
	bool bFlipX = false;

	/** Flip the grid along the Y axis by negating the Y coordinates */
	UPROPERTY(EditAnywhere, Category = "Grid Transform")
	bool bFlipY = false;

	/** Flip the grid along the Z axis by negating the Z coordinates */
	UPROPERTY(EditAnywhere, Category = "Grid Transform")
	bool bFlipZ = false;

	/** Number of cells along each grid axis */
	UPROPERTY(VisibleAnywhere, Category = "Current Grid Info", meta=(DisplayName="Grid Size", NoResetToDefault))
	FIntVector GridExtents;

	/** Minimum grid cell index */
	UPROPERTY(VisibleAnywhere, Category = "Current Grid Info", meta = (DisplayName = "Min Index", NoResetToDefault))
	FIntVector GridMin;

	/** Maximum grid cell index */
	UPROPERTY(VisibleAnywhere, Category = "Current Grid Info", meta = (DisplayName = "Max Index", NoResetToDefault))
	FIntVector GridMax;
};




UCLASS()
class GRADIENTSPACEUETOOLBOX_API UReshapeModelGridTool 
	: public UInteractiveTool, public IInteractiveToolCameraFocusAPI
{
	GENERATED_BODY()
public:

	UReshapeModelGridTool();

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;


	// IInteractiveToolCameraFocusAPI implementation
	virtual bool SupportsWorldSpaceFocusBox() override;
	virtual FBox GetWorldSpaceFocusBox() override;
	virtual bool SupportsWorldSpaceFocusPoint() override;
	virtual bool GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut) override;

	
public:
	void SetTargetWorld(UWorld* World);
	UWorld* GetTargetWorld();

	void SetExistingActor(AGSModelGridActor* Actor);


public:
	UPROPERTY()
	TObjectPtr<UReshapeModelGridSettings> Settings = nullptr;

protected:

	UPROPERTY()
	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	UPROPERTY()
	TWeakObjectPtr<AGSModelGridActor> ExistingActor = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ActiveMaterial = nullptr;

	TArray<TObjectPtr<UMaterialInterface>> ActiveMaterials;

	UPROPERTY()
	TObjectPtr<UPreviewMesh> PreviewMesh;

	TPimplPtr<FReshapeModelGridToolInternalData> InternalData;

	virtual void UpdatePreviewGridShape();
	virtual void OnGridCellDimensionsModified();

	FVector3d GetCellDimensions() const;

	UE::Geometry::FFrame3d GridFrame;

	UPROPERTY()
	TObjectPtr<UConstructionPlaneMechanic> PlaneMechanic = nullptr;
	void UpdateGridPivotFrame(const UE::Geometry::FFrame3d& Frame);


protected:
	TSharedPtr<FGSActionButtonTarget> CropButtonsTarget;
	virtual void OnCropButtonClicked(FString CommandName);
	TSharedPtr<FGSActionButtonTarget> PivotButtonsTarget;
	virtual void OnPivotButtonClicked(FString CommandName);
};


