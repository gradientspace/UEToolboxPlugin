// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"

#include "PropertyTypes/ActionButtonGroup.h"
#include "PropertyTypes/OrderedOptionSet.h"

#include "CreateTextureTool.generated.h"

class UWorld;

UCLASS(MinimalAPI)
class UGSCreateTextureToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const {
		return true;
	}
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const;
};


UENUM()
enum class EGSCreateTextureToolFilter : uint8
{
	Nearest UMETA(DisplayName = "Nearest"),
	Bilinear UMETA(DisplayName = "Bi-linear"),
	Trilinear UMETA(DisplayName = "Tri-linear"),
	Default UMETA(DisplayName = "Default (from Texture Group)")
};

UENUM()
enum class EGSCreateTextureToolBorder : uint8
{
	Wrap,
	Clamp,
	Mirror
};

UENUM()
enum class EGSCreateTextureToolSourceFormat : uint8
{
	Invalid				UMETA(Hidden), 
	/** single-channel 8-bit grayscale   (red channel) */
	G8					UMETA(DisplayName = "G8"),
	/** 4x8-bit RGBA, standard image format */
	BGRA8				UMETA(DisplayName = "BGRA8"),
	/** 4x8-bit RGB, 8-bit Exponent   (increased dynamic range, for HDR) */
	BGRE8				UMETA(DisplayName = "BGRE8"),
	/** 4x16-bit integer RGBA */
	RGBA16				UMETA(Hidden, DisplayName = "RGBA16"),
	/** 4x16-bit half-float RGBA */
	RGBA16F				UMETA(DisplayName = "RGBA16F"),

	RGBA8_DEPRECATED	UMETA(Hidden),
	RGBE8_DEPRECATED	UMETA(Hidden),

	/** single-channel 16-bit grayscale   (red channel) */
	G16					UMETA(Hidden, DisplayName = "G16"),
	/** 4x32-bit full-float RGBA */
	RGBA32F				UMETA(DisplayName = "RGBA32F"),
	/** single-channel 16-bit half-float grayscale   (red channel) */
	R16F				UMETA(DisplayName = "R16F"),
	/** single-channel 32-bit full-float grayscale   (red channel) */
	R32F				UMETA(DisplayName = "R32F")
};


UENUM()
enum class EGSCreateTextureToolCompression : uint8
{
	/** 
	 * Default texture compression for colored images. 
	 * Use for Albedo/Diffuse textures
	 * No Alpha:    DXT1 (BC1 on DX11+).   approx .5 byte/pixel
	 * With Alpha:  DXT5 (BC3 on DX11+).   approx  1 byte/pixel
	 * sRGB supported
	 */
	Default					UMETA(DisplayName = "Default (Compressed RGBA8)"),

	/** 
	 * Default texture compression for Normal Maps.
	 * Compressed with DXT5 (BC5 on DX11+)
	 * BC5 stores 8-bit red & green, blue & alpha are discarded (approx 1 byte/pixel)
	 * no SRGB
	 */
	NormalMap				UMETA(DisplayName = "Normal Map (Compressed RG8)"),

	/** 
	 * Default texture compression for Mask images.
	 * Use for multi-channel non-color textures (eg combined Metallic-Roughness-Specular (MRS), etc)
	 * No Alpha:    DXT1 (BC1 on DX11+).   approx .5 byte/pixel
	 * With Alpha:  DXT5 (BC3 on DX11+).   approx  1 byte/pixel
	 * sRGB not supported, texture is interpreted as Linear 
	 * (Equivalent to Default (DXT1/DXT5, or BC1/BC3 on DX11+) with sRGB disabled)
	 */
	Masks					UMETA(DisplayName = "Masks (Compressed RGBA8, no SRGB)"),

	/** 
	 * Grayscale
	 * sRGB disabled: uncompressed single-channel 8-bit grayscale   (red channel)
	 * sRGB enabled: uncompressed B8G8R8A8  
	 * Use for single-channel map textures  (eg Metallic, Specular, etc)
	 */
	Grayscale				UMETA(DisplayName = "Grayscale (G8*)"),

	/** 
	 * Use for single-channel displacement maps  (eg bump map)
	 * sRGB disabled: uncompressed single-channel 8-bit grayscale   (red channel)
	 * sRGB enabled: uncompressed B8G8R8A8
	 */
	DisplacementMap			UMETA(DisplayName = "DisplacementMap (G8*)"),

