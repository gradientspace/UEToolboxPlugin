// Copyright Gradientspace Corp. All Rights Reserved.
#include "SFlyoutSelectionPanel.h"

#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/STileView.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSpacer.h"

#include "Internationalization/BreakIterator.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/SlateStyleMacros.h"

#include "Brushes/SlateColorBrush.h"

#include "ModelingWidgets/ModelingCustomizationUtil.h"

#define LOCTEXT_NAMESPACE "SFlyoutSelectionPanel"


/**
 * Internal class used for icons in the ComboPanel STileView and in the ComboButton
 */
class SFlyoutSelectionPanelIconTile : public SCompoundWidget
{
public:
	using FOnGenerateItemWidget = typename TSlateDelegates<SFlyoutSelectionPanel::FSelectableItem>::FOnGenerateWidget;

	SLATE_BEGIN_ARGS(SFlyoutSelectionPanelIconTile)
		: _ThumbnailSize(102, 102)
		, _ExteriorPadding(0.0f)
		, _ImagePadding(FMargin(0))
		, _ThumbnailPadding(FMargin(0.0f, 0))
		, _bShowLabel(true)
		, _IsSelected(false)
		{
		}

		SLATE_ATTRIBUTE(SFlyoutSelectionPanel::FSelectableItem, SourceItem)

		SLATE_ATTRIBUTE(const FSlateBrush*, Image)
		SLATE_EVENT(FOnGenerateItemWidget, OnGenerateItem)

		SLATE_ATTRIBUTE(FText, DisplayName)
		/** The size of the thumbnail */
		SLATE_ARGUMENT(FVector2D, ThumbnailSize)
		/** The size of the thumbnail */
		SLATE_ARGUMENT(float, ExteriorPadding)
		/** The size of the thumbnail */
		SLATE_ARGUMENT(FMargin, ImagePadding)
		/** The size of the image in the thumbnail. Used to make the thumbnail not fill the entire thumbnail area. If this is not set the Image brushes desired size is used */
		SLATE_ARGUMENT(TOptional<FVector2D>, ImageSize)
		SLATE_ARGUMENT(FMargin, ThumbnailPadding)
		SLATE_ARGUMENT(bool, bShowLabel)
		SLATE_ATTRIBUTE(bool, IsSelected)
	SLATE_END_ARGS()

public:
	/**
	 * Construct this widget
	 */
	void Construct( const FArguments& InArgs );

	TSharedPtr<SWidget> MakeImageWidget(FVector2D ImageSize);

	void UpdateSourceItem(SFlyoutSelectionPanel::FSelectableItem NewSourceItem);

private:
	TAttribute<const FSlateBrush*> Image;
	TAttribute<FText> Text;
	TAttribute<bool> IsSelected;
	TAttribute<SFlyoutSelectionPanel::FSelectableItem> SourceItem;
	FOnGenerateItemWidget OnGenerateItem;

	FVector2D ImageWidgetSize;
	TSharedPtr<SWidget> ImageWidget;
	TSharedPtr<SBox> ImageParentBox;
};


void SFlyoutSelectionPanelIconTile::UpdateSourceItem(SFlyoutSelectionPanel::FSelectableItem NewSourceItem)
{
	SourceItem = NewSourceItem;

	Image = NewSourceItem.GetIcon();

	if (OnGenerateItem.IsBound())
	{
		ImageParentBox->SetContent(
			OnGenerateItem.Execute(SourceItem.Get()) 
		);
	}
	else if (Image.IsSet() && Image.Get() != nullptr)
	{
		ImageParentBox->SetContent(
			SNew(SImage)
			.Image(Image)
			.DesiredSizeOverride(ImageWidgetSize)
		);
	}
}


TSharedPtr<SWidget> SFlyoutSelectionPanelIconTile::MakeImageWidget(FVector2D ImageSize)
{
	if (OnGenerateItem.IsBound())
	{
		ImageParentBox = SNew(SBox)
			.WidthOverride(ImageSize.X)
			.HeightOverride(ImageSize.Y)
			[
				OnGenerateItem.Execute(SourceItem.Get())
			];
	}
	else if (Image.IsSet() && Image.Get() != nullptr)
	{
		ImageParentBox = SNew(SBox)
			.WidthOverride(ImageSize.X)
			.HeightOverride(ImageSize.Y)
			[
				SNew(SImage)
					.Image(Image)
					.DesiredSizeOverride(ImageSize)
			];
	}
	else
	{
		ImageParentBox = SNew(SBox)
			.WidthOverride(ImageSize.X)
			.HeightOverride(ImageSize.Y);
	}
	return ImageParentBox;
}


