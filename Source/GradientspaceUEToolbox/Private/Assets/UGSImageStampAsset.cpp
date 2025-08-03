// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Assets/UGSImageStampAsset.h"


UGSTextureImageStampAsset::UGSTextureImageStampAsset()
{
	Stamp = CreateDefaultSubobject<UGSTextureImageStamp>(TEXT("TextureStamp"));
}