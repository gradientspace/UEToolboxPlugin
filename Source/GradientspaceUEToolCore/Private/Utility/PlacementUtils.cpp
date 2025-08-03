// Copyright Gradientspace Corp. All Rights Reserved.
#include "Utility/PlacementUtils.h"

#include "Math/GSAxisBox3.h"

#include "FrameTypes.h"
#include "VectorTypes.h"

// for initial hit, to move
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Landscape.h"

using namespace UE::Geometry;
using namespace GS;


bool GS::FindVerticalGroundHit(const FVector3d& WorldPosition, UWorld* TargetWorld, FVector3d& HitPosition, FVector3d& HitNormal, bool bFallbackToGroundPlane)
{
	FRay3d WorldRay(WorldPosition + (double)Mathf::SafeMaxExtent() * FVector3d::UnitZ(), -FVector3d::UnitZ());
	return GS::FindRayGroundHit(WorldRay, TargetWorld, HitPosition, HitNormal, bFallbackToGroundPlane);
}

bool GS::FindRayGroundHit(const FRay3d& WorldRay, UWorld* TargetWorld, FVector3d& HitPosition, FVector3d& HitNormal, bool bFallbackToGroundPlane)
{
	FVector LineTraceStart = WorldRay.Origin;
	FVector LineTraceEnd = WorldRay.PointAt(TNumericLimits<float>::Max());

	double HitDistance = TNumericLimits<double>::Max();

	FCollisionQueryParams QueryParams;
	QueryParams.MobilityType = EQueryMobilityType::Static;

	// try to hit landscape
	for (TActorIterator<ALandscape> LandscapeIterator(TargetWorld); LandscapeIterator; ++LandscapeIterator) {
		ALandscape* Landscape = *LandscapeIterator;
		FHitResult HitResult;
		if (Landscape->ActorLineTraceSingle(HitResult, LineTraceStart, LineTraceEnd, ECollisionChannel::ECC_WorldStatic, QueryParams)) {
			if (HitResult.Distance < HitDistance) {
				HitDistance = HitResult.Distance;
				HitNormal = HitResult.Normal;
			}
		}
	}

	// also check landscape proxies (in WP world landscape will be split into proxies?)
	for (TActorIterator<ALandscapeProxy> ProxyIterator(TargetWorld); ProxyIterator; ++ProxyIterator) {
		ALandscapeProxy* Landscape = *ProxyIterator;
		FHitResult HitResult;
		if (Landscape->ActorLineTraceSingle(HitResult, LineTraceStart, LineTraceEnd, ECollisionChannel::ECC_WorldStatic, QueryParams)) {
			if (HitResult.Distance < HitDistance) {
				HitDistance = HitResult.Distance;
				HitNormal = HitResult.Normal;
			}
		}
	}


	if (HitDistance == TNumericLimits<double>::Max() && bFallbackToGroundPlane)
	{
		FFrame3d GroundPlane(FVector3d::Zero(), FVector3d::UnitZ());
		FVector GroundHit;
		if (GroundPlane.RayPlaneIntersection(WorldRay.Origin, WorldRay.Direction, 2, GroundHit))
		{
			HitDistance = WorldRay.GetParameter(GroundHit);
			HitNormal = FVector3d::UnitZ();
		}
	}

	if (HitDistance < TNumericLimits<double>::Max()) {
		HitPosition = WorldRay.PointAt(HitDistance);
		return true;
	}
	return false;
}


double GS::ComputeVisibilityMetric(
	const GS::AxisBox3d& WorldBounds,
	const GS::FViewProjectionInfo& ProjectionInfo)
{
	double Accum = 0;
	double Sum = 0;

	const double CenterWeight = 5.0;
	const double CornerWeight = 1.0;
	const double FaceWeight = 1.0;

	if (ProjectionInfo.IsVisible(WorldBounds.Center()))
		Accum += CenterWeight;

	for (int i = 0; i < 8; ++i)	{
		if (ProjectionInfo.IsVisible(WorldBounds.BoxCorner(i)))
			Accum += CornerWeight;
	}

	for (int i = 0; i < 6; ++i) {
		if (ProjectionInfo.IsVisible(WorldBounds.FaceCenter(i)))
			Accum += FaceWeight;
	}

	// todo: edges?

	return Accum / (CenterWeight + 8*CornerWeight + 6*FaceWeight);
}



