// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreUObject.h"
#include "Math/Color.h"
#include "Core/UEVersionCompat.h"
#include "GSColorPalette.generated.h"


struct GRADIENTSPACEUETOOLCORE_API FGSColorPaletteTarget
{
	TFunction<void(FLinearColor NewColor, bool bShiftDown)> SetSelectedColor = [](FLinearColor, bool) {};
	TFunction<void(FLinearColor NewColor)> RemovePaletteColor = [](FLinearColor) {};
};


USTRUCT()
struct GRADIENTSPACEUETOOLCORE_API FGSColorPalette
{
	GENERATED_BODY()

	FGSColorPalette() {
		Colors.Add(FLinearColor::Black);
		Colors.Add(FLinearColor::White);

		Colors.Add(FLinearColor(.75f,.75f,.75f));
		Colors.Add(FLinearColor(.5f, .5f, .5f));
		Colors.Add(FLinearColor(.25f, .25f, .25f));
		Colors.Add(FLinearColor::Red);
		Colors.Add(FLinearColor::Green);
		Colors.Add(FLinearColor::Blue);
		Colors.Add(FLinearColor(1.0f, 1.0f, 0.0f));
		Colors.Add(FLinearColor(1.0f, 0.0f, 1.0f));
		Colors.Add(FLinearColor(0.0f, 1.0f, 1.0f));
		Colors.Add(FLinearColor(FColor::Orange));
		Colors.Add(FLinearColor(FColor::Purple));
		Colors.Add(FLinearColor(FColor::Turquoise));
		Colors.Add(FLinearColor(FColor::Emerald));
	}

	UPROPERTY(EditAnywhere, Category="Color Palette")
	TArray<FLinearColor> Colors;
	
	// target and events
	TSharedPtr<FGSColorPaletteTarget> Target;

	DECLARE_MULTICAST_DELEGATE_OneParam(OnPaletteModifiedDelegate, const FGSColorPalette* Palette);
	OnPaletteModifiedDelegate OnPaletteModified;


	// styling options
	int ButtonSize = 15;


	// util functions

	void AddNewPaletteColor(FLinearColor NewColor) {
		// ignore duplicates
		FColor IntAddColor = NewColor.ToFColor(false);
		for (FLinearColor Color : Colors) {
			if (Color.ToFColor(false) == IntAddColor)
				return;
		}

		Colors.Insert(NewColor, 2);
		while (Colors.Num() > 30)
			GSUE::TArrayPop(Colors);
		OnPaletteModified.Broadcast(this);
	}

	void RemovePaletteColor(FLinearColor RemoveColor) {
		bool bRemoved = false;
		FColor IntRemoveColor = RemoveColor.ToFColor(false);
		for (int k = 2; k < Colors.Num() && !bRemoved; ++k)
		{
			if (Colors[k].ToFColor(false) == IntRemoveColor) {
				Colors.RemoveAt(k);
				bRemoved = true;
			}
		}
		if (bRemoved)
			OnPaletteModified.Broadcast(this);
	}
};