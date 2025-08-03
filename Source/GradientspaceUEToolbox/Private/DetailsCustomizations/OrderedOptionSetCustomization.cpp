// Copyright Gradientspace Corp. All Rights Reserved.
#include "DetailsCustomizations/OrderedOptionSetCustomization.h"

#include "PropertyTypes/OrderedOptionSet.h"

#include "DetailWidgetRow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "SOptionSlider.h"

#define LOCTEXT_NAMESPACE "FGSOrderedOptionSetCustomization"

TSharedRef<IPropertyTypeCustomization> FGSOrderedOptionSetCustomization::MakeInstance()
{
	return MakeShareable(new FGSOrderedOptionSetCustomization);
}

void FGSOrderedOptionSetCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() < 1) return;
	FGSOrderedOptionSet* OptionSetProperty = (FGSOrderedOptionSet*)StructPropertyHandle->GetValueBaseAddress((uint8*)OuterObjects[0]);
	if (!OptionSetProperty) return;

	TSharedPtr<SGSOptionSlider::FOptionSet> OptionSet = MakeShared<SGSOptionSlider::FOptionSet>();
	OptionSet->GetMaxValidIndex = [OptionSetProperty]() { return OptionSetProperty->GetMaxValidIndex(); };
	OptionSet->GetSelectedIndex = [OptionSetProperty]() { return OptionSetProperty->SelectedIndex; };
	OptionSet->SetSelectedIndex = [OptionSetProperty](int NewIndex, bool bIsInteractive) {
		OptionSetProperty->SelectedIndex = NewIndex;
	};
	OptionSet->GetSelectedIndexLabel = [OptionSetProperty](int SelectedIndex) {
		return OptionSetProperty->GetLabelForIndex(SelectedIndex);
	};

	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	];

	// this is kinda gross but there is no way to force an update of the label on an SSpinBox,
	// so we have to fully replace it to get it to rebuild if the extern strings are modified...

	SliderContainer = SNew(SBox)
	[
		SNew(SGSOptionSlider)
			.OptionSet(OptionSet)
	];

	HeaderRow.ValueContent()
	[
		SliderContainer.ToSharedRef()
	];

	OptionSetProperty->OnOptionSetModified.AddLambda([this, OptionSet]() {
		if (SliderContainer.IsValid() == false)
			return;		// how does this happen?
		SliderContainer->SetContent(
			SNew(SGSOptionSlider)
				.OptionSet(OptionSet)
		);
	});
}

void FGSOrderedOptionSetCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

#undef LOCTEXT_NAMESPACE
