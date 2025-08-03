// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolQueryInterfaces.h"
#include "FrameTypes.h"
#include "Utility/CameraUtils.h"

#include "CreateModelGridTool.generated.h"

class UWorld;
class AGSModelGridActor;
class UPreviewMesh;
class UModelGrid;
class FGridPreviewOpFactory;
class UMaterialInstanceDynamic;
class UMaterialInterface;
struct FCreateModelGridToolInternalData;
class UConstructionPlaneMechanic;
class UDragAlignmentMechanic;

namespace GS { class ModelGrid; }


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UCreateModelGridToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	virtual UCreateModelGridTool* CreateNewToolOfType(const FToolBuilderState& SceneState) const;
};

UENUM()
enum class ECreateModelGridInitialGrid : uint8
{
	SingleCell,
	NoCells,
	Plane,
	Box
};


UENUM()
enum class ECreateModelGridOrigin : uint8
{
	Center,
	BaseZ,
	Zero
};


UCLASS()
class GRADIENTSPACEUETOOLCORE_API UCreateModelGridSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** type of initial grid */
	UPROPERTY(EditAnywhere, Category = "Grid Shape", meta=(DisplayName="New Grid Contents"))
	ECreateModelGridInitialGrid GridType = ECreateModelGridInitialGrid::SingleCell;

	UPROPERTY(EditAnywhere, Category = "Grid Shape", meta = (UIMin = 1, UIMax = 100, ClampMax = 512, EditCondition = "GridType==ECreateModelGridInitialGrid::Plane || GridType==ECreateModelGridInitialGrid::Box", EditConditionHides))
	int NumCellsX = 4;

	UPROPERTY(EditAnywhere, Category = "Grid Shape", meta = (UIMin = 1, UIMax = 100, ClampMax = 512, EditCondition = "GridType==ECreateModelGridInitialGrid::Plane || GridType==ECreateModelGridInitialGrid::Box", EditConditionHides))
	int NumCellsY = 4;

	UPROPERTY(EditAnywhere, Category = "Grid Shape", meta = (UIMin = 1, UIMax = 100, ClampMax = 512, EditCondition = "GridType==ECreateModelGridInitialGrid::Box", EditConditionHides))
	int NumCellsZ = 4;

	UPROPERTY(EditAnywhere, Category = "Grid Shape", meta = (EditCondition = "GridType==ECreateModelGridInitialGrid::Plane || GridType==ECreateModelGridInitialGrid::Box", EditConditionHides))
	ECreateModelGridOrigin Origin = ECreateModelGridOrigin::BaseZ;

	/** Allow separate dimensions along the grid X/Y/Z dimensions */
	UPROPERTY(EditAnywhere, Category = "Grid Cell Size")
	bool bNonUniform = false;

	/** Fixed grid cell dimension */
	UPROPERTY(EditAnywhere, Category = "Grid Cell Size", meta = (UIMin = 5, UIMax = 250, ClampMin = 0.1, ClampMax = 10000, EditCondition = "bNonUniform==false", EditConditionHides))
	double Dimension = 50.0;

	/** Fixed grid cell X dimension */
	UPROPERTY(EditAnywhere, Category = "Grid Cell Size", meta = (UIMin = 5, UIMax = 250, ClampMin = 0.1, ClampMax = 10000, EditCondition = "bNonUniform==true", EditConditionHides))
	double DimensionX = 50.0;
	/** Fixed grid cell Y dimension */
	UPROPERTY(EditAnywhere, Category = "Grid Cell Size", meta = (UIMin = 5, UIMax = 250, ClampMin = 0.1, ClampMax = 10000, EditCondition = "bNonUniform==true", EditConditionHides))
	double DimensionY = 50.0;
	/** Fixed grid cell Z dimension */
	UPROPERTY(EditAnywhere, Category = "Grid Cell Size", meta = (UIMin = 5, UIMax = 250, ClampMin = 0.1, ClampMax = 10000, EditCondition = "bNonUniform==true", EditConditionHides))
	double DimensionZ = 50.0;


	/** 
	 * Automatically position the grid relative to the current View. 
	 * Automatic placement will be locked if the position is manually set using the 3D gizmo.
	 **/
	//UPROPERTY(EditAnywhere, Category = "Placement", meta=(EditCondition="!bPlacementIsLocked", HideEditConditionToggle))
	UPROPERTY(meta = (TransientToolProperty))
	bool bAutoPosition = false;

	UPROPERTY(meta=(TransientToolProperty))
	bool bPlacementIsLocked = false;

	/** 
	 * Name of created Actor (and Asset, if desired) 
	 */
	UPROPERTY(EditAnywhere, Category = "Output")
	FString Name = TEXT("ModelGrid");

	/** 
	 * If true, new ModelGrid Asset will be created and assigned to the new ModelGrid Actor.
	 * Otherwise, the created ModelGrid will be stored inside the new Actor's GridComponent
	 */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bCreateAsset = false;
};




UCLASS()
class GRADIENTSPACEUETOOLCORE_API UCreateModelGridTool 
	: public UInteractiveTool, public IInteractiveToolCameraFocusAPI
{
	GENERATED_BODY()
public:

	UCreateModelGridTool();

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

	// subclass can override this to implement more complex functionality (eg like creating asset in Editor tool)
	virtual void OnCreateNewModelGrid(GS::ModelGrid&& SourceGrid);

public:
	UPROPERTY()
	TObjectPtr<UCreateModelGridSettings> Settings = nullptr;

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

	TPimplPtr<FCreateModelGridToolInternalData> InternalData;

	virtual void UpdatePreviewGridShape();
	virtual void OnGridCellDimensionsModified();

	FVector3d GetCellDimensions() const;

	UE::Geometry::FFrame3d GridFrame;

	UPROPERTY()
	TObjectPtr<UConstructionPlaneMechanic> PlaneMechanic = nullptr;
	UPROPERTY()
	TObjectPtr<UDragAlignmentMechanic> DragAlignmentMechanic = nullptr;
	void UpdateGridFrameFromGizmo(const UE::Geometry::FFrame3d& Frame);

	FViewCameraState ViewState;
	GS::FViewProjectionInfo LastViewInfo;
	bool bHaveViewState = false;

	bool bAutoPlacementPending = false;
	bool bAutoPlacementLocked = false;
	void UpdateAutoPlacement();
};


