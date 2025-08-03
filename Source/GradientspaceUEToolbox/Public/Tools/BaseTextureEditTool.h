// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "BaseTools/MeshSurfacePointTool.h"
#include "InteractiveToolQueryInterfaces.h"
#include "TransformTypes.h"
#include "FrameTypes.h"
#include "IntVectorTypes.h"
#include "Utility/CameraUtils.h"
#include "PropertyTypes/ActionButtonGroup.h"
#include "PropertyTypes/EnumButtonsGroup.h"

#include "BaseTextureEditTool.generated.h"

class UWorld;
class UPreviewMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UMaterial;


USTRUCT()
struct FMaterialTextureListTex
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "TextureSet")
	TObjectPtr<UTexture2D> Texture;

	// unique ID in parent FToolMaterialTextureSet, generated on construction
	int UniqueID = 0;

	// saved material parameter info (if this texture is a parameter)
	bool bIsParameter = false;
	FName ParameterName = NAME_None;
};


USTRUCT()
struct FMaterialTextureList
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "TextureSet")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, Category = "TextureSet")
	int SourceMaterialSetIndex;

	UPROPERTY(EditAnywhere, Category = "TextureSet")
	TArray<FMaterialTextureListTex> Textures;
};


USTRUCT()
struct FToolMaterialTextureSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="TextureSet")
	TArray<FMaterialTextureList> MaterialTextures;

	bool FindTextureByIdentifier(int UniqueID, int& MaterialIndex, int& TextureIndex) const;
	bool FindTexture(const UMaterialInterface* Material, const UTexture2D* Texture, int& MaterialIndex, int& TextureIndex) const;
};


UENUM()
enum class EGSTextureFilterMode : uint8
{
	Nearest UMETA(DisplayName = "Nearest"),
	Bilinear UMETA(DisplayName = "Bi-linear"),
	Trilinear UMETA(DisplayName = "Tri-linear"),
	Default UMETA(DisplayName = "Default (from Texture Group)")
};




UCLASS()
class GRADIENTSPACEUETOOLBOX_API UTextureSelectionSetSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** 
	 * The set of available Paintable Textures, collected from the Materials assigned to the target object.
	 * (Click to expand)
	 */
	UPROPERTY(EditAnywhere, Category = "Active Texture", meta = (NoResetToDefault, TransientToolProperty))
	FToolMaterialTextureSet TextureSet;

	/** 
	 * ActiveMaterial contains the Active Texture that edits will be applied to.
	 * During painting, a live preview is shown on a temporary copy of this Material
	 */
	UPROPERTY(VisibleAnywhere, Category = "Active Texture", AdvancedDisplay, meta = (NoResetToDefault, TransientToolProperty))
	TObjectPtr<UMaterialInterface> ActiveMaterial;

	UPROPERTY(EditAnywhere, Category = "Active Texture", meta = ())
	FGSEnumButtonsGroup ViewMode;

	/** 
	 * Active Texture is the Texture2D asset that edits will be applied to.
	 * During painting, a live preview is shown on a temporary copy of this Texture.
	 * Changes to the texture are committed when (1) changing the Active Texture or (2) Accepting the Tool
	 * (If you Cancel the Tool, changes to the current Texture are discarded, but edits committed by changing the Active Texture will remain, and can be removed with Undo)
	 */
	UPROPERTY(VisibleAnywhere, Category = "Active Texture", AdvancedDisplay, meta = (NoResetToDefault, TransientToolProperty))
	TObjectPtr<UTexture2D> ActiveTexture;

	/** Configure active type of Texture Sampler Filtering in the paint preview ( hotkey [n] : toggle between Nearest and texture default ) */
	UPROPERTY(EditAnywhere, Category = "Active Texture", AdvancedDisplay, meta = (NoResetToDefault, TransientToolProperty))
	EGSTextureFilterMode FilterMode = EGSTextureFilterMode::Nearest;

	/** 
	 * The pixel format of the current Texture's Source data. Committing changes to the Texture will update this source image 
	 * Note that painting is always RGBA8, so if the Source data is single-channel, the remaining channels will be discarded on commmit.
	 * Similarly if the Source has a higher bit-depth (eg half-float, float32, etc), values > 1 will be lost on commit.
	 */
	//UPROPERTY(VisibleAnywhere, Category = "Active Texture", AdvancedDisplay, meta = (NoResetToDefault, TransientToolProperty))
	UPROPERTY(meta = (TransientToolProperty))
	FString SourceFormat;
	/**
	 * The compression mode used to compress the current Texture. Compression is not visible while painting.
	 * You must switch to another texture, or accept the tool, to see the results of texture compression.
	 */
	//UPROPERTY(VisibleAnywhere, Category = "Active Texture", AdvancedDisplay, meta = (NoResetToDefault, TransientToolProperty))
	UPROPERTY(meta = (TransientToolProperty))
	FString CompressedFormat;

	void SetActiveTextureByUniqueID(int UniqueID) {
		OnUpdateActiveTextureInTool(UniqueID);
	}
	int FindTextureUniqueID(const UMaterialInterface* Material, const UTexture2D* Texture) const;

	// this event is for the UI to listen to
	DECLARE_MULTICAST_DELEGATE_OneParam(OnActiveTextureModifiedEvent, const UTextureSelectionSetSettings*);
	OnActiveTextureModifiedEvent OnActiveTextureModifiedByTool;

	//! Tool sets this lambda to respond to selection changes in UI
	TFunction<void(int UniqueID)> OnUpdateActiveTextureInTool;


	// hidden properties used to try to restore texture & material on startup
	UPROPERTY()
	TObjectPtr<UMaterialInterface> LastActiveMaterial;
	UPROPERTY()
	TObjectPtr<UTexture2D> LastActiveTexture;
};



