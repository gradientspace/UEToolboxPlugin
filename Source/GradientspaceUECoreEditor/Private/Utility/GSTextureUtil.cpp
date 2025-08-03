// Copyright Gradientspace Corp. All Rights Reserved.
#include "Utility/GSTextureUtil.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "PixelFormat.h"
#include "RenderUtils.h"
#include "ImageCoreUtils.h"
#include "ImageUtils.h"
#include "TextureCompiler.h"

using namespace GS;


template<typename FormatType>
bool InitializeSourceDataFromMip0(UTexture2D* Texture, int Width, int Height, ETextureSourceFormat SourceFormat, bool bFinalUpdateResource)
{
	// configure settings to allow uncompressed PlatformData mip0 to be read directly
	auto PrevCompresionNone = Texture->CompressionNone;
	Texture->CompressionNone = true;
	auto PrevMipGen = Texture->MipGenSettings;
	Texture->MipGenSettings = TMGS_NoMipmaps;
	auto PrevCompression = Texture->CompressionSettings;
	Texture->CompressionSettings = TC_Default;
	Texture->UpdateResource();

	void* RawMipData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_ONLY);
	if (!RawMipData)
		return false;

	const FormatType* SourceMipData = reinterpret_cast<const FormatType*>(RawMipData);
	Texture->Source.Init2DWithMipChain(Width, Height, SourceFormat);
	uint8* DestData = Texture->Source.LockMip(0);
	if (!DestData) {
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		return false;
	}
	//FMemory::Memcpy(DestData, SourceMipData, SizeX * SizeY * sizeof(FormatType));
	FMemory::Memcpy(DestData, SourceMipData, Texture->Source.CalcMipSize(0));


	Texture->Source.UnlockMip(0);
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

	Texture->CompressionNone = PrevCompresionNone;
	Texture->MipGenSettings = PrevMipGen;
	Texture->CompressionSettings = PrevCompression;

	if (bFinalUpdateResource)
		Texture->UpdateResource();

	return true;
}


bool GS::InitializeSourceDataFromMip0(UTexture2D* Texture, int UseWidth, int UseHeight, ERawImageFormat::Type SourceFormat, bool bFinalUpdateResource)
{
	switch (SourceFormat)
	{
	case ERawImageFormat::G8:
		return InitializeSourceDataFromMip0<uint8>(Texture, UseWidth, UseHeight, TSF_G8, bFinalUpdateResource);  break;
	case ERawImageFormat::G16:
		return InitializeSourceDataFromMip0<uint16>(Texture, UseWidth, UseHeight, TSF_G16, bFinalUpdateResource); break;
	case ERawImageFormat::BGRA8:
		return InitializeSourceDataFromMip0<FColor>(Texture, UseWidth, UseHeight, TSF_BGRA8, bFinalUpdateResource); break;
	case ERawImageFormat::BGRE8:
		return InitializeSourceDataFromMip0<FColor>(Texture, UseWidth, UseHeight, TSF_BGRE8, bFinalUpdateResource); break;
	case ERawImageFormat::RGBA16:
		return InitializeSourceDataFromMip0<uint16>(Texture, UseWidth, UseHeight, TSF_RGBA16, bFinalUpdateResource); break;
	case ERawImageFormat::RGBA16F:
		return InitializeSourceDataFromMip0<FFloat16Color>(Texture, UseWidth, UseHeight, TSF_RGBA16F, bFinalUpdateResource); break;
	case ERawImageFormat::RGBA32F:
		return InitializeSourceDataFromMip0<FLinearColor>(Texture, UseWidth, UseHeight, TSF_RGBA32F, bFinalUpdateResource); break;
	case ERawImageFormat::R16F:
		return InitializeSourceDataFromMip0<FFloat16>(Texture, UseWidth, UseHeight, TSF_R16F, bFinalUpdateResource); break;
	case ERawImageFormat::R32F:
		return InitializeSourceDataFromMip0<float>(Texture, UseWidth, UseHeight, TSF_R32F, bFinalUpdateResource); break;
	}
	return false;
}



bool GS::InitializeSourceDataFromImage(UTexture2D* Texture, const FImage& SourceImage, bool bFinalUpdateResource)
{
	int Width = SourceImage.GetWidth();
	int Height = SourceImage.GetHeight();

	ETextureSourceFormat TSFormat = FImageCoreUtils::ConvertToTextureSourceFormat(SourceImage.Format);
	
	Texture->Source.Init2DWithMipChain(Width, Height, TSFormat);
	uint8* DestData = Texture->Source.LockMip(0);
	if (!DestData)
		return false;

	int64 Mip0Size = Texture->Source.CalcMipSize(0);
	check(Mip0Size == SourceImage.RawData.Num());
	FMemory::Memcpy(DestData, &SourceImage.RawData[0], Mip0Size );

	Texture->Source.UnlockMip(0);

	if (bFinalUpdateResource)
		Texture->UpdateResource();

	return true;
}






bool GS::UpdateTextureSourceFromBuffer(TArrayView<FColor> ColorBuffer, UTexture2D* ApplyToTexture, bool bWaitForCompile)
{
	int Width = ApplyToTexture->GetPlatformData()->SizeX;
	int Height = ApplyToTexture->GetPlatformData()->SizeY;
	if (!ensure(Width*Height == ColorBuffer.Num()))
		return false;

	bool bSRGB = ApplyToTexture->SRGB;

	// create new RGBA8 source image and populate it from UpdatedBuffer
	FImage SourceImage;
	SourceImage.Init(Width, Height,
		ERawImageFormat::BGRA8, bSRGB ? EGammaSpace::sRGB : EGammaSpace::Linear);
	TArrayView64<FColor> SourceImageView = SourceImage.AsBGRA8();
	for (int k = 0; k < Width * Height; ++k)
		SourceImageView[k] = ColorBuffer[k];

	// figure out the existing texture source data format
	ETextureSourceFormat ExistingFormat = ApplyToTexture->Source.GetFormat();
	ERawImageFormat::Type ExistingRawFormat = FImageCoreUtils::ConvertToRawImageFormat(ExistingFormat);

	// if necessary, convert our RGBA8 source image to the source data format
	if (ExistingRawFormat != ERawImageFormat::BGRA8) {
		FImage TempImage;
		SourceImage.CopyTo(TempImage, ExistingRawFormat, bSRGB ? EGammaSpace::sRGB : EGammaSpace::Linear);
		SourceImage = MoveTemp(TempImage);
	}

	// update the source data in the texture
	GS::InitializeSourceDataFromImage(ApplyToTexture, SourceImage);

	// this will UpdateResource()
	ApplyToTexture->PostEditChange();

	if (bWaitForCompile)
		FTextureCompilingManager::Get().FinishCompilation({ ApplyToTexture });

	return true;
}