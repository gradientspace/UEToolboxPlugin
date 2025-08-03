// Copyright Gradientspace Corp. All Rights Reserved.
#include "PropertyTypes/EnumButtonsGroup.h"



void FGSEnumButtonsGroup::AddItem(FGSEnumButtonsGroupItem EnumItem)
{
	//  todo sanity check

	EnumItems.Add(EnumItem);
}


void FGSEnumButtonsGroup::AddItems(const TArray<FString>& Items)
{
	for (int i = 0; i < Items.Num(); ++i)
	{
		FGSEnumButtonsGroupItem Item;
		Item.ItemName = Items[i];
		Item.ItemIdentifier = i;
		Item.UIString = FText::FromString(Items[i]);
		Item.TooltipText = FText::FromString(Items[i]);
		AddItem(Item);
	}
}


void FGSEnumButtonsGroup::AddItems(const TArray<FText>& Items)
{
	for ( int i = 0; i < Items.Num(); ++i)
	{
		FGSEnumButtonsGroupItem Item;
		Item.ItemName = Items[i].ToString();
		Item.ItemIdentifier = i;
		Item.UIString = Items[i];
		Item.TooltipText = Items[i];
		AddItem(Item);
	}
}
