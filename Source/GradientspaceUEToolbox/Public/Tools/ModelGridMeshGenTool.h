// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "PropertySets/CreateMeshObjectTypeProperties.h"
#include "TransformTypes.h"

#include "ModelGridMeshGenTool.generated.h"


class AGSModelGridActor;
class UPreviewMesh;
class UMeshOpPreviewWithBackgroundCompute;
class FGridMeshGenOpFactory;
class UWorld;
class UMaterialInterface;



UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridMeshGenToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};



UENUM()
enum class EModelGridMeshGenGroupModes : uint8
{
	PerFace = 0,
	PlanarAreas = 1
};


UENUM()
enum class EModelGridMeshGenUVModes : uint8
{
	Ignore = 0,
	/** Clear UV channel on output mesh */
	DiscardUVs = 1,
	/** Repack existing UVs assuming the provided TargetUVResolution */
	Repack = 2,
	/** 
	 * Compute UVs for each planar face based on allocating a fixed NxN pixel grid to a unit cell-face
	 * Each planar face will be precisely pixel-aligned
	 */
	FacePixelsPack = 3
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridMeshGenToolSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Meshing")
	bool bRemoveHidden = true;

	UPROPERTY(EditAnywhere, Category = "Meshing")
	bool bSelfUnion = true;

	UPROPERTY(EditAnywhere, Category = "Meshing")
	bool bOptimizePlanarAreas = false;

	UPROPERTY(EditAnywhere, Category = "Meshing")
	bool bInvertFaces = false;

	UPROPERTY(EditAnywhere, Category = "Groups")
	EModelGridMeshGenGroupModes GroupMode = EModelGridMeshGenGroupModes::PerFace;

	UPROPERTY(EditAnywhere, Category="Groups")
	bool bShowGroupColors = false;

	UPROPERTY(EditAnywhere, Category = "UVs")
	EModelGridMeshGenUVModes UVMode = EModelGridMeshGenUVModes::Repack;

	UPROPERTY(EditAnywhere, Category = "UVs", meta=(EditCondition="UVMode==EModelGridMeshGenUVModes::Repack", EditConditionHides))
	int TargetUVResolution = 512;

	UPROPERTY(EditAnywhere, Category = "UVs", meta = (EditCondition = "UVMode==EModelGridMeshGenUVModes::FacePixelsPack", EditConditionHides, UIMin=1,UIMax=10,ClampMin=1,ClampMax=1000))
	int PixelResolution = 4;

	UPROPERTY(EditAnywhere, Category = "UVs", meta = (EditCondition = "UVMode==EModelGridMeshGenUVModes::FacePixelsPack", EditConditionHides, UIMin = 0, UIMax = 10, ClampMin = 1, ClampMax = 64))
	int IslandPixelBorder = 0;

	UPROPERTY(VisibleAnywhere, Category = "UVs", meta = (EditCondition = "UVMode==EModelGridMeshGenUVModes::FacePixelsPack", EditConditionHides))
	int TexResolution = 0;
};

UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridMeshGenTextureSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UModelGridMeshGenTextureSettings();

	/** Create a new Texture suitable for use with Pixel Painting on the meshed grid */
	UPROPERTY(EditAnywhere, Category = "Texture / Material", meta = ())
	bool bCreateTexture = true;

	/** Bake the grid cell/face colors to the Pixel Paint texture (otherwise will be initialized to solid white) */
	UPROPERTY(EditAnywhere, Category = "Texture / Material", meta = (EditCondition = "bCreateTexture", EditConditionHides, HideEditConditionToggle))
	bool bBakeGridColors = true;

	/** Create a new Material with the created Texture assigned to it, and assign that Material to the generated Mesh Asset/Actor */
	UPROPERTY(EditAnywhere, Category = "Texture / Material", meta = (EditCondition = "bCreateTexture", EditConditionHides, HideEditConditionToggle))
	bool bCreateMaterial = true;

	/** The Material to use as a template for the generated Material (ie this Material will be duplicated )*/
	UPROPERTY(EditAnywhere, Category = "Texture / Material", meta = (EditCondition = "bCreateTexture && bCreateMaterial", EditConditionHides, HideEditConditionToggle))
	TObjectPtr<UMaterialInterface> BaseMaterial;

	/** The name of the Texture Parameter in the BaseMaterial to assign the generated Texture to */
	UPROPERTY(EditAnywhere, Category = "Texture / Material", meta = (EditCondition = "bCreateTexture && bCreateMaterial", EditConditionHides, HideEditConditionToggle))
	FString TexParameterName = TEXT("PixelPaintTex");
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UModelGridMeshGenTool : public UInteractiveTool
{
	GENERATED_BODY()
public:
	UModelGridMeshGenTool();

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;


	virtual bool HasCancel() const override;
	virtual bool HasAccept() const override;
	virtual bool CanAccept() const override;


public:

	void SetTargetWorld(UWorld* World);
	UWorld* GetTargetWorld();

	void SetSourceActor(AGSModelGridActor* Actor);


protected:
	UPROPERTY()
	TObjectPtr<UModelGridMeshGenToolSettings> ToolSettings;

	/** Property set for type of output object (StaticMesh, Volume, etc) */
	UPROPERTY()
	TObjectPtr<UCreateMeshObjectTypeProperties> OutputTypeProperties;

	UPROPERTY()
	TObjectPtr<UModelGridMeshGenTextureSettings> TextureOutputProperties;
	


protected:
	UPROPERTY()
	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	UPROPERTY()
	TWeakObjectPtr<AGSModelGridActor> ExistingActor = nullptr;

	UE::Geometry::FTransformSRT3d WorldTransform;
	TArray<UMaterialInterface*> ActiveMaterialSet;

	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> EditCompute = nullptr;

	TPimplPtr<FGridMeshGenOpFactory> OperatorFactory;
	friend class FGridMeshGenOpFactory;

	void UpdateVisualizationSettings();
	void UpdatePropertySetVisibility();

};


