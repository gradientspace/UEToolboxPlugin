// Copyright Gradientspace Corp. All Rights Reserved.
#include "DetailsCustomizations/ActionButtonGroupCustomization.h"
#include "PropertyTypes/ActionButtonGroup.h"
#include "GradientspaceUEToolboxStyle.h"

#include "DetailWidgetRow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "FGSActionButtonGroupCustomization"

TSharedRef<IPropertyTypeCustomization> FGSActionButtonGroupCustomization::MakeInstance()
{
	return MakeShareable(new FGSActionButtonGroupCustomization);
}

void FGSActionButtonGroupCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() < 1) return;
	FGSActionButtonGroup* PropertyObj = (FGSActionButtonGroup*)StructPropertyHandle->GetValueBaseAddress((uint8*)OuterObjects[0]);
	if (!PropertyObj) return;

	TSharedPtr<FGSActionButtonTarget> Target = PropertyObj->Target;
	ensure(Target.IsValid());

	FMargin TextPadding(PropertyObj->PadLeft, PropertyObj->PadTop, PropertyObj->PadRight, PropertyObj->PadBottom);
	int FontSize = PropertyObj->FontSize;
	bool bBold = PropertyObj->bBold;

	auto MakeActionButton = [this, Target, FontSize, bBold, TextPadding](FGSActionButtonGroupItem Item)
	{
		const char* UseStyleName = (Item.bHaveCustomColor) ? "ActionButtonStyle_CustomColor" : "ActionButtonStyle";
		const FName UseFontStyle = (bBold) ? "Bold" : "Regular";

		TSharedPtr<SButton> Button = SNew(SButton)
			//.ButtonStyle(FAppStyle::Get(), "EditorUtilityButton")
			.ButtonStyle(FGradientspaceUEToolboxStyle::Get(), UseStyleName)
			//.ButtonStyle(FAppStyle::Get(), "DetailsView.SectionButton")
			.ToolTipText(Item.TooltipText)
			.ContentPadding(TextPadding)
			.OnClicked_Lambda([this, Item, Target]() {
				if (Target.IsValid() && (!!Target->ExecuteCommand) ) 
					Target->ExecuteCommand(Item.Command);
				return FReply::Handled();
			})
			.IsEnabled_Lambda([this, Item, Target]() {
				return (Target.IsValid() && (!!Target->IsCommandEnabled) ) ? Target->IsCommandEnabled(Item.Command) : true;
			})
			.Visibility_Lambda([this, Item, Target]() {
				return (Target.IsValid() && (!!Target->IsCommandVisible) && Target->IsCommandVisible(Item.Command) == false) ? EVisibility::Collapsed : EVisibility::Visible;
			})
			//.ButtonColorAndOpacity(FSlateColor(FColor(200,0,0))		// this works but cannot be changed after consruction...
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0))
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(UseFontStyle, FontSize))
					.Justification(ETextJustify::Center)
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					.Text(Item.UIString)
				]
			];
		if (Item.bHaveCustomColor)
			Button->SetBorderBackgroundColor(FSlateColor(Item.CustomColor));
		return Button.ToSharedRef();
	};

	TSharedPtr<SWrapBox> WrapBox = SNew(SWrapBox)
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.PreferredSize(2000)
		.UseAllottedSize(true);

	for (const FGSActionButtonGroupItem& Item : PropertyObj->Actions)
	{
		WrapBox->AddSlot()
			.Padding(0, 0, PropertyObj->ButtonSpacingHorz, PropertyObj->ButtonSpacingVert)
			[
				MakeActionButton(Item)
			];
	}

	HeaderRow.WholeRowContent()
	[
		SNew(SBox)
		.Padding(0, PropertyObj->GroupSpacingAbove, 0, PropertyObj->GroupSpacingBelow)
		[
			WrapBox.ToSharedRef()
		]
	];

}

void FGSActionButtonGroupCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

#undef LOCTEXT_NAMESPACE
