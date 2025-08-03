// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateWidgetStyleAsset.h"

class GRADIENTSPACEUEWIDGETS_API SGSOptionSlider : public SCompoundWidget
{
public:

	struct FOptionSet
	{
		TFunction<int()> GetMaxValidIndex;
		TFunction<FString(int Index)> GetSelectedIndexLabel;
		TFunction<void(int NewIndex, bool bInInteraction)> SetSelectedIndex;
		TFunction<int()> GetSelectedIndex;
	};



public:

	SLATE_BEGIN_ARGS(SGSOptionSlider)
		:
		_SpinBoxStyle(&FAppStyle::Get().GetWidgetStyle<FSpinBoxStyle>("NumericEntrySpinBox"))
		{}

		SLATE_ARGUMENT(TSharedPtr<FOptionSet>, OptionSet)

		SLATE_STYLE_ARGUMENT(FSpinBoxStyle, SpinBoxStyle)

		SLATE_ATTRIBUTE(FSlateFontInfo, Font)


	SLATE_END_ARGS()
	SGSOptionSlider()
	{
	}


	void Construct(const FArguments& InArgs);

protected:
	TSharedPtr<SWidget> SpinBox;

	TSharedPtr<FOptionSet> OptionSet;
	int OnGetSelectedIndex() const;
	void OnSelectedIndexChanged(int NewIndex);
	void OnSelectedIndexCommitted(int NewIndex, ETextCommit::Type CommitInfo);
	int LastCommittedIndex = -1;

	bool bInSliderInteraction = false;
	void OnBeginSliderMovement();
	void OnEndSliderMovement(int NewIndex);


protected:

	//~ SWidget Interface
	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override
	{
		FReply Reply = FReply::Handled();
		TSharedPtr<SWidget> FocusWidget = SpinBox;

		if ( InFocusEvent.GetCause() != EFocusCause::Cleared ) {
			// Forward keyboard focus to widget
			Reply.SetUserFocus(FocusWidget.ToSharedRef(), InFocusEvent.GetCause());
		}

		return Reply;
	}

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
	{
		FKey Key = InKeyEvent.GetKey();
		// todo support left/right keys here, maybe number keys?
		//if( Key == EKeys::Escape && EditableText->HasKeyboardFocus() )
		//{
		//	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Cleared);
		//}
		return FReply::Unhandled();
	}



	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		// look at SNumericEntryBox::Tick...why does it have cached value?
	}

};
