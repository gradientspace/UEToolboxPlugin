// Copyright Gradientspace Corp. All Rights Reserved.
#include "DetailsCustomizations/ToggleButtonGroupCustomization.h"
#include "PropertyTypes/ToggleButtonGroup.h"
#include "GradientspaceUEToolboxStyle.h"

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

#define LOCTEXT_NAMESPACE "FGSToggleButtonGroupCustomization"

TSharedRef<IPropertyTypeCustomization> FGSToggleButtonGroupCustomization::MakeInstance()
{
	return MakeShareable(new FGSToggleButtonGroupCustomization);
}




void FGSToggleButtonGroupCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() < 1) return;
	FGSToggleButtonGroup* PropertyObj = (FGSToggleButtonGroup*)StructPropertyHandle->GetValueBaseAddress((uint8*)OuterObjects[0]);
	if (!PropertyObj) return;

	TSharedPtr<FGSToggleButtonTarget> Target = PropertyObj->Target;
	ensure(Target.IsValid());

	FMargin TextPadding(PropertyObj->PadLeft, PropertyObj->PadTop, PropertyObj->PadRight, PropertyObj->PadBottom);
	int FontSize = PropertyObj->FontSize;
	bool bBold = PropertyObj->bBold;


	FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Regular", FontSize);
	float UseMinDesiredWidth = 0.0f;
	if (PropertyObj->ButtonFixedWidth != 0)
		UseMinDesiredWidth = PropertyObj->ButtonFixedWidth;
	// calculate max-width of all labels
	if (PropertyObj->bCalculateFixedButtonWidth)
	{
		const TSharedRef<FSlateFontMeasure>& FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		float MaxWidth = 0;
		for (const FGSToggleButtonGroupItem& EnumItem : PropertyObj->Items) {
			float StringWidth = FontMeasure->Measure(EnumItem.UIString, FontInfo).X;
			MaxWidth = FMath::Max(MaxWidth, StringWidth);
		}
		UseMinDesiredWidth = MaxWidth;
	}




	auto MakeToggleButton = [this, Target, bBold, TextPadding](FGSToggleButtonGroupItem Item, float MinDesiredWidth)
	{
		//const FButtonStyle& FoundStyle = FGradientspaceUEToolboxStyle::Get()->GetWidgetStyle<FButtonStyle>("ActionButtonStyle");
		//const char* UseStyleName = (Item.bHaveCustomColor) ? "ActionButtonStyle_CustomColor" : "ActionButtonStyle";
		//const FName UseFontStyle = (bBold) ? "Bold" : "Regular";

		TSharedPtr<SCheckBox> CheckButton = SNew(SCheckBox)
			//.ButtonStyle(FAppStyle::Get(), "EditorUtilityButton")
			.Style(FAppStyle::Get(), "DetailsView.SectionButton")
			.Padding(TextPadding)
			.ToolTipText(Item.TooltipText)
			.HAlign(HAlign_Center)
			.OnCheckStateChanged_Lambda([this, Item, Target](ECheckBoxState NewState)
			{
				bool bSet = (NewState == ECheckBoxState::Checked) ? true : false;
				if (Target.IsValid() && (!!Target->SetToggleValue))
					Target->SetToggleValue(Item.ToggleName, bSet);
			})
			.IsChecked_Lambda([this, Item, Target]() -> ECheckBoxState
			{
				bool bSet = (Target.IsValid() && (!!Target->GetToggleValue)) ? Target->GetToggleValue(Item.ToggleName) : false;
				return bSet ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.IsEnabled_Lambda([this, Item, Target]() {
				return (Target.IsValid() && (!!Target->IsToggleEnabled)) ? Target->IsToggleEnabled(Item.ToggleName) : true;
			})
			.Visibility_Lambda([this, Item, Target]() {
				return (Target.IsValid() && (!!Target->IsToggleVisible) && Target->IsToggleVisible(Item.ToggleName) == false) ? EVisibility::Collapsed : EVisibility::Visible;
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0))
				.AutoWidth()
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					.Text(Item.UIString)
					.MinDesiredWidth(MinDesiredWidth)
				]
			];
		return CheckButton.ToSharedRef();
	};



	EHorizontalAlignment Align = (PropertyObj->bCentered) ? EHorizontalAlignment::HAlign_Center : EHorizontalAlignment::HAlign_Left;



	//TSharedPtr<SWrapBox> WrapBox = SNew(SWrapBox)
	//	.HAlign(EHorizontalAlignment::HAlign_Center)
	//	.PreferredSize(2000)
	//	.UseAllottedSize(true);

	//for (const FGSToggleButtonGroupItem& Item : PropertyObj->Items)
	//{
	//	WrapBox->AddSlot()
	//	.Padding(0, 0, PropertyObj->ButtonSpacingHorz, PropertyObj->ButtonSpacingVert)
	//	[
	//		MakeToggleButton(Item, UseMinDesiredWidth)
	//	];
	//}



	TSharedPtr<SWidget> ButtonsContainerWidget;

	if ( PropertyObj->bUseGridLayout == false )
	{
		TSharedPtr<SWrapBox> WrapBox = SNew(SWrapBox)
			.HAlign(Align)
			.PreferredSize(2000)
			.UseAllottedSize(true);
		for (const FGSToggleButtonGroupItem& Item : PropertyObj->Items)
		{
			WrapBox->AddSlot()
			.Padding(0, 0, PropertyObj->ButtonSpacingHorz, PropertyObj->ButtonSpacingVert)
			[
				MakeToggleButton(Item, UseMinDesiredWidth)
			];
		}
		ButtonsContainerWidget = WrapBox;
	}
	else
	{
		TSharedPtr<SUniformWrapPanel> GridPanel = SNew(SUniformWrapPanel)
			.SlotPadding(FMargin(PropertyObj->ButtonSpacingHorz/2.0f, PropertyObj->ButtonSpacingVert/2.0f))
			.EvenRowDistribution(true);
		for (const FGSToggleButtonGroupItem& Item : PropertyObj->Items)
		{
			GridPanel->AddSlot()
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				MakeToggleButton(Item, UseMinDesiredWidth)
			];
		}
		ButtonsContainerWidget = GridPanel;
	}




	if ( PropertyObj->bShowLabel )
	{
		HeaderRow.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
		HeaderRow.ValueContent()
		[
			ButtonsContainerWidget.ToSharedRef()
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

void FGSToggleButtonGroupCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

#undef LOCTEXT_NAMESPACE
