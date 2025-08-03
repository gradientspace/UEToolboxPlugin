// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreUObject.h"
#include <functional>
#include "ActionButtonGroup.generated.h"


struct GRADIENTSPACEUETOOLCORE_API FGSActionButtonTarget
{
	// required
	std::function<void(FString CommandName)> ExecuteCommand;

	// optional, defaults to true if not available
	std::function<bool(FString)> IsCommandEnabled;
	std::function<bool(FString)> IsCommandVisible;
};


struct GRADIENTSPACEUETOOLCORE_API FGSActionButtonGroupItem
{
	FString Command;
	FText UIString;
	FText TooltipText;

	FColor CustomColor = FColor::White;
	bool bHaveCustomColor = false;
};


USTRUCT()
struct GRADIENTSPACEUETOOLCORE_API FGSActionButtonGroup
{
	GENERATED_BODY()

	// Tool must set this to a Target struct that implements the necessary functions, this will be
	// passed to each button and must live as long as the UI
	TSharedPtr<FGSActionButtonTarget> Target;

	// list of items that must be populated by creating Tool (can use utility functions below)
	TArray<FGSActionButtonGroupItem> Actions;

	// helper functions for adding Actions
	bool AddAction(FString Command, FText UIString, FText TooltipText);
	bool AddAction(FString Command, FText UIString, FText TooltipText, FColor CustomColor);


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

	//! slightly larger font size (9)
	void SetUIPreset_DefaultSize() {
		FontSize = 9.0; PadLeft = PadRight = 5.0; PadTop = 3.0; PadBottom = 4.0;
	}

	//! larger font size (10)
	void SetUIPreset_LargeSize() {
		FontSize = 10.0; PadLeft = PadRight = 6.0; PadTop = 4.0; PadBottom = 5.0;
	}

	//! standard details panel font size (8)
	void SetUIPreset_DetailsSize() {
		FontSize = 8.0; PadLeft = PadRight = 4.0; PadTop = 2.0; PadBottom = 3.0;
	}

	//! larger font size (10)
	void SetUIPreset_LargeActionButton() {
		FontSize = 12.0; bBold = true; PadLeft = PadRight = 12.0; PadTop = 4.0; PadBottom = 5.0; GroupSpacingAbove = 6.0f;
	}

};
