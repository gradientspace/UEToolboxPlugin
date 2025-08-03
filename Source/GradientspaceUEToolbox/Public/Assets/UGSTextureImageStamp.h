// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Assets/UGSImageStamp.h"
#include "UGSTextureImageStamp.generated.h"

class UTexture2D;


UENUM(BlueprintType)
enum class EGSImageStampColorMode : uint8
{
	/** 
	 * Use image Red/Green/Blue as stamp RGB color. Alpha = 1.
	 */
	RGBColor,
	/**
	 * Use image Red/Green/Blue/Alpha as stamp RGBA color.
	 */
	RGBAColor,
	/** 
	 * Use specified image channel as Alpha.
	 * RGB color is provided externally
	 */
	ChannelAsAlpha,
	/**
	 * Use image Red/Green/Blue as stamp RGB color. Use an existing image channel as the Alpha channel.
	 */
	RGBColorWithChannelAsAlpha
};

UENUM(BlueprintType)
enum class EGSImageStampChannel : uint8
{
	Red = 0,
	Green = 1,
	Blue = 2,
	Alpha = 3,
	Value = 4
};


UCLASS(BlueprintType)
class UGSTextureImageStamp : public UGSImageStamp
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ImageStamp")
	TObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageStamp")
	EGSImageStampColorMode ColorMode = EGSImageStampColorMode::RGBColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageStamp", meta=(EditCondition="ColorMode==EGSImageStampColorMode::ChannelAsAlpha || ColorMode==EGSImageStampColorMode::RGBColorWithChannelAsAlpha"))
	EGSImageStampChannel AlphaChannel = EGSImageStampChannel::Alpha;
};