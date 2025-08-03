// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

#include "Core/GSError.h"

class SBorder;
class ITableRow;
class STableViewBase;

class GRADIENTSPACEUETOOLBOX_API SGSToolFeedbackWidget : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SGSToolFeedbackWidget)
	{}

	SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetFromErrorSet(const GS::GSErrorSet& ErrorSet);

protected:
	enum class EFeedbackType
	{
		Message = 0, Warning = 1, Error = 2
	};
	struct FeedbackItem
	{
		EFeedbackType FeedbackType = EFeedbackType::Error;
		FText Text;
	};

	TArray<TSharedPtr<FeedbackItem>> CurrentFeedbackItems;

	TSharedPtr<SWidget> Container;
	TSharedPtr<SBorder> WidgetContents;
	TSharedPtr<SListView<TSharedPtr<FeedbackItem>>> ListView;

	TSharedRef<ITableRow> MakeFeedbackItemWidget(TSharedPtr<FeedbackItem> ItemText, const TSharedRef<STableViewBase>& OwnerTable);

};
