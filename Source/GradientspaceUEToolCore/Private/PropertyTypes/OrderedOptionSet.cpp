// Copyright Gradientspace Corp. All Rights Reserved.
#include "PropertyTypes/OrderedOptionSet.h"


void FGSOrderedOptionSet::AddOptions(const TArray<FText>& Labels)
{
	for (const FText& Label : Labels)
	{
		Options.Add( FOption{ Label.ToString() } );
	}
}

void FGSOrderedOptionSet::AddOptions(const TArray<FString>& Labels)
{
	for (const FString& Label : Labels)
	{
		Options.Add( FOption{ Label } );
	}
}

void FGSOrderedOptionSet::ReplaceOptions(const TArray<FString>& NewLabels)
{
	Options.Reset();
	for (const FString& Label : NewLabels)
	{
		Options.Add(FOption{ Label });
	}
	OnOptionSetModified.Broadcast();
}

int FGSOrderedOptionSet::GetMaxValidIndex() const
{
	return Options.Num() - 1;
}

FString FGSOrderedOptionSet::GetLabelForIndex(int Index) const
{
	Index = FMath::Clamp(Index, 0, GetMaxValidIndex());
	return Options[Index].Label;
}
