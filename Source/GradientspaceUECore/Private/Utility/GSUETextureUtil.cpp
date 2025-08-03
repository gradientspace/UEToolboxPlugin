// Copyright Gradientspace Corp. All Rights Reserved.
#include "Utility/GSUETextureUtil.h"
#include "IntVectorTypes.h"

#include "Sampling/ImageSampling.h"

using namespace UE::Geometry;

bool GS::SampleTextureBufferAtUV(
	const FColor* ColorBuffer,
	int ImageWidth, int ImageHeight,
	FVector2d UVCoords,
	FColor& ResultColor,
	bool bUseNearestSampling)
{
	ResultColor = FColor::Black;
	if (UVCoords.X < 0 || UVCoords.X > 1 || UVCoords.Y < 0 || UVCoords.Y > 1)
		return false;

	FVector2d UVPixel(UVCoords.X * (double)ImageWidth, UVCoords.Y * (double)ImageHeight);

	bool bFound = false;
	if (bUseNearestSampling) {
		FVector2i Pixel((int)UVPixel.X, (int)UVPixel.Y);
		int LinearIndex = Pixel.Y * ImageWidth + Pixel.X;
		ResultColor = ColorBuffer[LinearIndex];
		bFound = true;
	}
	else {
		bFound = GS::BilinearSampleCenteredGrid2<uint8_t, 4>((uint8_t*)ColorBuffer, ImageWidth, ImageHeight, UVPixel, (uint8_t*)&ResultColor.Bits);
	}
	
	return bFound;
}