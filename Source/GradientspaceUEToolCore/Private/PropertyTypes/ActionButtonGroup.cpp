// Copyright Gradientspace Corp. All Rights Reserved.

#include "PropertyTypes/ActionButtonGroup.h"

bool FGSActionButtonGroup::AddAction(FString Command, FText UIString, FText TooltipText)
{
	for (const FGSActionButtonGroupItem& Action : Actions)
	{
		if (Action.Command.Equals(Command))
		{
			UE_LOG(LogTemp, Warning, TEXT("[FGSActionButtonGroup::AddAction] tried to add existing Command string %s"), *Command);
			return false;
		}
	}

	FGSActionButtonGroupItem NewItem;
	NewItem.Command = Command;
	NewItem.UIString = UIString;
	NewItem.TooltipText = TooltipText;
	Actions.Add(MoveTemp(NewItem));
	return true;
}


bool FGSActionButtonGroup::AddAction(FString Command, FText UIString, FText TooltipText, FColor CustomColor)
{
	if (AddAction(Command, UIString, TooltipText)) {
		Actions.Last().bHaveCustomColor = true;
		Actions.Last().CustomColor = CustomColor;
		return true;
	}
	return false;
}
