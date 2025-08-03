// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreUObject.h"
#include <functional>

#include "ToggleButtonGroup.generated.h"

struct GRADIENTSPACEUETOOLCORE_API FGSToggleButtonTarget
{
	// required
	std::function<bool(FString ToggleName)> GetToggleValue;
	std::function<void(FString ToggleName, bool bNewValue)> SetToggleValue;

	// optional, defaults to true if not available
	std::function<bool(FString)> IsToggleEnabled;
	std::function<bool(FString)> IsToggleVisible;
};


struct GRADIENTSPACEUETOOLCORE_API FGSToggleButtonGroupItem
{
	FString ToggleName;
	FText UIString;
	FText TooltipText;
};


USTRUCT()
struct GRADIENTSPACEUETOOLCORE_API FGSToggleButtonGroup
{
	GENERATED_BODY()

	// Tool must set this to a Target struct that implements the necessary functions, this will be
	// passed to each button and must live as long as the UI
	TSharedPtr<FGSToggleButtonTarget> Target;

	TArray<FGSToggleButtonGroupItem> Items;

	bool AddToggle(FString ToggleName, FText UIString, FText TooltipText);



	// true = use standard label/value layout in details panel, false = use whole-row with no label
	bool bShowLabel = true;
	// true = centered, false = left-aligned
	bool bCentered = false;
	// font size of button UI text
	int FontSize = 9;
	bool bBold = false;
	// padding for button text
	double PadLeft = 5.0;
	double PadRight = 5.0;
	double PadTop = 3.0;
	double PadBottom = 4.0;
	// spacing between buttons
	double ButtonSpacingHorz = 2.0;
	double ButtonSpacingVert = 2.0;
	// spacing above/below button group - only used for whole-row layout
	double GroupSpacingAbove = 3.0;
	double GroupSpacingBelow = 3.0;
	// request fixed-width buttons, calculated based on the maximum label width
	bool bCalculateFixedButtonWidth = false;
	// use a constant fixed button width, ignored if 0
	float ButtonFixedWidth = 0.0f;
	// use a grid layout, rather than a wrap-box
	bool bUseGridLayout = false;
};

