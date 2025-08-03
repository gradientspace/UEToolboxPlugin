// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"
#include "ImageCore.h"

class UTexture2D;

namespace GS
{

/**
 * Update the Source data in the provided Texture from the existing PlatformData
 * The Source format of the Texture will be updated to whatever SourceFormat is
 * @param bFinalUpdateResource if true, Texture->UpdateResource() will be called to rebuild platform data. If you are doing this yourself, pass false to avoid wasting work
 */
GRADIENTSPACEUECOREEDITOR_API
bool InitializeSourceDataFromMip0(UTexture2D* Texture, int Width, int Height, ERawImageFormat::Type SourceFormat, bool bFinalUpdateResource = true);

/**
 * Update the Source data in the provided Texture from the SourceImage
 * The Source format of the Texture will be updated to whatever the SourceImage.Format is
 * @param bFinalUpdateResource if true, Texture->UpdateResource() will be called to rebuild platform data. If you are doing this yourself, pass false to avoid wasting work
 */
GRADIENTSPACEUECOREEDITOR_API
bool InitializeSourceDataFromImage(UTexture2D* Texture, const FImage& SourceImage, bool bFinalUpdateResource = true);


/**
 * Update the source image in TextureToUpdate from the provided ColorBuffer.
 * Will convert from RGBA8 to whatever the existing source format of TextureToUpdate is.
 */
GRADIENTSPACEUECOREEDITOR_API
bool UpdateTextureSourceFromBuffer(TArrayView<FColor> ColorBuffer, UTexture2D* TextureToUpdate, bool bWaitForCompile = true);



};