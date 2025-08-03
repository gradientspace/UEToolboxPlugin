// Copyright Gradientspace Corp. All Rights Reserved.
#include "Utility/CameraUtils.h"

void GS::FViewProjectionInfo::Initialize(IToolsContextRenderAPI* RenderAPI)
{
	Initialize(RenderAPI->GetSceneView());
}

void GS::FViewProjectionInfo::Initialize(const FSceneView* SceneView)
{
	ViewMatrices = SceneView->ViewMatrices;
	UnscaledViewRect = SceneView->UnscaledViewRect;
}


FVector2d GS::FViewProjectionInfo::WorldToPixel(const FVector& WorldPoint) const
{
	// hmm there is static function FSceneView::ProjectWorldToScreen...

	// project World point to Screen, via Projection matrix
	FVector4 HWorldPoint(WorldPoint, 1);
	FVector4 ScreenPoint = ViewMatrices.GetViewProjectionMatrix().TransformFVector4(HWorldPoint);

	if (ScreenPoint.W == 0.0)
		return FVector2d::Zero();

	// convert Screen point to Pixel coords
	double InvW = (ScreenPoint.W > 0.0 ? 1.0 : -1.0) / ScreenPoint.W;
	return FVector2d(
		(double)UnscaledViewRect.Min.X + (0.5 + ScreenPoint.X*0.5*InvW) * (double)UnscaledViewRect.Width(),
		(double)UnscaledViewRect.Min.Y + (0.5 - ScreenPoint.Y*0.5*InvW) * (double)UnscaledViewRect.Height());
}

bool GS::FViewProjectionInfo::IsVisible(const FVector2d& PixelPos) const
{
	if (PixelPos.X <= (double)UnscaledViewRect.Min.X || PixelPos.X >= (double)UnscaledViewRect.Max.X)
		return false;
	if (PixelPos.Y <= (double)UnscaledViewRect.Min.Y || PixelPos.Y >= (double)UnscaledViewRect.Max.Y)
		return false;
	return true;
}

bool GS::FViewProjectionInfo::IsVisible(const FVector3d& WorldPoint) const
{
	return IsVisible(WorldToPixel(WorldPoint));
}

FVector3d GS::FViewProjectionInfo::PixelToWorld(const FVector2d& Pixel, double Z) const
{
	// maybe look at FSceneView::DeprojectScreenToWorld static function...

	ensure(false);		// need to test this as it's not clear it's working, or what Z should be...

	//const FVector4 ScreenPoint = PixelToScreen(X, Y, Z);
	FVector4 ScreenPoint(
		-1.0f + Pixel.X / UnscaledViewRect.Width() * (2.0f),
		+1.0f + Pixel.Y / UnscaledViewRect.Height() * (-2.0f),
		Z, 1);

	//return ScreenToWorld(ScreenPoint);
	FVector4 Projected = ViewMatrices.GetInvViewProjectionMatrix().TransformFVector4(ScreenPoint);
	return FVector3d(Projected.X, Projected.Y, Projected.Z);
}


FVector4 GS::FViewProjectionInfo::PixelToScreen(double X, double Y, double Z) const
{
	double x = 0, y = 0;
	if (GProjectionSignY > 0.0f) {
		x = (-1.0) + (X / (double)UnscaledViewRect.Width()) * 2.0;
		y = 1.0 + (Y / (double)UnscaledViewRect.Height()) * (-2.0);
	}
	else {  // is this ever true? I don't think so...
		x = (-1.0) + (X / (double)UnscaledViewRect.Width()) * 2.0;
		y = 1.0 - (+1.0 + Y / (double)UnscaledViewRect.Height() * (-2.0));
	}
	return FVector4d(x, y, Z, 1.0);
}

FVector4d GS::FViewProjectionInfo::CursorToScreen(double X, double Y, double Z) const
{
	double ClampedX = FMath::Clamp(X - (double)UnscaledViewRect.Min.X, 0.0, (double)UnscaledViewRect.Width());
	double ClampedY = FMath::Clamp(Y - (double)UnscaledViewRect.Min.Y, 0.0, (double)UnscaledViewRect.Height());
	return PixelToScreen(ClampedX, ClampedY, Z);
}


FRay GS::FViewProjectionInfo::GetRayAtPixel(const FVector2d& Pixel) const
{
	// screen XY coords are in scaled [-1,1] space
	FVector4d ScreenPos = CursorToScreen(Pixel.X, Pixel.Y, 0.0);

	const FMatrix& InverseViewMatrix = ViewMatrices.GetInvViewMatrix();
	const FMatrix& InverseProjectionMatrix = ViewMatrices.GetInvProjectionMatrix();

	const bool bIsPerspective = ViewMatrices.IsPerspectiveProjection();
	if (bIsPerspective)
	{
		FVector3d Origin = ViewMatrices.GetViewOrigin();
		FVector4d ClipPos = FVector4(ScreenPos.X * (double)GNearClippingPlane, ScreenPos.Y * (double)GNearClippingPlane, 0.0, (double)GNearClippingPlane);
		FVector4d InverseProjected = InverseProjectionMatrix.TransformFVector4(ClipPos);
		FVector3d Direction = InverseViewMatrix.TransformVector(FVector(InverseProjected));
		return FRay(Origin, Direction.GetSafeNormal());
	}
	else
	{
		FVector4d ClipPos(ScreenPos.X, ScreenPos.Y, 0.5, 1.0);
		FVector4d InverseProjected = InverseProjectionMatrix.TransformFVector4(ClipPos);
		FVector3d Origin = (FVector)InverseViewMatrix.TransformFVector4(InverseProjected);
		FVector3d Direction = InverseViewMatrix.TransformVector( FVector::UnitZ() );

		// TODO: in modeling mode, in ortho, ray origin is translated because the above code basically places it at arbitary Z
		// should we do that here?
		//RayOrigin -= 0.1 * HALF_WORLD_MAX * RayDirection;

		return FRay(Origin, Direction.GetSafeNormal());
	}
}