	/** 
	 * Uncompressed B8G8R8A8 storage, 4 bytes/pixel
	 * Use to store (eg) encoded XYZ positions or displacement vectors.
	 * This format is directly readable as an FColor at Runtime (eg by Geometry Script, etc)
	 * no sRGB
	 */
	Uncompressed			UMETA(DisplayName = "VectorDisplacementMap (RGBA8*)"),

	/** 
	 * Uncompressed 4-channel half-float (8 bytes/pixel)
	 * no sRGB 
	 */
	HDR						UMETA(DisplayName = "HDR (RGBA16F*)"),

	/** 
	 * Uncompressed RGBA images, intended for user interface elements with alpha blending/etc
	 * 8-bit Source images stored as B8G8R8A8
	 * 16/32-bit Source images stored as FloatRGBA (16-bit half-float)
	 * sRGB supported
	 */
	UserInterface			UMETA(DisplayName = "User Interface (BGRA8*)"),

	/** 
	 * Single-channel 16-bit grayscale    (red channel)
	 * use for alpha masks, other high-precision maps
	 * BC4 compression on DX11+, approx .5 byte/pixel
	 * no sRGB
	 */
	Alpha					UMETA(DisplayName = "Alpha BG4 (Compressed G16)"),

	/** 
	 * uncompressed 8-bit Grayscale G8 format     (alpha channel)
	 * sRGB supported
	 */
	DistanceFieldFont		UMETA(DisplayName = "DistanceFieldFont (G8*)"),

	/** 
	 * Compressed format for HDR images. 
	 * 3-channel RGB 16-bit half-float with BC6h Compression, approx 1 byte/pixel
	 * Only supported on DX11+
	 */
	HDR_Compressed			UMETA(DisplayName = "HDR (Compressed RGB16F)"),

	/** 
	 * Compressed format for color images, both RGB and RGBA. Higher quality than Default.
	 * Compressed with BC7, approx 1 byte/pixel
	 * sRGB supported.
	 * Only supported on DX11+
	 */
	BC7						UMETA(DisplayName = "BC7 (Compressed RGBA8)"),

	/** 
	 * Uncompressed single-channel 16-bit half-float   (red channel) 
	 * Use for high-quality mask/grayscale images, displacement, or other compute-related purposes
	 */
	HalfFloat				UMETA(DisplayName = "HalfFloat (R16F*)"),
	LowQuality					UMETA(Hidden),
	EncodedReflectionCapture	UMETA(Hidden),
	/** 
	 * Uncompressed Single-channel 32-bit float   (red channel) 
	 * Use for maximum-quality mask/grayscale images, displacement, or other compute-related purposes
	 */
	SingleFloat				UMETA(DisplayName = "SingleFloat (R32F*)"),

	/** 
	 * Uncompressed 4-channel RGBA 32-bit full-float  (FLinearColor)
	 * Use for maximum-quality color images, or other compute-related purposes
	 */
	HDR_F32					UMETA(DisplayName = "HDR High-Precision (RGBA32F*)")
};



