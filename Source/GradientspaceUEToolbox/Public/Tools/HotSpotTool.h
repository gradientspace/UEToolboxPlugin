// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BaseTools/SingleTargetWithSelectionTool.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshSharingUtil.h"
#include "Templates/PimplPtr.h"

#include "Properties/MeshMaterialProperties.h"
#include "Properties/MeshUVChannelProperties.h"
#include "PropertySets/PolygroupLayersProperties.h"
#include "Polygroups/PolygroupSet.h"

#include "HotSpotTool.generated.h"

// Forward declarations
class UStaticMesh;
class UPreviewMesh;
class UMeshOpPreviewWithBackgroundCompute;
class FHotSpotUVOpFactory;
class UMaterialInterface;
namespace UE::Geometry { class FMeshRegionOperator; }

/**
 *
 */
UCLASS()
class GRADIENTSPACEUETOOLBOX_API UHotSpotToolBuilder : public USingleTargetWithSelectionToolBuilder
{
	GENERATED_BODY()

public:
	virtual USingleTargetWithSelectionTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	virtual bool RequiresInputSelection() const override { return false; }
};





UCLASS()
class GRADIENTSPACEUETOOLBOX_API UHotSpotToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "HotSpot", meta = (DisplayName = "Source Asset"))
	TObjectPtr<UStaticMesh> Source;

	UPROPERTY(EditAnywhere, Category = "HotSpot")
	double ScaleFactor = 1.0;

	UPROPERTY(EditAnywhere, Category = "HotSpot")
	bool bForceUpdate = false;

	UPROPERTY(EditAnywhere, Category = "HotSpot")
	int32 SelectionShift = 0;
};



UCLASS()
class GRADIENTSPACEUETOOLBOX_API UHotSpotTool : public USingleTargetWithSelectionTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

protected:
	UPROPERTY()
	TObjectPtr<UHotSpotToolProperties> Settings = nullptr;

	UPROPERTY()
	TObjectPtr<UMeshUVChannelProperties> UVChannelProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UPolygroupLayersProperties> PolygroupLayerProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UExistingMeshMaterialProperties> MaterialSettings = nullptr;

	UPROPERTY()
	TObjectPtr<UPreviewMesh> SourcePreview = nullptr;

	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> EditCompute = nullptr;


	UE::Geometry::FTransformSRT3d WorldTransform;

	UE::Geometry::FDynamicMesh3 InputMesh;
	TArray<int32> TriangleSelection;
	TSet<int32> ModifiedROI;

	TPimplPtr<UE::Geometry::FMeshRegionOperator> RegionOperator;
	UE::Geometry::FDynamicMesh3 EditRegionMesh;
	TSharedPtr<UE::Geometry::FSharedConstDynamicMesh3> EditRegionSharedMesh;


	TPimplPtr<FHotSpotUVOpFactory> OperatorFactory;
	friend class FHotSpotUVOpFactory;

	UMaterialInterface* ActiveHotSpotMaterial = nullptr;

	TSharedPtr<UE::Geometry::FPolygroupSet, ESPMode::ThreadSafe> ActiveGroupSet;
	void OnSelectedGroupLayerChanged();
	void UpdateActiveGroupLayer();
	int32 GetSelectedUVChannel() const;
	void OnPreviewMeshUpdated();
	void OnHotSpotSourceUpdated();
};
