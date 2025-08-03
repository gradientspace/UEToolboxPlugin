// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Math/Color.h"
#include "Math/Vector2D.h"

namespace GS
{

GRADIENTSPACEUECORE_API
bool SampleTextureBufferAtUV(
	const FColor* ColorBuffer,
	int ImageWidth, int ImageHeight,
	FVector2d UVCoords,
	FColor& ResultColor,
	bool bUseNearestSampling = false);



}