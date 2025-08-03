// Copyright Gradientspace Corp. All Rights Reserved.
#include "Utility/GSUEMathUtil.h"

using namespace UE::Geometry;
using namespace GS;


void GS::ConvertToUE(const TransformListd& TransformList, FTransformSequence3d& TransformSequence)
{
	TransformList.EnumerateTransforms([&TransformSequence](const Vector3d& Translate, const Vector3d& Scale, const Quaterniond& Rotation)
	{
		FTransformSRT3d Transform((FQuaterniond)Rotation, (FVector)Translate, (FVector)Scale);
		TransformSequence.Append(Transform);
	});
}
