// Copyright Gradientspace Corp. All Rights Reserved.
#include "EditorIntegration/GSToolFeedbackWidget.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Internationalization/BreakIterator.h"



#define LOCTEXT_NAMESPACE "GSToolFeedbackWidget"



void SGSToolFeedbackWidget::Construct(const FArguments& InArgs)
{
	ChildSlot.SetHorizontalAlignment(EHorizontalAlignment::HAlign_Left);
	ChildSlot.SetVerticalAlignment(EVerticalAlignment::VAlign_Bottom);
	ChildSlot.SetPadding(FMargin(100.0f, 0.0f, 5.0f, 10.0f));
	ChildSlot
	[
		SAssignNew(WidgetContents, SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
		//.Padding(FMargin(3.0f, 6.0f, 3.f, 6.f))
		.Visibility_Lambda([this]() { return (CurrentFeedbackItems.Num() > 0) ? EVisibility::SelfHitTestInvisible : EVisibility::Hidden; })
		.Padding(8.f)
	];

	ListView = SNew(SListView<TSharedPtr<FeedbackItem>>)
		.ListItemsSource(&CurrentFeedbackItems)
		.OnGenerateRow(this, &SGSToolFeedbackWidget::MakeFeedbackItemWidget);

	WidgetContents->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(75)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				ListView.ToSharedRef()
			]
		]
	);

	// Don't block things under the overlay.
	SetVisibility(EVisibility::SelfHitTestInvisible);
}

//TSharedPtr<SWidget> SGSToolFeedbackWidget::Initialize()
//{
//	SAssignNew(ContainerWidget, SVerticalBox)
//		+ SVerticalBox::Slot()
//		.HAlign(HAlign_Right)
//		.VAlign(VAlign_Bottom)
//		.Padding(FMargin(2.0f, 0.0f, 5.0f, 5.0f))
//		[
//			SNew(SBorder)
//				.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
//				.Padding(FMargin(3.0f, 6.0f, 3.f, 6.f))
//				[

//				]
//		];
//
//	return ContainerWidget;
//
//}

void SGSToolFeedbackWidget::SetFromErrorSet(const GS::GSErrorSet& ErrorSet)
{
	CurrentFeedbackItems.Reset();
	for (const GS::GSError & Error : ErrorSet.Errors)
	{
		TSharedPtr<FeedbackItem> item = MakeShared<FeedbackItem>();
		FString str = ANSI_TO_TCHAR(Error.Message.c_str());
		item->Text = FText::FromString(str);
		if (Error.ErrorLevel == GS::EGSErrorLevel::Information)
			item->FeedbackType = EFeedbackType::Message;
		else if (Error.ErrorLevel == GS::EGSErrorLevel::Warning)
			item->FeedbackType = EFeedbackType::Warning;
		else
			item->FeedbackType = EFeedbackType::Error;

		CurrentFeedbackItems.Add(item);
	}
	ListView->RequestListRefresh();
}



TSharedRef<ITableRow> SGSToolFeedbackWidget::MakeFeedbackItemWidget(TSharedPtr<SGSToolFeedbackWidget::FeedbackItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FSlateColor UseColor = FSlateColor::UseForeground();
	if (Item->FeedbackType == EFeedbackType::Warning)
		UseColor = FSlateColor(FColor(235, 235, 16));
	else if (Item->FeedbackType == EFeedbackType::Error)
		UseColor = FSlateColor(FColor(235, 16, 16));

	//FLinearColor BackgroundColor = FSlateColor::UseB

	return SNew(STableRow<TSharedPtr<FeedbackItem>>, OwnerTable)
	[
		SNew(SBorder)
			.Padding(2)
			//.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			[
				SNew(STextBlock)
					.AutoWrapText(true)
					.MinDesiredWidth(400)
					.ColorAndOpacity(UseColor)
					.Text(Item->Text)
			]
	];
}



#undef LOCTEXT_NAMESPACE