void SFlyoutSelectionPanelIconTile::Construct(const FArguments& InArgs)
{
	Image = InArgs._Image;
	Text = InArgs._DisplayName;
	IsSelected = InArgs._IsSelected;
	this->SourceItem = InArgs._SourceItem;
	this->OnGenerateItem = InArgs._OnGenerateItem;
	this->ImageWidgetSize = InArgs._ImageSize.Get(FVector2D(64, 64));

	TSharedPtr<SWidget> LabelContent;
	if (InArgs._bShowLabel)
	{
		LabelContent = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SBorder)
				.Padding(InArgs._ThumbnailPadding)
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Right)
				.Padding(FMargin(3.0f, 3.0f))
				[
					SNew(STextBlock)
					.Font( DEFAULT_FONT("Regular", 8) )
					.AutoWrapText(false)
					.LineBreakPolicy(FBreakIterator::CreateWordBreakIterator())
					.Text(Text)
					.ColorAndOpacity( FAppStyle::Get().GetSlateColor("Colors.Foreground") )
				]
			];
	}
	else
	{
		LabelContent = SNew(SSpacer);
	}

	ImageWidget = MakeImageWidget(ImageWidgetSize);

	ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(InArgs._ExteriorPadding))
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		//.BorderImage(FAppStyle::Get().GetBrush("ProjectBrowser.ProjectTile.DropShadow"))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
				// Thumbnail
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(InArgs._ThumbnailSize.X)
					.HeightOverride(InArgs._ThumbnailSize.Y)
					[
						SNew(SBorder)
						.Padding(InArgs._ImagePadding)
						//.BorderImage(FAppStyle::Get().GetBrush("ProjectBrowser.ProjectTile.ThumbnailAreaBackground"))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Top)
						// TODO: Sort out the color transformation for BorderBackgroundColor to use FAppStyle
						// There is some color transformation beyond just SRGB that cause the colors to appear
						// darker than the actual color.
						//.BorderBackgroundColor(FAppStyle::Get().GetSlateColor("Colors.Recessed"))
						//.BorderBackgroundColor(FLinearColor(0.16f, 0.16f, 0.16f))
						//.BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f))
						[
							ImageWidget->AsShared()
						]
					]
				]
			]
			+ SOverlay::Slot()
			[
				SNew(SBox)
					.WidthOverride(InArgs._ThumbnailSize.X)
					[
						LabelContent->AsShared()
					]
			]
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.Image_Lambda
				(
					[this]()
					{
						const bool bSelected = IsSelected.Get();
						const bool bHovered = IsHovered();

						if (bSelected && bHovered)
						{
							static const FName SelectedHover("ProjectBrowser.ProjectTile.SelectedHoverBorder");
							return FAppStyle::Get().GetBrush(SelectedHover);
						}
						else if (bSelected)
						{
							static const FName Selected("ProjectBrowser.ProjectTile.SelectedBorder");
							return FAppStyle::Get().GetBrush(Selected);
						}
						else if (bHovered)
						{
							static const FName Hovered("ProjectBrowser.ProjectTile.HoverBorder");
							return FAppStyle::Get().GetBrush(Hovered);
						}

						return FStyleDefaults::GetNoBrush();
					}
				)
			]
		]
	];
}





