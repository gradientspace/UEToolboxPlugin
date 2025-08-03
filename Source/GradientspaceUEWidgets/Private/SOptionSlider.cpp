// Copyright Gradientspace Corp. All Rights Reserved.
#include "SOptionSlider.h"

#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/NumericTypeInterface.h"
#if WITH_EDITOR
#include "Editor.h"
#endif


#define LOCTEXT_NAMESPACE "SGSOptionSlider"


struct SGSOptionSliderNumericTypeInterface : TDefaultNumericTypeInterface<int>
{
	TSharedPtr<SGSOptionSlider::FOptionSet> OptionSet;

	virtual FString ToString(const int& Index) const override
	{
		return OptionSet->GetSelectedIndexLabel(Index);
	}

	virtual TOptional<int> FromString(const FString& InString, const int& InExistingValue) override
	{
		// temporarily ignore text edit...
		return InExistingValue;
	}
};



void SGSOptionSlider::Construct(const FArguments& InArgs)
{
	check(InArgs._OptionSet.IsValid());

	this->OptionSet = InArgs._OptionSet;
	int MaxValidIndex = this->OptionSet->GetMaxValidIndex();

	TSharedPtr<SGSOptionSliderNumericTypeInterface> ValueInterface = MakeShared<SGSOptionSliderNumericTypeInterface>();
	ValueInterface->OptionSet = this->OptionSet;

	SAssignNew(SpinBox, SSpinBox<int>)
		.Style(InArgs._SpinBoxStyle)
		.Font(InArgs._Font.IsSet() ? InArgs._Font : FCoreStyle::GetDefaultFontStyle("Mono", 9) )
		.Value(this, &SGSOptionSlider::OnGetSelectedIndex)
		.Delta(0)
		//.ShiftMouseMovePixelPerDelta(1)
		//.LinearDeltaSensitivity(InArgs._LinearDeltaSensitivity)
		.OnValueChanged(this, &SGSOptionSlider::OnSelectedIndexChanged)
		.OnValueCommitted(this, &SGSOptionSlider::OnSelectedIndexCommitted)
		//.MinFractionalDigits(MinFractionalDigits)
		//.MaxFractionalDigits(MaxFractionalDigits)
		.MinSliderValue(0)
		.MaxSliderValue(MaxValidIndex)
		.MinValue(0)
		.MaxValue(MaxValidIndex)
		.SliderExponent(1)
		.EnableWheel(true)
		.WheelStep(1)
		//.BroadcastValueChangesPerKey(InArgs._BroadcastValueChangesPerKey)
		.OnBeginSliderMovement(this, &SGSOptionSlider::OnBeginSliderMovement)
		.OnEndSliderMovement(this, &SGSOptionSlider::OnEndSliderMovement)
		//.MinDesiredWidth(InArgs._MinDesiredValueWidth)
		.TypeInterface(ValueInterface)
		//.ToolTipText(this, &SNumericEntryBox<int>::GetValueAsText)
		;


	TSharedRef<SOverlay> Overlay = SNew(SOverlay);

	Overlay->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			SpinBox.ToSharedRef()
		];

	TSharedPtr<SWidget> MainContents;
	MainContents = Overlay;

	ChildSlot
	[
		MainContents.ToSharedRef()
	];
}



int SGSOptionSlider::OnGetSelectedIndex() const
{
	return FMath::Clamp(OptionSet->GetSelectedIndex(), 0, OptionSet->GetMaxValidIndex());
}

void SGSOptionSlider::OnSelectedIndexChanged(int NewIndex)
{
	NewIndex = FMath::Clamp(NewIndex, 0, OptionSet->GetMaxValidIndex());
	if (NewIndex == OnGetSelectedIndex()) return;

	OptionSet->SetSelectedIndex(NewIndex, /*bInInteraction=*/true);
}

void SGSOptionSlider::OnSelectedIndexCommitted(int NewIndex, ETextCommit::Type CommitInfo)
{
	NewIndex = FMath::Clamp(NewIndex, 0, OptionSet->GetMaxValidIndex());
	if (bInSliderInteraction || OnGetSelectedIndex() != NewIndex)
	{
		OptionSet->SetSelectedIndex(NewIndex, /*bInInteraction=*/false);
		LastCommittedIndex = NewIndex;
	}
}


void SGSOptionSlider::OnBeginSliderMovement()
{
	bInSliderInteraction = true;
	LastCommittedIndex = OnGetSelectedIndex();
#if WITH_EDITOR
	GEditor->BeginTransaction(LOCTEXT("EditNumericEntry", "Edit Number"));
#endif
}

void SGSOptionSlider::OnEndSliderMovement(int NewIndex)
{
	bInSliderInteraction = false;

	// force commit of value with transaction flags, for some reason (SNumericEntryBox does it, so...)
	if (LastCommittedIndex != NewIndex) {
		OptionSet->SetSelectedIndex(NewIndex, /*bInInteraction=*/false);
	}
	
	//else   // SNumericEntryBox has this in else branch but it doesn't make sense...wouldn't it leave the transaction open??
#if WITH_EDITOR
	{ 
		GEditor->EndTransaction();
	}
#endif
}



#undef LOCTEXT_NAMESPACE
