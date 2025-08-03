// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"

class UMaterialGraph;
class UMaterialInterface;
class UTexture2D;
class UMaterialGraphNode;
class UMaterialExpressionTextureBase;

namespace GS
{
	GRADIENTSPACEUECOREEDITOR_API
	UMaterialGraph* GetMaterialGraphForMaterial(UMaterialInterface* Material);

	GRADIENTSPACEUECOREEDITOR_API
	bool FindTextureNodeInMaterial(
		UMaterialGraph* Graph,
		const UTexture2D* FindTexture,
		UMaterialGraphNode*& FoundNode,
		UMaterialExpressionTextureBase*& FoundExpression);

	struct GRADIENTSPACEUECOREEDITOR_API MaterialTextureInfo
	{
		const UTexture2D* Texture = nullptr;
		bool bIsOverrideParameter = false;
		FName ParameterName = NAME_None;
	};

	GRADIENTSPACEUECOREEDITOR_API
	void FindActiveTextureSetForMaterial(UMaterialInterface* Material, TArray<MaterialTextureInfo>& TexturesOut,
		bool bIncludeBaseMaterialsForMIs);
}