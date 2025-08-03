// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BaseTools/BaseVoxelTool.h"
#include "DynamicMesh/MeshSharingUtil.h"
#include "PropertySets/OnAcceptProperties.h"

#include "ModelGridRasterizeTool.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;

namespace GS { class ModelGrid; }


UENUM()
enum class EGridRasterizeSizeType
{
	CellCount = 0,
	WorldDimension = 1
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridRasterizeToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Method to use to determine grid dimensions */
	UPROPERTY(EditAnywhere, Category = GridSettings)
	EGridRasterizeSizeType GridSize = EGridRasterizeSizeType::CellCount;

	/** Approximate number of cells along longest bounding-box axis */
	UPROPERTY(EditAnywhere, Category = GridSettings, meta = (UIMin = 4, UIMax = 128, EditCondition="GridSize==EGridRasterizeSizeType::CellCount", EditConditionHides))
	int CellCount = 50;

	UPROPERTY(EditAnywhere, Category = GridSettings, meta = (EditCondition = "GridSize==EGridRasterizeSizeType::WorldDimension", EditConditionHides))
	bool bNonUniform = false;

	/** Fixed world-space grid cell dimension */
	UPROPERTY(EditAnywhere, Category = GridSettings, meta = (UIMin = 5, UIMax = 250, EditCondition = "GridSize==EGridRasterizeSizeType::WorldDimension && bNonUniform==false", EditConditionHides))
	double Dimension = 10.0;

	/** Fixed world-space grid cell X dimension */
	UPROPERTY(EditAnywhere, Category = GridSettings, meta = (UIMin = 5, UIMax = 250, EditCondition = "GridSize==EGridRasterizeSizeType::WorldDimension && bNonUniform==true", EditConditionHides))
	double DimensionX = 10.0;
	/** Fixed world-space grid cell Y dimension */
	UPROPERTY(EditAnywhere, Category = GridSettings, meta = (UIMin = 5, UIMax = 250, EditCondition = "GridSize==EGridRasterizeSizeType::WorldDimension && bNonUniform==true", EditConditionHides))
	double DimensionY = 10.0;
	/** Fixed world-space grid cell Z dimension */
	UPROPERTY(EditAnywhere, Category = GridSettings, meta = (UIMin = 5, UIMax = 250, EditCondition = "GridSize==EGridRasterizeSizeType::WorldDimension && bNonUniform==true", EditConditionHides))
	double DimensionZ = 10.0;

	/** Number of cells along X/Y/Z axes of resulting grid */
	UPROPERTY(VisibleAnywhere, Category = GridSettings, meta=(NoResetToDefault, DisplayName="CellCounts"))
	FString CellCounts;


	UPROPERTY(EditAnywhere, Category = RasterizeSettings)
	bool bSampleVertexColors = true;


	/** */
	//UPROPERTY(EditAnywhere, Category = RasterizeSettings)
	bool bFillHolesAndSolidify = true;

	/**  */
	//UPROPERTY(EditAnywhere, Category = RasterizeSettings)
	bool bRemoveInterior = true;

	/**  */
	//UPROPERTY(EditAnywhere, Category = RasterizeSettings, meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "1000"))
	double ThickenShells = 0.0;
};



UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridOutputProperties : public UOnAcceptHandleSourcesPropertiesSingle
{
	GENERATED_BODY()
public:
	/** Base name of the newly generated object to which the output is written to. */
	UPROPERTY(EditAnywhere, Category = OutputObject, meta = (TransientToolProperty, DisplayName = "Name", NoResetToDefault))
	FString OutputNewName;
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridRasterizeTool : public UBaseVoxelTool
{
	GENERATED_BODY()

public:
	UModelGridRasterizeTool() {}

protected:

	// hack base tool to repalce shutdown
	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void SetupProperties() override;
	virtual void SaveProperties() override;

	virtual FString GetCreatedAssetName() const override;
	virtual FText GetActionName() const override;

	virtual void ConvertInputsAndSetPreviewMaterials(bool bSetPreviewMesh) override;

	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<UModelGridRasterizeToolProperties> RasterizeProperties;

	UPROPERTY()
	TObjectPtr<UModelGridOutputProperties> GridOutputProperties;

	TArray<TSharedPtr<UE::Geometry::FSharedConstDynamicMesh3>> SharedMeshHandles;
	UE::Geometry::FAxisAlignedBox3d CombinedBounds;


	void OnOpCompleted(const UE::Geometry::FDynamicMeshOperator* Operator);
	TSharedPtr<GS::ModelGrid> LastResultGrid;


	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ActiveMaterial = nullptr;

	TArray<TObjectPtr<UMaterialInterface>> ActiveMaterials;

};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridRasterizeToolBuilder : public UBaseCreateFromSelectedToolBuilder
{
	GENERATED_BODY()

public:
	virtual int32 MinComponentsSupported() const override { return 1; }

	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override
	{
		return NewObject<UModelGridRasterizeTool>(SceneState.ToolManager);
	}
};







