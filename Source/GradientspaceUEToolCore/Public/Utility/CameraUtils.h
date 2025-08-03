// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "ToolContextInterfaces.h"
#include "SceneView.h" // FViewMatrices


namespace GS
{
	// caches information from FSceneView about projection matrices, to allow world/pixel mapping when the SceneView is not available
	struct GRADIENTSPACEUETOOLCORE_API FViewProjectionInfo
	{
		// todo mirror FViewCameraState info here

		// 3D viewport camera transformation matrices
		FViewMatrices ViewMatrices;
		// view rectangle in pixel coords
		FIntRect UnscaledViewRect;


		// set internal data from SceneView
		void Initialize(const FSceneView* SceneView);
		void Initialize(IToolsContextRenderAPI* RenderAPI);


		//! map 3D point to 2D pixel coordinate
		FVector2d WorldToPixel(const FVector& WorldPoint) const;

		//! map 2D point and Z coordinate? (depth?) to 3D world position
		FVector3d PixelToWorld(const FVector2d& Pixel, double Z) const;

		//! map 2D pixel coords (where X=0,Y=0 is top-left) to "screen space" coords in range (-1,1)
		FVector4d PixelToScreen(double X, double Y, double Z) const;
		//! clamped PixelToScreen (rename?)
		FVector4d CursorToScreen(double X, double Y, double Z) const;

		//! return world ray at 2D pixel coords
		FRay GetRayAtPixel(const FVector2d& Pixel) const;

		//! check if 2D pixel position is in view rect
		bool IsVisible(const FVector2d& PixelPos) const;

		//! check if 3D world position is in view rect
		bool IsVisible(const FVector3d& WorldPoint) const;
	};

}
