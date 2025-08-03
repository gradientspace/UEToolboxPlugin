// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"
#include "UObject/ObjectMacros.h"
#include "Math/IntVector.h"

#include "GradientspaceScriptTypes.generated.h"

/**
 * GSIntBox3 is an integer bounding box.
 * Default-initialize to zero-coordinates for both Min and Max.
 */
USTRUCT(BlueprintType)
struct GRADIENTSPACESCRIPT_API FGSIntBox3
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IntBox3")
	FIntVector Min = FIntVector(0,0,0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IntBox3")
	FIntVector Max = FIntVector(0, 0, 0);
};

