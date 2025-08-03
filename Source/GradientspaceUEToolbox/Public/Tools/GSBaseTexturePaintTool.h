// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Tools/BaseTextureEditTool.h"
#include "PropertyTypes/GSColorPalette.h"

#include "GSBaseTexturePaintTool.generated.h"

UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGSTextureToolColorSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Color")
	FLinearColor PaintColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, Category = "Color")
	FLinearColor EraseColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Color", meta = (NoResetToDefault))
	FGSColorPalette Palette;
};

