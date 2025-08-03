// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreUObject.h"
#include <functional>

#include "EnumButtonsGroup.generated.h"

struct GRADIENTSPACEUETOOLCORE_API FGSEnumButtonsTarget
{
	std::function<int()> GetSelectedItemIdentifier;
	std::function<bool(int)> IsItemEnabled;
	std::function<bool(int)> IsItemVisible;

	std::function<void(int NewItemIdentifier)> SetSelectedItem;
};


struct GRADIENTSPACEUETOOLCORE_API FGSEnumButtonsGroupItem
{
	FString ItemName;
	int ItemIdentifier;

	FText UIString;
	FText TooltipText;
};


/**
 * FGSEnumButtonsGroup is intended to be used as a UProperty in an InteractiveToolPropertySet, ie for Tool Settings.
 * It is paired with a details customization that will show the EnumItems list as a set of buttons.
 * This is an alternative to an enum dropdown, for small numbers of items where it's nicer to show
 * all the options visually.
 * 
 * Note that this property does not actually store the selected-item index, it relies on a callback into the Tool.
 * As a result the selected index will not be saved/restored via SaveProperties()/RestoreProperties()
 * One way to handle this is to pair it with a serialized property, ie like so
 * 
 * 	UPROPERTY(EditAnywhere, Category = "...", meta = (TransientToolProperty))
 *	FGSEnumButtonsGroup Thing;
 *	UPROPERTY()
 *	int Thing_SelectedIndex = 0;
 */
USTRUCT()
struct GRADIENTSPACEUETOOLCORE_API FGSEnumButtonsGroup
{
	GENERATED_BODY()

	/**
	 * Inline selected index. usafe of this field is *optional* - clients can provide their own selected index
	 * via the functions in the Target. 
	 */
	UPROPERTY()
	int SelectedIndex = 0;

	// Tool must set this to a Target struct that implements the necessary functions, this will be
	// passed to each button and must live as long as the UI
	// Alternately struct can allocate it...  (is that safe? seems like binary serialization of struct-as-property could screw this up..)
	TSharedPtr<FGSEnumButtonsTarget> Target;

	// list of items that must be populated by creating Tool (can use utility functions below)
	TArray<FGSEnumButtonsGroupItem> EnumItems;


	// prevent copying non-uproperties, otherwise copies of this struct will copy the internals
	FGSEnumButtonsGroup& operator=(const FGSEnumButtonsGroup& copy)
	{
		SelectedIndex = copy.SelectedIndex;
		return *this;
	}


	void Initialize()
	{
		EnumItems.Reset();
		Target.Reset();
		SelectedIndex = 0;
	}

	TSharedPtr<FGSEnumButtonsTarget> SimpleInitialize()
	{
		EnumItems.Reset();
		Target = MakeShared<FGSEnumButtonsTarget>();
		Target->GetSelectedItemIdentifier = [this]() { return SelectedIndex; };
		Target->IsItemEnabled = [](int) { return true; };
		Target->IsItemVisible = [](int) { return true; };
		Target->SetSelectedItem = [this](int NewItem) { SelectedIndex = NewItem; };
		return Target;
	}

	void ValidateAfterSetup()
	{
		if (!EnumItems.IsValidIndex(SelectedIndex))
			SelectedIndex = 0;
	}


	void AddItem(FGSEnumButtonsGroupItem EnumItem);
	// sets index for each item sequntially, 0 to N-1
	void AddItems(const TArray<FString>& Items);
	void AddItems(const TArray<FText>& Items);



	// options for the UI  (perhaps move to a struct?)

	// true = use standard label/value layout in details panel, false = use whole-row with no label
	bool bShowLabel = true;
	// true = centered, false = left-aligned
	bool bCentered = false;
	// font size of button UI text
	int FontSize = 9;
	// padding for button text
	double PadLeft = 5.0;
	double PadRight = 5.0;
	double PadTop = 3.0;
	double PadBottom = 4.0;
	// spacing between buttons
	double ButtonSpacingHorz = 0.0;
	double ButtonSpacingVert = 0.0;
	// spacing above/below button group - only used for whole-row layout
	double GroupSpacingAbove = 3.0;
	double GroupSpacingBelow = 3.0;
	// request fixed-width buttons, calculated based on the maximum label width
	bool bCalculateFixedButtonWidth = false;
	// use a constant fixed button width, ignored if 0
	float ButtonFixedWidth = 0.0f;
	// use a grid layout, rather than a wrap-box
	bool bUseGridLayout = false;

	//! slightly larger font size (9)
	void SetUIPreset_DefaultSize() {
		FontSize = 9.0; PadLeft = PadRight = 5.0; PadTop = 3.0; PadBottom = 4.0;
	}

	//! standard details panel font size (8)
	void SetUIPreset_DetailsSize() {
		FontSize = 8.0; PadLeft = PadRight = 4.0; PadTop = 2.0; PadBottom = 3.0;
	}



	// Required by TStructOpsTypeTraits interface
	bool operator==(const FGSEnumButtonsGroup& Other) const
	{
		return SelectedIndex == Other.SelectedIndex;
	}
	bool operator!=(const FGSEnumButtonsGroup& Other) const
	{
		return !(*this == Other);
	}
};

template<>
struct TStructOpsTypeTraits<FGSEnumButtonsGroup> : public TStructOpsTypeTraitsBase2<FGSEnumButtonsGroup>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};

