// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GridActor/UGSModelGridAsset.h"
#include "ModelGrid/ModelGrid.h"

namespace GS
{

GRADIENTSPACEUESCENE_API
UGSModelGridAsset* CreateModelGridAssetFromGrid(const FString& NewAssetPath, ModelGrid&& NewGrid);

}
