// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "ToolContextInterfaces.h"
#include "Math/GSAxisBox3.h"
#include "Utility/CameraUtils.h"

class UWorld;

namespace GS
{

GRADIENTSPACEUETOOLCORE_API
bool FindRayGroundHit(const FRay3d& WorldRay, UWorld* TargetWorld, FVector3d& HitPosition, FVector3d& HitNormal, bool bFallbackToGroundPlane = true);

GRADIENTSPACEUETOOLCORE_API
bool FindVerticalGroundHit(const FVector3d& WorldPosition, UWorld* TargetWorld, FVector3d& HitPosition, FVector3d& HitNormal, bool bFallbackToGroundPlane = true);


GRADIENTSPACEUETOOLCORE_API
FVector3d FindInitialPlacementPositionFromViewInfo(
	const FViewCameraState& ViewState,
	const GS::FViewProjectionInfo& ProjectionInfo,
	UWorld* TargetWorld,
	double PlacementRadius,
	TFunctionRef<FVector(FVector)> WorldGridSnappingFunc);

GRADIENTSPACEUETOOLCORE_API
FVector3d FindInitialPlacementPositionFromViewInfo(
	const FViewCameraState& ViewState,
	const GS::FViewProjectionInfo& ProjectionInfo,
	UWorld* TargetWorld,
	const GS::AxisBox3d& WorldBounds,
	const GS::Vector3d& PlacementOrigin,
	TFunctionRef<FVector(FVector)> WorldGridSnappingFunc);

GRADIENTSPACEUETOOLCORE_API
double ComputeVisibilityMetric(
	const GS::AxisBox3d& WorldBounds,
	const GS::FViewProjectionInfo& ProjectionInfo);

}

