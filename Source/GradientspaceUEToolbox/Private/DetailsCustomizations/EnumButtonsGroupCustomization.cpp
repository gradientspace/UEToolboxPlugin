// Copyright Gradientspace Corp. All Rights Reserved.
#include "DetailsCustomizations/EnumButtonsGroupCustomization.h"
#include "PropertyTypes/EnumButtonsGroup.h"

#include "DetailWidgetRow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SUniformWrapPanel.h"
#include "Widgets/Input/SCheckBox.h"

#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"


#define LOCTEXT_NAMESPACE "FGSEnumButtonsGroupCustomization"

TSharedRef<IPropertyTypeCustomization> FGSEnumButtonsGroupCustomization::MakeInstance()
{
	return MakeShareable(new FGSEnumButtonsGroupCustomization);
}




TSharedRef<SCheckBox> MakeEnumItemButton(
	FString EnumItemName,
	int EnumItemIdentifier,
	TSharedPtr<FGSEnumButtonsTarget> Target,
	FText ButtonLabelText,
	FText TooltipText,
	FMargin TextPadding,	
	const FSlateFontInfo& FontInfo,
	float MinDesiredWidth = 0)
{
	return SNew(SCheckBox)
		//.ButtonStyle(FAppStyle::Get(), "EditorUtilityButton")
		.Style(FAppStyle::Get(), "DetailsView.SectionButton")
		.Padding(TextPadding)
		.ToolTipText(TooltipText)
		.HAlign(HAlign_Center)
		.OnCheckStateChanged_Lambda([EnumItemIdentifier, Target](ECheckBoxState NewState)
		{
			if ( NewState == ECheckBoxState::Checked )
				Target->SetSelectedItem(EnumItemIdentifier);
		})
		.IsChecked_Lambda([EnumItemIdentifier, Target]() -> ECheckBoxState
		{
			bool bSet = (Target->GetSelectedItemIdentifier() == EnumItemIdentifier);
			return bSet ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.IsEnabled_Lambda([EnumItemIdentifier, Target]() {
			return Target->IsItemEnabled(EnumItemIdentifier);
		})
		.Visibility_Lambda([EnumItemIdentifier, Target]() {
			return Target->IsItemVisible(EnumItemIdentifier) ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0,0))
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(FontInfo)
				.Justification(ETextJustify::Center)
				.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
				.Text(ButtonLabelText)
				.MinDesiredWidth(MinDesiredWidth)
			]
		];
}





void FGSEnumButtonsGroupCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() < 1) return;
	FGSEnumButtonsGroup* PropertyObj = (FGSEnumButtonsGroup*)StructPropertyHandle->GetValueBaseAddress((uint8*)OuterObjects[0]);
	if (!PropertyObj) return;

	TSharedPtr<FGSEnumButtonsTarget> Target = PropertyObj->Target;
	ensure(Target.IsValid());

	EHorizontalAlignment Align = (PropertyObj->bCentered) ? EHorizontalAlignment::HAlign_Center : EHorizontalAlignment::HAlign_Left;
	FMargin TextPadding(PropertyObj->PadLeft, PropertyObj->PadTop, PropertyObj->PadRight, PropertyObj->PadBottom);

	int FontSize = PropertyObj->FontSize;
	FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Regular", FontSize);

	float UseMinDesiredWidth = 0.0f;
	if (PropertyObj->ButtonFixedWidth != 0)
		UseMinDesiredWidth = PropertyObj->ButtonFixedWidth;
	
	if (PropertyObj->bCalculateFixedButtonWidth)
	{
		const TSharedRef<FSlateFontMeasure>& FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		float MaxWidth = 0;
		for (const FGSEnumButtonsGroupItem& EnumItem : PropertyObj->EnumItems)
		{
			float StringWidth = FontMeasure->Measure(EnumItem.UIString, FontInfo).X;
			MaxWidth = FMath::Max(MaxWidth, StringWidth);
		}
		UseMinDesiredWidth = MaxWidth;
	}

	TSharedPtr<SWidget> ButtonsContainerWidget;

	if ( PropertyObj->bUseGridLayout == false )
	{
		TSharedPtr<SWrapBox> WrapBox = SNew(SWrapBox)
			.HAlign(Align)
			.PreferredSize(2000)
			.UseAllottedSize(true);
		for (const FGSEnumButtonsGroupItem& EnumItem : PropertyObj->EnumItems)
		{
			WrapBox->AddSlot()
			.Padding(0, 0, PropertyObj->ButtonSpacingHorz, PropertyObj->ButtonSpacingVert)
			[
				MakeEnumItemButton(EnumItem.ItemName, EnumItem.ItemIdentifier, Target, EnumItem.UIString, EnumItem.TooltipText, TextPadding, FontInfo, UseMinDesiredWidth)
			];
		}
		ButtonsContainerWidget = WrapBox;
	}
	else
	{
		TSharedPtr<SUniformWrapPanel> GridPanel = SNew(SUniformWrapPanel)
			.SlotPadding(FMargin(PropertyObj->ButtonSpacingHorz/2.0f, PropertyObj->ButtonSpacingVert/2.0f))
			.EvenRowDistribution(true);
		for (const FGSEnumButtonsGroupItem& EnumItem : PropertyObj->EnumItems)
		{
			GridPanel->AddSlot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
			[
				MakeEnumItemButton(EnumItem.ItemName, EnumItem.ItemIdentifier, Target, EnumItem.UIString, EnumItem.TooltipText, TextPadding, FontInfo, UseMinDesiredWidth)
			];
		}
		ButtonsContainerWidget = GridPanel;
	}




	// PolyEdit tool uses ToolbarBuilder to do the wide-lozenge thing...
	// However you can't just add a button to it, and also it's not clear it could wrap the buttons...
	// (see MeshTopologySelectionMechanicCustomization.cpp)
	//TSharedPtr<FUICommandList> Temp = MakeShared<FUICommandList>();
	//FSlimHorizontalToolBarBuilder ToolbarBuilder(Temp, FMultiBoxCustomization::None);

	if ( PropertyObj->bShowLabel )
	{
		HeaderRow.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
		HeaderRow.ValueContent()
		[
			//SNew(SBox)
			//.Padding(0, 4, 0, 0)
			//[
				ButtonsContainerWidget.ToSharedRef()
			//]
		];

		// by default all detail row ValueWidget's are inside a SConstrainedBox (SDetailSingleItemRow.cpp, line 391) which
		// somehow has a minimum size of 125 (if you scale the panel down), but then it gets stuck at 125 and won't scale back
		// up if you make it bigger, unless the MinDesiredWidth is set higher.
		// (at least, it was like this in 5.3)
		HeaderRow.ValueWidget.MaxDesiredWidth(1000);
		HeaderRow.ValueWidget.MinDesiredWidth(500);
	}
	else {		// Centered, No Label
		HeaderRow.WholeRowContent()
		[
			SNew(SBox)
			.Padding(0, PropertyObj->GroupSpacingAbove, 0, PropertyObj->GroupSpacingBelow)
			[
				ButtonsContainerWidget.ToSharedRef()
			]
		];
	}

}

void FGSEnumButtonsGroupCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}


#undef LOCTEXT_NAMESPACE