void SFlyoutSelectionPanel::Construct(const FArguments& InArgs)
{
	this->ComboButtonTileSize = InArgs._ComboButtonTileSize;
	this->FlyoutTileSize = InArgs._FlyoutTileSize;
	this->FlyoutSize = InArgs._FlyoutSize;
	this->Items = InArgs._ListItems;
	this->OnSelectionChanged = InArgs._OnSelectionChanged;
	this->OnGenerateItem = InArgs._OnGenerateItem;

	// Check for any parameters that the coder forgot to specify.
	if ( Items.Num() == 0 )
	{
		UE::ModelingUI::SetCustomWidgetErrorString(LOCTEXT("MissingListItemsError", "Please specify a ListItems with at least one Item."), this->ChildSlot);
		return;
	}

	int InitialSelectionIndex = FMath::Clamp(InArgs._InitialSelectionIndex, 0, Items.Num());
	SelectedItem = Items[InitialSelectionIndex];

	MissingIcon = MakeShared<FSlateBrush>();


	TileView = SNew(STileView<TSharedPtr<FSelectableItem>>)
		.ListItemsSource( &Items )
		.OnGenerateTile(this, &SFlyoutSelectionPanel::CreateFlyoutIconTile)
		.OnSelectionChanged(this, &SFlyoutSelectionPanel::FlyoutSelectionChanged)
		.ClearSelectionOnClick(false)
		.ItemAlignment(EListItemAlignment::LeftAligned)
		.ItemWidth(InArgs._FlyoutTileSize.X)
		.ItemHeight(InArgs._FlyoutTileSize.Y)
		.SelectionMode(ESelectionMode::Single);

	ComboButtonContent = SNew(SFlyoutSelectionPanelIconTile)
		//.Image_Lambda([this]() { return (SelectedItem != nullptr && SelectedItem->Icon != nullptr) ? SelectedItem->Icon : MissingIcon.Get(); })
		//.Image([this]() { return (SelectedItem != nullptr && SelectedItem->Icon != nullptr) ? SelectedItem->Icon : MissingIcon.Get(); })
		.DisplayName_Lambda([this]() { return SelectedItem->Name; })
		.ExteriorPadding(0.0f)
		.ImagePadding(FMargin(0,0,0,0))
		//.ImageSize( FVector2D(ComboButtonTileSize.X-13, ComboButtonTileSize.Y-13) )
		.ImageSize(FVector2D(ComboButtonTileSize.X, ComboButtonTileSize.Y))
		.ThumbnailSize(ComboButtonTileSize)
		.bShowLabel(false)
		.SourceItem_Lambda([this]() { return *SelectedItem; })
		.OnGenerateItem(OnGenerateItem);

	ComboButton = SNew(SComboButton)
		//.ToolTipText(BrushTypeHandle->GetToolTipText())		// how to do this? SWidget has TooltipText...
		.HasDownArrow(false)
		.OnGetMenuContent( this, &SFlyoutSelectionPanel::OnGetMenuContent )
		.OnMenuOpenChanged( this, &SFlyoutSelectionPanel::OnMenuOpenChanged )
		//.IsEnabled(IsEnabledAttribute)
		.ContentPadding(FMargin(0,0,0,0))
		.ButtonContent()
		[
			ComboButtonContent->AsShared()
		];

	TileView->SetSelection(SelectedItem);

	ChildSlot
	[
		ComboButton->AsShared()
	];
}

void SFlyoutSelectionPanel::SetSelectionIndex(int InSelectionIndex)
{
	int SelectionIndex = FMath::Clamp(InSelectionIndex, 0, Items.Num());

	SelectedItem = Items[SelectionIndex];
	TileView->SetSelection(SelectedItem);
}

TSharedRef<SWidget> SFlyoutSelectionPanel::OnGetMenuContent()
{
	bool bInShouldCloseWindowAfterMenuSelection = true;
	bool bCloseSelfOnly = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, nullptr, nullptr, bCloseSelfOnly);
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("SectionHeader", "Options"));
	{
		TSharedPtr<SWidget> MenuContent;
		MenuContent =
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SAssignNew(TileViewContainer, SBox)
				.Padding(6.0f)
				.MinDesiredWidth( FlyoutSize.X )
			];

		TileViewContainer->SetContent(TileView->AsShared());

		MenuBuilder.AddWidget(MenuContent.ToSharedRef(), FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SFlyoutSelectionPanel::OnMenuOpenChanged(bool bOpen)
{
	if (bOpen == false)
	{
		ComboButton->SetMenuContent(SNullWidget::NullWidget);
	}
}


TSharedRef<ITableRow> SFlyoutSelectionPanel::CreateFlyoutIconTile(
	TSharedPtr<FSelectableItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow< TSharedPtr<FSelectableItem> >, OwnerTable)
		.Style(FAppStyle::Get(), "ProjectBrowser.TableRow")
		.ToolTipText( Item->Name )
		.Padding(2.0f)
		[
			SNew(SFlyoutSelectionPanelIconTile)
			.Image( (Item->GetIcon() != nullptr) ? Item->GetIcon() : MissingIcon.Get() )
			.DisplayName( Item->Name )
			.ThumbnailSize( FlyoutTileSize )
			.ImagePadding(FMargin(0,4,0,0))
			.ImageSize( FVector2D(FlyoutTileSize.X-25, FlyoutTileSize.Y-25) )
			.IsSelected_Lambda( [this, Item] { return Item.Get() == SelectedItem.Get(); })
			.SourceItem(*Item)
			.OnGenerateItem(this->OnGenerateItem)
		];
}


void SFlyoutSelectionPanel::FlyoutSelectionChanged(
	TSharedPtr<FSelectableItem> SelectedItemIn,
	ESelectInfo::Type SelectInfo)
{
	SelectedItem = SelectedItemIn;

	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
		if (SFlyoutSelectionPanelIconTile* ComboContentTile = (SFlyoutSelectionPanelIconTile*)ComboButtonContent.Get())
		{
			ComboContentTile->UpdateSourceItem(*SelectedItem);
		}
	}

	if ( OnSelectionChanged.IsBound() )
	{
		OnSelectionChanged.ExecuteIfBound(SelectedItem);
	}	
}


#undef LOCTEXT_NAMESPACE
