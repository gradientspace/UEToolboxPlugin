// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreUObject.h"
#include "OrderedOptionSet.generated.h"


// TODO: perhaps should use a function-struct like FGSEnumButtonsGroup, instead of delegate
// and internal SelectedIndex value...


// this struct is meant to be used as a UProperty where you want a slider that lets you
// scrub through a sequential list of values that have explicit labels. A details customization
// is used create that UI, so the struct contents are not exposed as UProperties
USTRUCT()
struct GRADIENTSPACEUETOOLCORE_API FGSOrderedOptionSet
{
	GENERATED_BODY()

	struct FOption
	{
		FString Label;
		// could support custom values here...would eg allow reordering/etc
	};
	TArray<FOption> Options;

	int SelectedIndex = 0;

	void AddOptions(const TArray<FText>& Labels);
	void AddOptions(const TArray<FString>& Labels);

	void ReplaceOptions(const TArray<FString>& NewLabels);

	int GetMaxValidIndex() const;
	FString GetLabelForIndex(int Index) const;

	FSimpleMulticastDelegate OnOptionSetModified;
};
