// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"
#include "Containers/UnrealString.h"

class UObject;
class UMaterialInterface;
class UTexture2D;

namespace GS
{
	GRADIENTSPACEUECORE_API
	bool AssignMaterialToTarget(UMaterialInterface* Material, int MaterialIndex, UObject* ActorOrComponent, UObject* Asset = nullptr, bool bUpdateAssetAndComponent = true);


	GRADIENTSPACEUECORE_API
	bool SetMaterialTextureParameter(UMaterialInterface* Material, const FString& ParameterName, UTexture2D* Texture);

	GRADIENTSPACEUECORE_API
	void DoInternalMaterialUpdates(UMaterialInterface* Material);

}