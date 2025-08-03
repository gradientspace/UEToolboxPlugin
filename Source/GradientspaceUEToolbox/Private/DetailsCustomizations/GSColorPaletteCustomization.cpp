// Copyright Gradientspace Corp. All Rights Reserved.
#include "DetailsCustomizations/GSColorPaletteCustomization.h"

#include "PropertyTypes/GSColorPalette.h"

#include "DetailWidgetRow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Colors/SColorBlock.h"


#define LOCTEXT_NAMESPACE "FGSColorPaletteCustomization"

TSharedRef<IPropertyTypeCustomization> FGSColorPaletteCustomization::MakeInstance()
{
	return MakeShareable(new FGSColorPaletteCustomization);
}




TSharedRef<SWidget> MakeColorPaletteButton(
	FLinearColor Color,
	int ButtonSize,
	TSharedPtr<FGSColorPaletteTarget> Target )
{
	return
		SNew(SBox)
		.WidthOverride(ButtonSize)
		.HeightOverride(ButtonSize)
		.MinDesiredWidth(ButtonSize).MaxDesiredWidth(ButtonSize)
		.MinDesiredHeight(ButtonSize).MaxDesiredHeight(ButtonSize)
		[
			SNew(SColorBlock)
				.ToolTipText(FText::FromString(FString::Printf(TEXT("(R=%1.2f G=%1.2f B=%1.2f A=%1.2f)"),
					Color.R, Color.G, Color.B, Color.A)))
				.ColorIsHSV(false).UseSRGB(false)
				.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Separate)
				.Color(Color)
				.OnMouseButtonDown_Lambda([Color, Target](const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) { 
					if (Target.IsValid()) {
						if (MouseEvent.GetModifierKeys().IsControlDown())
							Target->RemovePaletteColor(Color);
						else
							Target->SetSelectedColor(Color, MouseEvent.GetModifierKeys().IsShiftDown());
					}
					return FReply::Handled();
				})
		];
}




void FGSColorPaletteCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() < 1) return;
	FGSColorPalette* ColorPaletteProperty = (FGSColorPalette*)StructPropertyHandle->GetValueBaseAddress((uint8*)OuterObjects[0]);
	if (!ColorPaletteProperty) return;

	PaletteWidget = SNew(SBox)
		.Padding(0, 4, 0, 0);

	UpdatePalette(ColorPaletteProperty);

	HeaderRow.WholeRowContent()
	[
		PaletteWidget.ToSharedRef()
	];

	ColorPaletteProperty->OnPaletteModified.AddSP(this, &FGSColorPaletteCustomization::UpdatePalette);
}


void FGSColorPaletteCustomization::UpdatePalette(const FGSColorPalette* Palette)
{
	TSharedPtr<SWrapBox> PaletteContents = SNew(SWrapBox)
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.PreferredSize(2000)
		.UseAllottedSize(true);

	for (const FLinearColor& Color : Palette->Colors)
	{
		PaletteContents->AddSlot()
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		[
			MakeColorPaletteButton(Color, Palette->ButtonSize, Palette->Target)
		];
	}

	PaletteWidget->SetContent(PaletteContents.ToSharedRef());
}


void FGSColorPaletteCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}




#undef LOCTEXT_NAMESPACE 