FVector3d GS::FindInitialPlacementPositionFromViewInfo(
	const FViewCameraState& ViewState, const GS::FViewProjectionInfo& ProjectionInfo,
	UWorld* TargetWorld, double PlacementRadius,
	TFunctionRef<FVector(FVector)> WorldGridSnappingFunc)
{
	Vector3d Center = FVector3d::Zero();
	AxisBox3d UseBounds(Center, PlacementRadius);
	return GS::FindInitialPlacementPositionFromViewInfo(ViewState, ProjectionInfo, TargetWorld,
		UseBounds, UseBounds.BaseZ(), WorldGridSnappingFunc);
}

FVector3d GS::FindInitialPlacementPositionFromViewInfo(
	const FViewCameraState& ViewState, const GS::FViewProjectionInfo& ProjectionInfo,
	UWorld* TargetWorld, 
	const GS::AxisBox3d& WorldBounds,
	const GS::Vector3d& PlacementOrigin,
	TFunctionRef<FVector(FVector)> WorldGridSnappingFunc)
{
	FFrame3d ViewFrame(ViewState.Position, ViewState.Orientation);

	double PlacementRadius = WorldBounds.Diagonal().Length() / 2.0;
	FVector3d DepthShift = PlacementRadius * ViewFrame.X();		// X is forward

	auto PlacedBounds = [&](Vector3d HitPosition)
	{
		return WorldBounds.Translated( HitPosition - PlacementOrigin + DepthShift);
	};

	bool bHaveGroundHit = false;
	FVector3d EyeRayHitPosition = FVector3d::Zero(), EyeRayHitNormal = FVector3d::UnitZ();
	double ViewMetric = 0;

	double LastImprovement = 0;
	for (int k = 45; k >= 0; --k)
	{
		FFrame3d TiltedFrame = ViewFrame;
		TiltedFrame.Rotate(FQuaterniond(ViewState.Right(), 1.0 * (double)k, true));
		FRay TiltedWorldRay(TiltedFrame.Origin, TiltedFrame.X(), true);
		FVector3d CurHitPosition, CurHitNormal;
		bool bHaveCurHit = FindRayGroundHit(TiltedWorldRay, TargetWorld, CurHitPosition, CurHitNormal);
		if (bHaveCurHit)
		{
			double NewViewMetric = (bHaveCurHit) ? ComputeVisibilityMetric( PlacedBounds(CurHitPosition), ProjectionInfo) : 0.0;
			if (NewViewMetric <= ViewMetric) continue;

			double Improvement = (ViewMetric > 0) ? (NewViewMetric / ViewMetric) : 9999;
			if (LastImprovement > 0 && (LastImprovement / Improvement) < 0.25)
				break;
			LastImprovement = Improvement;

			bHaveGroundHit = true;
			ViewMetric = NewViewMetric;
			EyeRayHitPosition = CurHitPosition;
			EyeRayHitNormal = CurHitNormal;

			if (ViewMetric >= 1.0)
				break;
		}
	}


	// if we did not get a ground hit, try tracing directly down towards ground at a distance forward from the camera
	if (!bHaveGroundHit)
	{
		double ForwardDist = 5 * PlacementRadius;			// todo be smarter here...
		FVector3d ForwardPos = ViewFrame.Origin + ForwardDist * ViewFrame.X();
		bHaveGroundHit = FindVerticalGroundHit(ForwardPos, TargetWorld, EyeRayHitPosition, EyeRayHitNormal);
	}

	// Want to bump forward by placement radius (really should do this incrementally until all bbox corners are visible...).
	// Need to re-solve for ground hit (by tracing ray downwards)
	if (bHaveGroundHit)
	{
		FVector3d ForwardPos = EyeRayHitPosition + DepthShift;
		FindVerticalGroundHit(ForwardPos, TargetWorld, EyeRayHitPosition, EyeRayHitNormal);
	}

	if (!bHaveGroundHit) {
		EyeRayHitPosition = FVector3d::Zero();
		EyeRayHitNormal = FVector3d::UnitZ();
	}

	// snap to grid if necessary
	FVector3d SnappedPos = WorldGridSnappingFunc(EyeRayHitPosition);
	EyeRayHitPosition = SnappedPos;

	//HitNormal = GroundHitNormal;
	return EyeRayHitPosition;
}
