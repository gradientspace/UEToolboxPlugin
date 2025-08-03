// Copyright Gradientspace Corp. All Rights Reserved.

#include "PropertyTypes/ToggleButtonGroup.h"

bool FGSToggleButtonGroup::AddToggle(FString ToggleName, FText UIString, FText TooltipText)
{
	for (const FGSToggleButtonGroupItem& Toggle : Items)
	{
		if (Toggle.ToggleName.Equals(ToggleName))
		{
			UE_LOG(LogTemp, Warning, TEXT("[FGSToggleButtonGroup::AddToggle] tried to add existing ToggleName string %s"), *ToggleName);
			return false;
		}
	}

	FGSToggleButtonGroupItem NewItem;
	NewItem.ToggleName = ToggleName;
	NewItem.UIString = UIString;
	NewItem.TooltipText = TooltipText;
	Items.Add(MoveTemp(NewItem));
	return true;
}


