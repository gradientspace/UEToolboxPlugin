// Copyright Gradientspace Corp. All Rights Reserved.
#include "DetailsCustomizations/PaintToolCustomizations.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "ModelingWidgets/ModelingCustomizationUtil.h"

#include "SFlyoutSelectionPanel.h"

#include "Tools/PixelPaintTool.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"

using namespace UE::ModelingUI;

#define LOCTEXT_NAMESPACE "GSPaintToolCustomizations"


TSharedRef<IDetailCustomization> FTextureSelectionSetSettingsDetails::MakeInstance()
{
	return MakeShareable(new FTextureSelectionSetSettingsDetails);
}

FTextureSelectionSetSettingsDetails::~FTextureSelectionSetSettingsDetails()
{
	if (ActiveSettings.IsValid())
		ActiveSettings.Get()->OnActiveTextureModifiedByTool.Remove(TexturePickerModifiedHandle);
}


void FTextureSelectionSetSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	check(ObjectsBeingCustomized.Num() > 0);
	ActiveSettings = Cast<UTextureSelectionSetSettings>(ObjectsBeingCustomized[0]);

	TSharedPtr<IPropertyHandle> MaterialTextureSetHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(UTextureSelectionSetSettings, TextureSet), UTextureSelectionSetSettings::StaticClass());
	ensure(MaterialTextureSetHandle->IsValidHandle());

	// if no textures, show error message instead
	if (ActiveSettings->TextureSet.MaterialTextures.Num() == 0)
	{
		DetailBuilder.EditDefaultProperty(MaterialTextureSetHandle)->CustomWidget()
			.OverrideResetToDefault(FResetToDefaultOverride::Hide())
			.WholeRowContent()
			[
				SNew(SBox)
				.HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center)
				.HeightOverride(65)
				[
					SNew(STextBlock)
					.WrapTextAt(175.0f).Justification(ETextJustify::Center)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.15f, 0.15f)))
					.Text(LOCTEXT("NoTexturesMsg", "( No Paintable Textures could be found in the available Materials!! )"))
				]
			];
		return;
	}

	TSharedPtr<SWidget> TexInfoWidget = MakeTextureInfoWidget();
	TSharedPtr<SWidget> UseTexturePicker = MakeTexturePickerWidget(MaterialTextureSetHandle->GetToolTipText());

	DetailBuilder.EditDefaultProperty(MaterialTextureSetHandle)->CustomWidget()
		.OverrideResetToDefault(FResetToDefaultOverride::Hide())
		.WholeRowContent()
		[
			SNew(SVerticalBox)
			//+ SVerticalBox::Slot()
			//.Padding(FMargin(0, ModelingUIConstants::DetailRowVertPadding, 0, 0))
			//.AutoHeight()
			//[
			//	SNew(STextBlock)
			//	.Justification(ETextJustify::Left)
			//	.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
			//	.Text(LOCTEXT("TexturePickerLabel", "Available Textures"))
			//]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(0, ModelingUIConstants::DetailRowVertPadding))
				.AutoWidth()
				[
					SNew(SBox)
					.HeightOverride(ComboIconSize + 14)
					[
						UseTexturePicker->AsShared()
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(10, 10))
				.AutoWidth()
				[
					TexInfoWidget->AsShared()
				]
			]
		];


};


TSharedPtr<SWidget> FTextureSelectionSetSettingsDetails::MakeTextureInfoWidget()
{
	TexPathInfo = SNew(STextBlock)
		.Justification(ETextJustify::Left)
		.TextStyle(FAppStyle::Get(), "SmallText")
		.Text(FText::FromString(TEXT("/texture/path")));
	MatPathInfo = SNew(STextBlock)
		.Justification(ETextJustify::Left)
		.TextStyle(FAppStyle::Get(), "SmallText")
		.Text(FText::FromString(TEXT("/material/path")));
	SourceFormat = SNew(STextBlock)
		.Justification(ETextJustify::Left)
		.TextStyle(FAppStyle::Get(), "SmallText")
		.Text(FText::FromString(TEXT("(source info)")));
	PlatformFormat = SNew(STextBlock)
		.Justification(ETextJustify::Left)
		.TextStyle(FAppStyle::Get(), "SmallText")
		.Text(FText::FromString(TEXT("(platform info)")));


	FText SourceFormatTooltip = LOCTEXT("SourceFormat",
		"The pixel format of the current Texture's Source data. Committing changes to the Texture will update this source image\n" \
		"Note that painting is always RGBA8, so if the Source data is single-channel, the remaining channels will be discarded on commmit.\n" \
		"Similarly if the Source has a higher bit-depth (eg half-float, float32, etc), values > 1 will be lost on commit.");
	SourceFormat->SetToolTipText(SourceFormatTooltip);

	FText PlatformFormatTooltip = LOCTEXT("PlatformFormat",
		"The compression mode used to compress the current Texture. Compression is not visible while painting.\n" \
		"You must switch to another texture, or accept the tool, to see the results of texture compression.");
	PlatformFormat->SetToolTipText(PlatformFormatTooltip);

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)
		[
			TexPathInfo.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)
		[
			MatPathInfo.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)
		[
			SourceFormat.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)
		[
			PlatformFormat.ToSharedRef()
		];
}


