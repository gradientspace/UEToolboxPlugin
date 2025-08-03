// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STileView.h"
#include "Styling/CoreStyle.h"

class SComboButton;

class GRADIENTSPACEUEWIDGETS_API SFlyoutSelectionPanel : public SCompoundWidget
{
public:
	// selectable item/option in the panel
	struct FSelectableItem
	{
		// label shown on tile
		FText Name;
		
		// only one of the icon fields below ought to be set. SharedIcon overrides Icon.

		// pointer to external icon
		const FSlateBrush* Icon = nullptr;
		// owned icon
		TSharedPtr<FSlateBrush> SharedIcon = nullptr;

		// unqiue integer identifier for this item provided by client
		int Identifier = 0;

		const FSlateBrush* GetIcon() const { return SharedIcon.IsValid() ? SharedIcon.Get() : Icon; }
	};

	using FOnGenerateItemWidget = typename TSlateDelegates<FSelectableItem>::FOnGenerateWidget;

	// this event fires when the selection changes, ie active item is modified
	DECLARE_DELEGATE_OneParam(FOnActiveItemChanged, TSharedPtr<FSelectableItem>);
	typedef FOnActiveItemChanged FOnSelectionChanged;

public:

	SLATE_BEGIN_ARGS(SFlyoutSelectionPanel)
		: _ComboButtonTileSize(50, 50)
		, _FlyoutTileSize(85, 85)
		, _FlyoutSize(600, 400)
		, _InitialSelectionIndex(0)
		, _OnSelectionChanged()
		, _OnGenerateItem()
		{
		}

		/** The size of the combo button icon tile */
		SLATE_ARGUMENT(FVector2D, ComboButtonTileSize)

		/** The size of the icon tiles in the flyout */
		SLATE_ARGUMENT(FVector2D, FlyoutTileSize)

		/** Size of the Flyout Panel */
		SLATE_ARGUMENT(FVector2D, FlyoutSize)

		/** List of selectable options */
		SLATE_ARGUMENT( TArray<TSharedPtr<FSelectableItem>> , ListItems )

		/** The text to display above the button list */
		SLATE_ATTRIBUTE( FText, FlyoutHeaderText )

		/** Index into ListItems of the initially-selected item, defaults to index 0 */
		SLATE_ARGUMENT(int, InitialSelectionIndex)

		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)

		SLATE_EVENT(FOnGenerateItemWidget, OnGenerateItem)

	SLATE_END_ARGS()


	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	* Sets the currently selected index without user interaction.
	* 
	* @param InSelectionIndex The new selection index for the widget
	*/
	void SetSelectionIndex(int InSelectionIndex);

protected:
	FVector2D ComboButtonTileSize;
	FVector2D FlyoutTileSize;
	FVector2D FlyoutSize;

	/** Pointer to the array of data items that we are observing */
	TArray<TSharedPtr<FSelectableItem>> Items;

	//! called when selection changes
	FOnSelectionChanged OnSelectionChanged;

	//! called to generate item widgets in flyout/etc
	FOnGenerateItemWidget OnGenerateItem;

	// STileView functions
	virtual TSharedRef<ITableRow> CreateFlyoutIconTile(
		TSharedPtr<FSelectableItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable);
	virtual void FlyoutSelectionChanged(
		TSharedPtr<FSelectableItem> SelectedItemIn,
		ESelectInfo::Type SelectInfo);

	// SComboButton functions
	TSharedRef<SWidget> OnGetMenuContent();
	void OnMenuOpenChanged(bool bOpen);

	// fallback icon for when no icon is provided
	TSharedPtr<FSlateBrush> MissingIcon;

	// pointer to currently-selected item
	TSharedPtr<FSelectableItem> SelectedItem;

	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<SWidget> ComboButtonContent;
	TSharedPtr<STileView<TSharedPtr<FSelectableItem>>> TileView;
	TSharedPtr<SBox> TileViewContainer;
};