UCLASS(MinimalAPI)
class UGSCreateTextureToolSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** X and Y pixel dimesions (width/height) of the created texture image */
	UPROPERTY(EditAnywhere, Category = "Dimensions", meta = (DisplayName="Width/Height", EditCondition = "bCustomDimensions==false", EditConditionHides, HideEditConditionToggle, TransientToolProperty))
	FGSOrderedOptionSet Resolution;

	UPROPERTY()
	int SavedDefaultResolutionIndex = 9;		// this is just used to init & save Resolution value across runs

	UPROPERTY()
	bool bCustomDimensions = false;

	/** Custom X/Width pixel dimension */
	UPROPERTY(EditAnywhere, Category = "Dimensions", meta = (EditCondition="bCustomDimensions", UIMin = 1, UIMax = 8192, ClampMax = 16384))
	int CustomWidth = 512;

	/** Y/Height pixel dimension */
	UPROPERTY(EditAnywhere, Category = "Dimensions", meta = (EditCondition= "bCustomDimensions", EditConditionHides, HideEditConditionToggle, UIMin = 1, UIMax = 8192, ClampMax = 16384))
	int CustomHeight = 512;

	UPROPERTY()
	bool bShowResolutionWarning = false;

	UPROPERTY(VisibleAnywhere, Category="Dimensions", meta=(EditCondition="bShowResolutionWarning",EditConditionHides,HideEditConditionToggle))
	FString ResolutionWarning = "";


	/**
	 * Format of the Source image data. 
	 * This is the "true" image data, that will be converted to a Platform-specific format based on the Compression Mode.
	 */
	UPROPERTY(EditAnywhere, Category = "Texture Settings", meta = ())
	EGSCreateTextureToolSourceFormat SourceFormat = EGSCreateTextureToolSourceFormat::BGRA8;

	/** 
	 * Compression Setting used to during conversion of the Source image into a Platform-specific GPU texture buffer representation.
	 * 
	 * Formats marked with a (*) are uncompressed
	 */
	UPROPERTY(EditAnywhere, Category = "Texture Settings", meta = ())
	EGSCreateTextureToolCompression CompressionMode = EGSCreateTextureToolCompression::Uncompressed;

	/**
	 * If true, the Source image data is interpreted as being SRGB-encoded, otherwise it is interpreted as being Linear
	 * 
	 * Only G8 and BGRA8 Source Formats support sRGB encoding 
	 */
	UPROPERTY(EditAnywhere, Category = "Texture Settings", meta = (EditCondition=bFormatSupportsSRGB, HideEditConditionToggle))
	bool bSRGB = true;

	UPROPERTY()
	bool bFormatSupportsSRGB = true;

	/**
	 * Enable 'Virtual Texturing Streaming' on this Texture. 
	 * 
	 * This option is only configurable if Virtual Texturing is enabled in the Project Settings.
	 */
	UPROPERTY(EditAnywhere, Category = "Texture Settings", meta = (EditCondition=bEnableVTOption, HideEditConditionToggle))
	bool bVirtualTexturing = true;

	UPROPERTY()
	bool bEnableVTOption = false;


	/** 
	 * Texture Filtering Mode for the Output Texture 
	 */
	UPROPERTY(EditAnywhere, Category = "Texture Settings", meta = ())
	EGSCreateTextureToolFilter FilterMode = EGSCreateTextureToolFilter::Default;


	UPROPERTY(EditAnywhere, Category = "Texture Settings", meta = (TransientToolProperty))
	FGSActionButtonGroup FormatPresets;


	//UPROPERTY(VisibleAnywhere, Category = "Settings", meta = ())
	//bool bVT = true;

	/** Border Mode */
	//UPROPERTY(EditAnywhere, Category = "Settings", AdvancedDisplay, meta = ())
	//EGSCreateTextureToolBorder BorderMode = EGSCreateTextureToolBorder::Wrap;


	/** Fill color for the generated Texture */
	UPROPERTY(EditAnywhere, Category = "Fill Settings", meta = ())
	FColor FillColor = FColor::Black;

	UPROPERTY()
	bool bOverrideAlpha = false;

	/** Custom Alpha value that will be used instead of the FillColor.A value */
	UPROPERTY(EditAnywhere, Category = "Fill Settings", meta = (EditCondition = "bOverrideAlpha", ClampMin = 0, ClampMax = 255))
	int OverrideAlpha = 255;

	UPROPERTY(EditAnywhere, Category = "Fill Settings", meta = (TransientToolProperty))
	FGSActionButtonGroup ColorPresets;




	UPROPERTY(EditAnywhere, Category = "Create Texture", meta = ())
	FString BaseName = TEXT("T_Image");

	UPROPERTY(EditAnywhere, Category = "Create Texture", meta = (TransientToolProperty))
	FGSActionButtonGroup Actions;
};


UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGSCreateTextureTool : public UInteractiveTool
{
	GENERATED_BODY()
public:
	UGSCreateTextureTool();
	virtual void SetTargetWorld(UWorld* World);

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	void CreateTextureWithActiveSettings();

protected:
	TWeakObjectPtr<UWorld> TargetWorld;

	UPROPERTY()
	TObjectPtr<UGSCreateTextureToolSettings> Settings;



	TSharedPtr<FGSActionButtonTarget> QuickActionTarget;
	virtual bool OnQuickActionButtonEnabled(FString CommandName);
	virtual void OnQuickActionButtonClicked(FString CommandName);

	void UpdateOnResolutionChange();
	void UpdateOnTexSettingsChange();
};