void FTextureSelectionSetSettingsDetails::UpdateTextureInfo()
{
	if (!ActiveSettings.IsValid())
		return;
	const UTexture2D* Texture = ActiveSettings->ActiveTexture;
	const UMaterialInterface* Material = ActiveSettings->ActiveMaterial;

	TexPathInfo->SetToolTipText((Texture == nullptr) ? LOCTEXT("None", "(none)") : FText::FromString(Texture->GetPathName()));
	TexPathInfo->SetText((Texture == nullptr) ? LOCTEXT("None", "(none)") : FText::FromString(FString::Printf(TEXT("Tex: %s"), *(Texture->GetName()) )));
	MatPathInfo->SetToolTipText((Material == nullptr) ? LOCTEXT("None", "(none)") : FText::FromString(Material->GetPathName()));
	MatPathInfo->SetText((Material == nullptr) ? LOCTEXT("None", "(none)") : FText::FromString(FString::Printf(TEXT("Mat: %s"), *(Material->GetName()) )));

	SourceFormat->SetText(FText::FromString(*(ActiveSettings->SourceFormat)));
	PlatformFormat->SetText(FText::FromString(ActiveSettings->CompressedFormat));
}


TSharedPtr<SWidget> FTextureSelectionSetSettingsDetails::MakeTexturePickerWidget(const FText& UseTooltipText)
{
	int32 InitialItemIndex = 0;
	TArray<TSharedPtr<SFlyoutSelectionPanel::FSelectableItem>> ComboItems;

	const FToolMaterialTextureSet& MaterialTextureSet = ActiveSettings->TextureSet;
	for (const FMaterialTextureList& MatTexList : MaterialTextureSet.MaterialTextures)
	{
		for (FMaterialTextureListTex Tex : MatTexList.Textures)
		{
			// can directly use texture w/ slatebrush!
			TSharedPtr<FSlateBrush> TexBrush = MakeShared<FSlateBrush>();
			TexBrush->SetResourceObject(Tex.Texture);

			FString UseName = FString::Printf(TEXT("Material: %s  Texture: %s"),
				*MatTexList.Material->GetName(), *Tex.Texture->GetName());

			TSharedPtr<SFlyoutSelectionPanel::FSelectableItem> NewComboItem = MakeShared<SFlyoutSelectionPanel::FSelectableItem>();
			NewComboItem->Name = FText::FromString(UseName);
			NewComboItem->Identifier = Tex.UniqueID;
			NewComboItem->SharedIcon = TexBrush;

			int NewItemIndex = ComboItems.Num();

			if (MatTexList.Material == ActiveSettings->ActiveMaterial && Tex.Texture == ActiveSettings->ActiveTexture)
				InitialItemIndex = NewItemIndex;

			TextureUniqueIDToIndexMap.Add(Tex.UniqueID, NewItemIndex);
			ComboItems.Add(NewComboItem);
		}
	}

	TexturePicker = SNew(SFlyoutSelectionPanel)
		.ToolTipText(UseTooltipText)
		.ComboButtonTileSize(FVector2D(ComboIconSize, ComboIconSize))
		.FlyoutTileSize(FVector2D(FlyoutIconSize, FlyoutIconSize))
		.FlyoutSize(FVector2D(FlyoutWidth, 1))
		.ListItems(ComboItems)
		.OnSelectionChanged_Lambda([this](TSharedPtr<SFlyoutSelectionPanel::FSelectableItem> NewSelectedItem) {
			if (ActiveSettings.IsValid()) {
				ActiveSettings.Get()->SetActiveTextureByUniqueID(NewSelectedItem->Identifier);
				UpdateTextureInfo();
			}
		})
		.FlyoutHeaderText(LOCTEXT("TexturePickerHeader", "Available Textures"))
		.InitialSelectionIndex(InitialItemIndex);

	TexturePickerModifiedHandle = ActiveSettings.Get()->OnActiveTextureModifiedByTool.AddLambda(
		[this](const UTextureSelectionSetSettings* Settings) {
			int UniqueID = Settings->FindTextureUniqueID(Settings->ActiveMaterial, Settings->ActiveTexture);
			if (UniqueID >= 0) {
				if (const int* FoundIndex = TextureUniqueIDToIndexMap.Find(UniqueID))
					TexturePicker->SetSelectionIndex(*FoundIndex);
			}
		});

	return TexturePicker;
}



#undef LOCTEXT_NAMESPACE
