// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "ModelGrid/ModelGrid.h"
#include "FrameTypes.h"

class IToolsContextRenderAPI;

namespace UE::Geometry { class FDynamicMesh3; }

namespace GS::GridToolUtil
{

GRADIENTSPACEUETOOLCORE_API
void DrawGridOriginCell(
	IToolsContextRenderAPI* RenderAPI,
	const UE::Geometry::FFrame3d& GridFrame,
	GS::ModelGrid& Grid);



}