UCLASS()
class GRADIENTSPACEUETOOLBOX_API UCompressionPreviewSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Compression Preview")
	FGSActionButtonGroup Actions;
};



UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGSBaseTextureEditTool : public UMeshSurfacePointTool,
	public IInteractiveToolNestedAcceptCancelAPI
{
	GENERATED_BODY()
public:
	virtual void SetTargetWorld(UWorld* World);

	virtual void Setup() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	// control functions

	virtual void SetNearestFilteringOverride(bool bEnable);
	virtual void ToggleCompressionPreview(bool bSetToEnabled);
	virtual void EnsureCompressionPreviewDisabled();


	// IInteractiveToolNestedAcceptCancelAPI
	virtual bool SupportsNestedCancelCommand() override { return true; }
	virtual bool CanCurrentlyNestedCancel() override;
	virtual bool ExecuteNestedCancelCommand() override;
	virtual bool SupportsNestedAcceptCommand() override { return false; }
	virtual bool CanCurrentlyNestedAccept() override { return false; }
	virtual bool ExecuteNestedAcceptCommand() override { return false; }


protected:
	// APIs that children can implement
	
	//! called twice, once before texture-commit transaction is opened, and again inside it if the texture-change is transacting  (maybe just make two separate events?)
	//! TexPaintTool uses this to bake in active-layer
	virtual void PreActiveTargetTextureUpdate(bool bInTransaction) {}
	virtual void OnActiveTargetTextureUpdated() {}
	virtual void PreTextureChangeOnUndoRedo() {}
	virtual bool HideTargetObjectOnStartup() const { return true; }

	//! this is used for texture visualization
	virtual UE::Geometry::FIndex4i GetActiveChannelViewMask() const { return UE::Geometry::FIndex4i(1,1,1,1); }

protected:
	UPROPERTY()
	TObjectPtr<UTextureSelectionSetSettings> TextureSetSettings;

	UPROPERTY()
	TObjectPtr<UCompressionPreviewSettings> CompressionPreviewSettings;

protected:
	TWeakObjectPtr<UWorld> TargetWorld;

	UE::Geometry::FTransformSRT3d TargetTransform;
	FVector3d TargetScale;
	TArray<UMaterialInterface*> InitialMaterials;

	UPROPERTY()
	TObjectPtr<UPreviewMesh> PreviewMesh;

	bool ComputeHitPixel(const FRay& WorldRay, const FHitResult& HitResult,
		FVector3d& LocalPoint, FVector3d& LocalNormal,
		int& HitTriangleID,
		FVector2f& HitUVPos, UE::Geometry::FVector2i& HitPixelCoords) const;

protected:
	FViewCameraState CameraState;
	GS::FViewProjectionInfo LastViewInfo;

protected:
	//! selected source texture that we copy from, and write from on accept or texture-change
	UPROPERTY()
	TObjectPtr<UTexture2D> SourcePaintTexture;

	//! transient/temporary paint texture, derived from SourcePaintTexture. Strokes get composited here.
	UPROPERTY()
	TObjectPtr<UTexture2D> ActivePaintTexture;

	//! duplicate of Material containing SourcePaintTexture, where it has been replaced by ActivePaintTexture
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ActivePaintMaterial;

	//! material used to visualize ActivePaintTexture directly
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> TextureVizMaterial;

	UPROPERTY()
	int ActivePaintMaterialID = 0;		// Material ID on target paint mesh - index into SourceMaterials

	UPROPERTY()
	int PaintTextureWidth = 0;
	UPROPERTY()
	int PaintTextureHeight = 0;
	UPROPERTY()
	bool bIsSRGB = true;

	UPROPERTY()
	bool bActivePaintTextureValid = false;
	virtual bool IsActivePaintTextureValid() const { return bActivePaintTextureValid; }

	//! return MaterialID for active paint texture/material, or -1 if not valid
	virtual int GetActivePaintMaterialID() const;

	TArray<FColor> TextureColorBuffer4b;
	void PushFullImageUpdate();
	void PushRegionImageUpdate(int PixelX, int PixelY, int Width, int Height);
	void UpdateSamplingMode();

	FColor ConvertToFColor(const FLinearColor& LinearColor) const;
	FLinearColor ConvertToLinear(const FColor& Color) const;

	void UpdateActiveTargetTextureFromExternal(int UniqueTextureID);
	void UpdateActiveTargetTexture(UMaterialInterface* SourceMaterial, const UTexture2D* SourceTexture, bool bBroadcastChange);
	void InitializeActivePaintTextureMaterial(UMaterialInterface* SourceMaterial, FMaterialTextureListTex SourceTextureInfo);

	friend class FPixelPaint_TextureChange;
	void UndoRedoTextureChange(const FPixelPaint_TextureChange& Change, bool bUndo);

	int NestedCancelCount = 0;
	int CurrentTextureStrokeCount = 0;

	void EnableCompressionPreviewPanel();
	bool bCompressionPreviewEnabled = false;

	void UpdateCurrentVisibleMaterials();
};

