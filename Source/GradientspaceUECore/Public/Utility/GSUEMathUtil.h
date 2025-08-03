// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "TransformSequence.h"

#include "Math/GSTransformList.h"

namespace GS
{
	using namespace UE::Geometry;

	GRADIENTSPACEUECORE_API void ConvertToUE(const TransformListd& TransformList, FTransformSequence3d& TransformSequence);


}
