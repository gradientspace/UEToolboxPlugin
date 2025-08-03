// Copyright Gradientspace Corp. All Rights Reserved.

#include "Utility/GridToolUtil.h"
#include "ToolContextInterfaces.h"
#include "ToolDataVisualizer.h"

using namespace GS;
using namespace UE::Geometry;

void GS::GridToolUtil::DrawGridOriginCell(
	IToolsContextRenderAPI* RenderAPI,
	const FFrame3d& GridFrame,
	ModelGrid& Grid)
{
	FToolDataVisualizer Draw;
	Draw.BeginFrame(RenderAPI);

	Draw.PushTransform(GridFrame.ToFTransform());

	AxisBox3d OriginBox = Grid.GetCellLocalBounds(Vector3i::Zero());
	Vector3d Dimensions = OriginBox.Diagonal();
	Draw.DrawWireBox((FBox)OriginBox, FLinearColor::Red, 1.0f, false);

	Vector3d Origin = OriginBox.Min;
	Draw.DrawPoint((FVector)Origin, FLinearColor::Red, 10.0f, false);

	double AxisLength = 2.0 * GS::Max(Dimensions.X, GS::Max(Dimensions.Y, Dimensions.Z));
	Draw.DrawLine((FVector)Origin, (FVector)(Origin + AxisLength * Vector3d::UnitX()), FLinearColor::Red, 2.0, false);
	Draw.DrawLine((FVector)Origin, (FVector)(Origin + AxisLength * Vector3d::UnitY()), FLinearColor::Green, 2.0, false);
	Draw.DrawLine((FVector)Origin, (FVector)(Origin + AxisLength * Vector3d::UnitZ()), FLinearColor::Blue, 2.0, false);

	Draw.EndFrame();
}

