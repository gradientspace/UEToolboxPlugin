// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "HotSpotTexturingFunctions.generated.h"

UCLASS(meta = (ScriptName = "GradientspaceScript_HotSpotTexturing"), MinimalAPI)
class UGradientspaceScriptLibrary_HotSpotTexturing : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Appends 
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Gradientspace|HotSpotTexturing", meta = (ScriptMethod))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	AppendHotSpotRectangleGridV1(
		UDynamicMesh* TargetMesh,
		float PixelDimension = 1024,
		int32 Subdivisions = 5,
		float PaddingPixelWidth = 1,
		bool bFlipX = false,
		bool bFlipY = false,
		bool bRepeatLast = false,
		UGeometryScriptDebug* Debug = nullptr );


};
