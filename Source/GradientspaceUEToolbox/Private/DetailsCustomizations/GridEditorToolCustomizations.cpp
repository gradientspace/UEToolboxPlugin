// Copyright Gradientspace Corp. All Rights Reserved.

#include "DetailsCustomizations/GridEditorToolCustomizations.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

#include "ModelingWidgets/SComboPanel.h"
#include "ModelingWidgets/SToolInputAssetComboPanel.h"
#include "ModelingWidgets/ModelingCustomizationUtil.h"

#include "SFlyoutSelectionPanel.h"
#include "SOptionSlider.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SlateTextBlockLayout.h"
#include "Framework/Text/PlainTextLayoutMarshaller.h"

#include "GradientspaceUEToolboxStyle.h"

#include "Tools/ModelGridEditorTool.h"
#include "Materials/MaterialInterface.h"

#include "AssetToolsModule.h"		// for material icons

using namespace UE::ModelingUI;

#define LOCTEXT_NAMESPACE "GridEditorToolCustomizations"




TSharedRef<IDetailCustomization> FGridEditorPropertiesDetails::MakeInstance()
{
	return MakeShareable(new FGridEditorPropertiesDetails);
}

FGridEditorPropertiesDetails::~FGridEditorPropertiesDetails()
{
	if (TargetTool.IsValid())
	{
		//TargetTool.Get()->OnActiveEditingModeModified.Remove(EditingModeUpdateHandle);
	}
}

void FGridEditorPropertiesDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	check(ObjectsBeingCustomized.Num() > 0);
	UModelGridEditorToolProperties* ToolProperties = CastChecked<UModelGridEditorToolProperties>(ObjectsBeingCustomized[0]);
	UModelGridEditorTool* Tool = ToolProperties->Tool.Get();
	TargetTool = Tool;

	TSharedPtr<IPropertyHandle> EditingModePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UModelGridEditorToolProperties, EditingMode), UModelGridEditorToolProperties::StaticClass());
	ensure(EditingModePropertyHandle->IsValidHandle());

	DetailBuilder.EditDefaultProperty(EditingModePropertyHandle)->CustomWidget()
		.OverrideResetToDefault(FResetToDefaultOverride::Hide())
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(ModelingUIConstants::MultiWidgetRowHorzPadding, ModelingUIConstants::DetailRowVertPadding, 0, ModelingUIConstants::MultiWidgetRowHorzPadding))
			.HAlign(HAlign_Center)
			.FillWidth(1.0f)
			[
				MakeEditingModeWidget()->AsShared()
			]
		];
	
}


TSharedPtr<SWidget> FGridEditorPropertiesDetails::MakeEditingModeWidget()
{
	static const FText EditingModeLabels[] = {
		LOCTEXT("EditingModeModel", "Model"),  
		LOCTEXT("EditingModePaint", "Paint")
	};
	static const FText EditingModeTooltips[] = {
		LOCTEXT("EditingModeModelTooltip", "Modify Grid"),
		LOCTEXT("EditingModePaintTooltip", "Paint Grid"),
	};

	auto MakeEditingModeButton = [this](EModelGridEditorToolType EditingMode)
	{
		return SNew(SCheckBox)
			.Style(FAppStyle::Get(), "DetailsView.SectionButton")
			.Padding(FMargin(2, 2))
			.HAlign(HAlign_Center)
			.ToolTipText(EditingModeTooltips[(int32)EditingMode] )
			.OnCheckStateChanged_Lambda([this, EditingMode](ECheckBoxState State) {
				if (TargetTool.IsValid() && State == ECheckBoxState::Checked)
				{
					TargetTool.Get()->SetActiveEditingMode(EditingMode);
				}
			})
			.IsChecked_Lambda([this, EditingMode]() {
				return (TargetTool.IsValid() && TargetTool.Get()->GetActiveEditingMode() == EditingMode) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMargin(20, 2, 20, 2))
				.AutoWidth()
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					//.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					.TextStyle(FAppStyle::Get(), "LargeText")
					.Text(EditingModeLabels[(int32)EditingMode] )
				]
			];
	};


	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			MakeEditingModeButton(EModelGridEditorToolType::Model)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			MakeEditingModeButton(EModelGridEditorToolType::Paint)
		];
}




TSharedRef<IDetailCustomization> FGridEditorModelPropertiesDetails::MakeInstance()
{
	return MakeShareable(new FGridEditorModelPropertiesDetails);
}

FGridEditorModelPropertiesDetails::~FGridEditorModelPropertiesDetails()
{
	if (TargetTool.IsValid())
	{
		TargetTool.Get()->OnActiveCellTypeModified.Remove(CellTypeUpdateHandle);
		TargetTool.Get()->OnActiveDrawModeModified.Remove(DrawModeUpdateHandle);
	}
}

void FGridEditorModelPropertiesDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	check(ObjectsBeingCustomized.Num() > 0);
	UModelGridEditorTool_ModelProperties* ModelProperties = CastChecked<UModelGridEditorTool_ModelProperties>(ObjectsBeingCustomized[0]);
	UModelGridEditorTool* Tool = ModelProperties->Tool.Get();
	TargetTool = Tool;

	float ComboIconSize = 60;
	float FlyoutIconSize = 100;
	float FlyoutWidth = 840;


	// build combo flyout for cell type

	TSharedPtr<IPropertyHandle> CellTypePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UModelGridEditorTool_ModelProperties, CellType), UModelGridEditorTool_ModelProperties::StaticClass());
	ensure(CellTypePropertyHandle->IsValidHandle());

	TArray<UModelGridEditorTool::FCellTypeItem> CellTypes;
	TargetTool->GetKnownCellTypes(CellTypes);

	int32 CurrentCellTypeIndex = 0;
	TArray<TSharedPtr<SFlyoutSelectionPanel::FSelectableItem>> CellType_ComboItems;
	for (auto Item : CellTypes)
	{
		TSharedPtr<SFlyoutSelectionPanel::FSelectableItem> NewComboItem = MakeShared<SFlyoutSelectionPanel::FSelectableItem>();
		NewComboItem->Name = Item.Name;
		NewComboItem->Identifier = (int)Item.CellType;
		NewComboItem->Icon = FGradientspaceUEToolboxStyle::Get()->GetBrush(FName(*Item.StyleName));
		int NewItemIndex = CellType_ComboItems.Num();
		if (Item.CellType == ModelProperties->CellType)
		{
			CurrentCellTypeIndex = NewItemIndex;
		}
		CellType_ComboItems.Add(NewComboItem);
		CellTypeToIndexMap.Add((int)Item.CellType, NewItemIndex);
	}

	CellTypeCombo = SNew(SFlyoutSelectionPanel)
		.ToolTipText(CellTypePropertyHandle->GetToolTipText())
		.ComboButtonTileSize(FVector2D(ComboIconSize, ComboIconSize))
		.FlyoutTileSize(FVector2D(FlyoutIconSize, FlyoutIconSize))
		.FlyoutSize(FVector2D(FlyoutWidth, 1))
		.ListItems(CellType_ComboItems)
		.OnSelectionChanged_Lambda([this](TSharedPtr<SFlyoutSelectionPanel::FSelectableItem> NewSelectedItem) {
			if (TargetTool.IsValid())
			{
				TargetTool.Get()->SetActiveCellType( (EModelGridDrawCellType)NewSelectedItem->Identifier );
			}
		})
		.FlyoutHeaderText(LOCTEXT("CellTypeHeader", "Cell Types"))
		.InitialSelectionIndex(CurrentCellTypeIndex);

	CellTypeUpdateHandle = TargetTool.Get()->OnActiveCellTypeModified.AddLambda([this](EModelGridDrawCellType NewCellType) {
		CellTypeCombo->SetSelectionIndex( CellTypeToIndexMap[(int)NewCellType] );
	});



	// build combo flyout for draw mode

	TSharedPtr<IPropertyHandle> DrawModePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UModelGridEditorTool_ModelProperties, DrawMode), UModelGridEditorTool_ModelProperties::StaticClass());
	ensure(DrawModePropertyHandle->IsValidHandle());

	TArray<UModelGridEditorTool::FDrawModeItem> DrawModes;
	TargetTool->GetKnownDrawModes(DrawModes);

	int32 CurrentDrawModeIndex = 0;
	TArray<TSharedPtr<SComboPanel::FComboPanelItem>> DrawMode_ComboItems;
	for (auto Item : DrawModes)
	{
		TSharedPtr<SComboPanel::FComboPanelItem> NewComboItem = MakeShared<SComboPanel::FComboPanelItem>();
		NewComboItem->Name = Item.Name;
		NewComboItem->Identifier = (int)Item.DrawMode;
		NewComboItem->Icon = FGradientspaceUEToolboxStyle::Get()->GetBrush(FName(*Item.StyleName));
		int NewItemIndex = DrawMode_ComboItems.Num();
		if (Item.DrawMode == ModelProperties->DrawMode)
		{
			CurrentDrawModeIndex = NewItemIndex;
		}
		DrawMode_ComboItems.Add(NewComboItem);
		DrawModeToIndexMap.Add((int)Item.DrawMode, NewItemIndex);
	}

	DrawModeCombo = SNew(SComboPanel)
		.ToolTipText(DrawModePropertyHandle->GetToolTipText())
		.ComboButtonTileSize(FVector2D(ComboIconSize, ComboIconSize))
		.FlyoutTileSize(FVector2D(FlyoutIconSize, FlyoutIconSize))
		.FlyoutSize(FVector2D(FlyoutWidth, 1))
		.ListItems(DrawMode_ComboItems)
		.OnSelectionChanged_Lambda([this](TSharedPtr<SComboPanel::FComboPanelItem> NewSelectedItem) {
			if (TargetTool.IsValid())
			{
				TargetTool.Get()->SetActiveDrawMode( (EModelGridDrawMode)NewSelectedItem->Identifier );
			}
		})
		.FlyoutHeaderText(LOCTEXT("DrawModeHeader", "Draw Modes"))
		.InitialSelectionIndex(CurrentDrawModeIndex);

	DrawModeUpdateHandle = TargetTool.Get()->OnActiveDrawModeModified.AddLambda([this](EModelGridDrawMode NewDrawMode) {
		DrawModeCombo->SetSelectionIndex( DrawModeToIndexMap[(int)NewDrawMode] );
	});


	// build overall UI

	DetailBuilder.EditDefaultProperty(CellTypePropertyHandle)->CustomWidget()
		.OverrideResetToDefault(FResetToDefaultOverride::Hide())
		.WholeRowContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(FMargin(0, ModelingUIConstants::DetailRowVertPadding, 0, 0))
			.AutoHeight()
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Left)
				.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
				.Text(LOCTEXT("CellTypeUILabel", "Active Cell Type"))
			]
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
						CellTypeCombo->AsShared()
					]
				]
			]
		];

	// hide edittype handle
	TSharedPtr<IPropertyHandle> DrawEditTypePropertHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UModelGridEditorTool_ModelProperties, EditType), UModelGridEditorTool_ModelProperties::StaticClass());
	if ( ensure(DrawEditTypePropertHandle->IsValidHandle()) ) DrawEditTypePropertHandle->MarkHiddenByCustomization();
	TSharedPtr<IPropertyHandle> WorkPlanePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UModelGridEditorTool_ModelProperties, WorkPlane), UModelGridEditorTool_ModelProperties::StaticClass());
	if (ensure(WorkPlanePropertyHandle->IsValidHandle())) WorkPlanePropertyHandle->MarkHiddenByCustomization();
	TSharedPtr<IPropertyHandle> AutoOrientPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UModelGridEditorTool_ModelProperties, OrientationMode), UModelGridEditorTool_ModelProperties::StaticClass());
	if (ensure(AutoOrientPropertyHandle->IsValidHandle())) AutoOrientPropertyHandle->MarkHiddenByCustomization();

	DetailBuilder.EditDefaultProperty(DrawModePropertyHandle)->CustomWidget()
		.OverrideResetToDefault(FResetToDefaultOverride::Hide())
		.WholeRowContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(FMargin(0, ModelingUIConstants::DetailRowVertPadding, 0, 0))
			.AutoHeight()
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Left)
				.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
				.Text(LOCTEXT("DrawModeUILabel", "Active Tool"))
			]
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
						DrawModeCombo->AsShared()
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(ModelingUIConstants::MultiWidgetRowHorzPadding/2, ModelingUIConstants::DetailRowVertPadding, 0, ModelingUIConstants::MultiWidgetRowHorzPadding))
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().Padding(FMargin(0, ModelingUIConstants::DetailRowVertPadding, 0, 0)).AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							WrapInFixedWidthBox(DrawEditTypePropertHandle->CreatePropertyNameWidget(), 40)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							MakeDrawEditTypeWidget()->AsShared()
						]
					]
					+ SVerticalBox::Slot().Padding(FMargin(0, ModelingUIConstants::DetailRowVertPadding, 0, 0)).AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							WrapInFixedWidthBox(WorkPlanePropertyHandle->CreatePropertyNameWidget(), 40)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							MakeWorkPlaneWidget()->AsShared()
						]
					]
					+ SVerticalBox::Slot().Padding(FMargin(0, ModelingUIConstants::DetailRowVertPadding, 0, 0)).AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							WrapInFixedWidthBox(AutoOrientPropertyHandle->CreatePropertyNameWidget(), 40)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							MakeOrientationWidget()->AsShared()
						]
					]
				]
			]
		];
}




TSharedPtr<SWidget> FGridEditorModelPropertiesDetails::MakeDrawEditTypeWidget()
{
	static const FText DrawEditTypeLabels[] = {
		LOCTEXT("DrawEditModeAdd", "Add"),  
		LOCTEXT("DrawEditModeReplace", "Replace"),
		LOCTEXT("DrawEditModeErase", "Erase")
	};
	static const FText EditingModeTooltips[] = {
		LOCTEXT("DrawEditModeAddTooltip", "Add new cells but keep existing"),
		LOCTEXT("DrawEditModeReplaceTooltip", "Replace existing cells"),
		LOCTEXT("DrawEditModeEraseTooltip", "Erase cells")
	};

	auto MakeButton = [this](EModelGridDrawEditType EnumValue) {
		return SNew(SCheckBox)
			.Style(FAppStyle::Get(), "DetailsView.SectionButton").HAlign(HAlign_Center)
			.Padding(FMargin(2, 2))
			.ToolTipText(EditingModeTooltips[(int32)EnumValue] )
			.OnCheckStateChanged_Lambda([this, EnumValue](ECheckBoxState State) {
				if (TargetTool.IsValid() && State == ECheckBoxState::Checked) {
					TargetTool.Get()->SetActiveDrawEditType(EnumValue);
				}
			})
			.IsChecked_Lambda([this, EnumValue]() {
				return (TargetTool.IsValid() && TargetTool.Get()->GetActiveDrawEditType() == EnumValue) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().VAlign(VAlign_Center).AutoWidth()
				.Padding(FMargin(4, 1))
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					.Text(DrawEditTypeLabels[(int32)EnumValue] )
				]
			];
	};


	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f) [ MakeButton(EModelGridDrawEditType::Add) ]
		+ SHorizontalBox::Slot().FillWidth(1.0f) [ MakeButton(EModelGridDrawEditType::Replace) ]
		+ SHorizontalBox::Slot().FillWidth(1.0f) [ MakeButton(EModelGridDrawEditType::Erase) ];
}


TSharedPtr<SWidget> FGridEditorModelPropertiesDetails::MakeWorkPlaneWidget()
{
	static const FText Labels[] = {
		LOCTEXT("WorkPlaneAuto", "Auto"),  
		LOCTEXT("WorkPlaneX", "X"),
		LOCTEXT("WorkPlaneY", "Y"),
		LOCTEXT("WorkPlaneZ", "Z")
	};
	static const FText Tooltips[] = {
		LOCTEXT("WorkPlaneAutoTooltip", "Automatic or No work plane"),
		LOCTEXT("WorkPlaneXTooltip", "Work plane is YZ"),
		LOCTEXT("WorkPlaneYTooltip", "Work plane is XZ"),
		LOCTEXT("WorkPlaneZTooltip", "Work plane is XY")
	};
	using EnumTypeName = EModelGridWorkPlane;

	auto MakeButton = [this](EnumTypeName EnumValue) {
		return SNew(SCheckBox)
			.Style(FAppStyle::Get(), "DetailsView.SectionButton").HAlign(HAlign_Center)
			.Padding(FMargin(2, 2))
			.ToolTipText(Tooltips[(int32)EnumValue] )
			.OnCheckStateChanged_Lambda([this, EnumValue](ECheckBoxState State) {
				if (TargetTool.IsValid() && State == ECheckBoxState::Checked) {
					TargetTool.Get()->SetActiveWorkPlaneMode(EnumValue);
				}
			})
			.IsChecked_Lambda([this, EnumValue]() {
				return (TargetTool.IsValid() && TargetTool.Get()->GetActiveWorkPlaneMode() == EnumValue) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().VAlign(VAlign_Center).AutoWidth()
				.Padding(FMargin(4, 1))
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					.Text(Labels[(int32)EnumValue] )
				]
			];
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f) [ MakeButton(EModelGridWorkPlane::Auto) ]
		+ SHorizontalBox::Slot().FillWidth(0.8f) [ MakeButton(EModelGridWorkPlane::X) ]
		+ SHorizontalBox::Slot().FillWidth(0.8f) [ MakeButton(EModelGridWorkPlane::Y) ]
		+ SHorizontalBox::Slot().FillWidth(0.8f) [ MakeButton(EModelGridWorkPlane::Z) ];
}




TSharedPtr<SWidget> FGridEditorModelPropertiesDetails::MakeOrientationWidget()
{
	static const FText Labels[] = {
		LOCTEXT("OrientationModeFixed", "Fixed"),
		LOCTEXT("OrientationModeViewAligned", "View Aligned")
	};
	static const FText Tooltips[] = {
		LOCTEXT("OrientationModeFixedTooltip", "Constant cell orientation"),
		LOCTEXT("OrientationModeViewAlignedTooltip", "Align primary face of cell to be perpendicular to the view direction")
	};

	using EnumTypeName = EModelGridDrawOrientationType;

	auto MakeButton = [this](EnumTypeName EnumValue) {
		return SNew(SCheckBox)
			.Style(FAppStyle::Get(), "DetailsView.SectionButton").HAlign(HAlign_Center)
			.Padding(FMargin(2, 2))
			.ToolTipText(Tooltips[(int32)EnumValue] )
			.OnCheckStateChanged_Lambda([this, EnumValue](ECheckBoxState State) {
				if (TargetTool.IsValid() && State == ECheckBoxState::Checked) {
					TargetTool.Get()->SetActiveDrawOrientationMode(EnumValue);
				}
			})
			.IsChecked_Lambda([this, EnumValue]() {
				return (TargetTool.IsValid() && TargetTool.Get()->GetActiveDrawOrientationMode() == EnumValue) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().VAlign(VAlign_Center).AutoWidth()
				.Padding(FMargin(4, 1))
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					.Text(Labels[(int32)EnumValue] )
				]
			];
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f) [ MakeButton(EModelGridDrawOrientationType::Fixed) ]
		+ SHorizontalBox::Slot().FillWidth(0.8f) [ MakeButton(EModelGridDrawOrientationType::AlignedToView) ];
}




TSharedRef<IDetailCustomization> FGridEditorColorPropertiesDetails::MakeInstance()
{
	return MakeShareable(new FGridEditorColorPropertiesDetails);
}

FGridEditorColorPropertiesDetails::FGridEditorColorPropertiesDetails()
{
}

FGridEditorColorPropertiesDetails::~FGridEditorColorPropertiesDetails()
{
	//if (TargetTool.IsValid())
	//{
	//	TargetTool.Get()->OnActivePaintModeModified.Remove(PaintModeUpdateHandle);
	//}
}


void FGridEditorColorPropertiesDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	check(ObjectsBeingCustomized.Num() > 0);
	UModelGridEditorTool_ColorProperties* ColorProperties = CastChecked<UModelGridEditorTool_ColorProperties>(ObjectsBeingCustomized[0]);
	UModelGridEditorTool* Tool = ColorProperties->Tool.Get();
	TargetTool = Tool;

	ThumbnailPool = DetailBuilder.GetThumbnailPool();


	TSharedPtr<IPropertyHandle> SelectedMaterialIndexHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(UModelGridEditorTool_ColorProperties, SelectedMaterialIndex), UModelGridEditorTool_ColorProperties::StaticClass());
	ensure(SelectedMaterialIndexHandle->IsValidHandle());

	TSharedPtr<IPropertyHandle> AvailableMaterialsHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(UModelGridEditorTool_ColorProperties, AvailableMaterials), UModelGridEditorTool_ColorProperties::StaticClass());
	ensure(AvailableMaterialsHandle->IsValidHandle());

	//DetailBuilder.EditDefaultProperty(AvailableMaterialsHandle)->CustomWidget()
	//	//.OverrideResetToDefault(FResetToDefaultOverride::Hide())
	//	.WholeRowContent()
	//	[
	//		SNew(SListView<TSharedPtr<FGridMaterialListItem>>)
	//			.ItemHeight(24)
	//			.ListItemsSource(&ColorProperties->AvailableMaterials_RefList)
	//			.OnGenerateRow(this, &FGridEditorColorPropertiesDetails::GenerateItemRow)		// ugh can we do this w/ lambda or something?
	//			.OnSelectionChanged_Lambda([this](TSharedPtr< FGridMaterialListItem> NewItem, ESelectInfo::Type SelectInfo)
	//			{
	//				TargetTool.Get()->SetActiveSelectedMaterialByItem(*NewItem);
	//			})
	//	];



	int32 CurrentMaterialIndex = 0;
	TArray<TSharedPtr<SFlyoutSelectionPanel::FSelectableItem>> Material_ComboItems;
	for (TSharedPtr<FGridMaterialListItem> MaterialItem : ColorProperties->AvailableMaterials_RefList)
	{
		TSharedPtr<SFlyoutSelectionPanel::FSelectableItem> ComboItem = MakeShared<SFlyoutSelectionPanel::FSelectableItem>();
		ComboItem->Name = (MaterialItem->Name.Len() > 0) ? FText::FromString(MaterialItem->Name) : FText::FromString( MaterialItem->Material.GetName() );
		ComboItem->Identifier = MaterialItem->ToolItemIndex;
		ComboItem->Icon = nullptr;
		int NewItemIndex = Material_ComboItems.Num();
		if (MaterialItem->ToolItemIndex == ColorProperties->SelectedMaterialIndex)
		{
			CurrentMaterialIndex = NewItemIndex;
		}
		Material_ComboItems.Add(ComboItem);
		//CellTypeToIndexMap.Add((int)Item.CellType, NewItemIndex);
	}

	float ComboIconSize = 60;
	float FlyoutIconSize = 100;
	float FlyoutWidth = 840;

	SelectedMaterialFlyout = SNew(SFlyoutSelectionPanel)
		.ToolTipText(AvailableMaterialsHandle->GetToolTipText())
		.ComboButtonTileSize(FVector2D(ComboIconSize, ComboIconSize))
		.FlyoutTileSize(FVector2D(FlyoutIconSize, FlyoutIconSize))
		.FlyoutSize(FVector2D(FlyoutWidth, 1))
		.ListItems(Material_ComboItems)
		.OnSelectionChanged_Lambda([this](TSharedPtr<SFlyoutSelectionPanel::FSelectableItem> NewSelectedItem) {
			if (TargetTool.IsValid()) 
				TargetTool.Get()->SetActiveSelectedMaterialByIndex(NewSelectedItem->Identifier);
		})
		.FlyoutHeaderText(LOCTEXT("CellTypeHeader", "Cell Types"))
		.InitialSelectionIndex(CurrentMaterialIndex)
		.OnGenerateItem_Lambda([ColorProperties, FlyoutIconSize, this](SFlyoutSelectionPanel::FSelectableItem PanelItem)
		{
			TSharedPtr<FGridMaterialListItem>& MaterialItem = ColorProperties->AvailableMaterials_RefList[PanelItem.Identifier];
			UClass* ObjectClass = UMaterialInterface::StaticClass();
			TSharedPtr<SBorder> ThumbnailBorder;
			FAssetData AssetData(MaterialItem->Material);
			TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(AssetData, FlyoutIconSize, FlyoutIconSize, this->ThumbnailPool));
			FAssetThumbnailConfig AssetThumbnailConfig;
			return AssetThumbnail->MakeThumbnailWidget(AssetThumbnailConfig);
		});

	// TODO support tool telling us about type update...
	
	//CellTypeUpdateHandle = TargetTool.Get()->OnActiveCellTypeModified.AddLambda([this](EModelGridDrawCellType NewCellType) {
	//	CellTypeCombo->SetSelectionIndex(CellTypeToIndexMap[(int)NewCellType]);
	//});

	DetailBuilder.EditDefaultProperty(AvailableMaterialsHandle)->CustomWidget()
		.OverrideResetToDefault(FResetToDefaultOverride::Hide())
		.WholeRowContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(FMargin(0, ModelingUIConstants::DetailRowVertPadding, 0, 0))
			.AutoHeight()
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Left)
				.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
				.Text(LOCTEXT("MaterialsUILabel", "Active Material"))
			]
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
						SelectedMaterialFlyout->AsShared()
					]
				]
			]
		];


	SelectedMaterialIndexHandle->MarkHiddenByCustomization();
}


TSharedRef<SWidget> FGridEditorColorPropertiesDetails::GenerateMaterialThumbnail(
	FGridMaterialListItem& MaterialItem)
{
	UClass* ObjectClass = UMaterialInterface::StaticClass();
	uint32 ThumbnailIconSize = 32;
	TSharedPtr<SBorder> ThumbnailBorder;
	FAssetData AssetData(MaterialItem.Material);
	TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(AssetData, ThumbnailIconSize, ThumbnailIconSize, this->ThumbnailPool));

	FAssetThumbnailConfig AssetThumbnailConfig;
	TSharedPtr<IAssetTypeActions> AssetTypeActions;
	if (ObjectClass != nullptr)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(ObjectClass).Pin();

		if (AssetTypeActions.IsValid())
		{
			AssetThumbnailConfig.AssetTypeColorOverride = AssetTypeActions->GetTypeColor();
		}
	}

	return SNew(SBorder)
	.Visibility(EVisibility::SelfHitTestInvisible)
	.Padding(FMargin(0, 0, 4, 4))
	.BorderImage(FAppStyle::Get().GetBrush("PropertyEditor.AssetTileItem.DropShadow"))
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		.Padding(1)
		[
			SAssignNew(ThumbnailBorder, SBorder)
			.Padding(0)
			.BorderImage(FStyleDefaults::GetNoBrush())
			//.OnMouseDoubleClick(this, &SPropertyEditorAsset::OnAssetThumbnailDoubleClick)
			[
				SNew(SBox)
				//.ToolTipText(TooltipAttribute)
				.WidthOverride(ThumbnailIconSize)
				.HeightOverride(ThumbnailIconSize)
				[
					AssetThumbnail->MakeThumbnailWidget(AssetThumbnailConfig)
				]
			]
		]
		+ SOverlay::Slot()
		[
			SNew(SImage)
			//.Image(this, &SPropertyEditorAsset::GetThumbnailBorder)		// see SPropertyEditorAsset::GetThumbnailBorder()
			.Image(FAppStyle::Get().GetBrush(FName("PropertyEditor.AssetThumbnailBorder")))
			.Visibility(EVisibility::SelfHitTestInvisible)
		]
	];
}

TSharedRef<ITableRow> FGridEditorColorPropertiesDetails::GenerateItemRow(TSharedPtr<FGridMaterialListItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FText>>, OwnerTable)
	[
		GenerateMaterialThumbnail(*Item)
		//SNew(STextBlock)
		//	.Text( FText::FromString(Item->Material->GetName()) )
	];
}








TSharedRef<IDetailCustomization> FGridEditorPaintPropertiesDetails::MakeInstance()
{
	return MakeShareable(new FGridEditorPaintPropertiesDetails);
}

FGridEditorPaintPropertiesDetails::~FGridEditorPaintPropertiesDetails()
{
	if (TargetTool.IsValid())
	{
		TargetTool.Get()->OnActivePaintModeModified.Remove(PaintModeUpdateHandle);
	}
}

void FGridEditorPaintPropertiesDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	check(ObjectsBeingCustomized.Num() > 0);
	UModelGridEditorTool_PaintProperties* PaintProperties = CastChecked<UModelGridEditorTool_PaintProperties>(ObjectsBeingCustomized[0]);
	UModelGridEditorTool* Tool = PaintProperties->Tool.Get();
	TargetTool = Tool;

	float ComboIconSize = 60;
	float FlyoutIconSize = 100;
	float FlyoutWidth = 840;

	// build combo flyout for paint mode

	TSharedPtr<IPropertyHandle> PaintModePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UModelGridEditorTool_PaintProperties, PaintMode), UModelGridEditorTool_PaintProperties::StaticClass());
	ensure(PaintModePropertyHandle->IsValidHandle());

	TArray<UModelGridEditorTool::FPaintModeItem> PaintModes;
	TargetTool->GetKnownPaintModes(PaintModes);

	int32 CurrentPaintModeIndex = 0;
	TArray<TSharedPtr<SComboPanel::FComboPanelItem>> PaintMode_ComboItems;
	for (auto Item : PaintModes)
	{
		TSharedPtr<SComboPanel::FComboPanelItem> NewComboItem = MakeShared<SComboPanel::FComboPanelItem>();
		NewComboItem->Name = Item.Name;
		NewComboItem->Identifier = (int)Item.PaintMode;
		NewComboItem->Icon = FGradientspaceUEToolboxStyle::Get()->GetBrush(FName( *Item.StyleName ));
		int NewItemIndex = PaintMode_ComboItems.Num();
		if (Item.PaintMode == PaintProperties->PaintMode)
		{
			CurrentPaintModeIndex = NewItemIndex;
		}
		PaintMode_ComboItems.Add(NewComboItem);
		PaintModeToIndexMap.Add((int)Item.PaintMode, NewItemIndex);
	}

	PaintModeCombo = SNew(SComboPanel)
		.ToolTipText(PaintModePropertyHandle->GetToolTipText())
		.ComboButtonTileSize(FVector2D(ComboIconSize, ComboIconSize))
		.FlyoutTileSize(FVector2D(FlyoutIconSize, FlyoutIconSize))
		.FlyoutSize(FVector2D(FlyoutWidth, 1))
		.ListItems(PaintMode_ComboItems)
		.OnSelectionChanged_Lambda([this](TSharedPtr<SComboPanel::FComboPanelItem> NewSelectedItem) {
			if (TargetTool.IsValid())
			{
				TargetTool.Get()->SetActivePaintMode( (EModelGridPaintMode)NewSelectedItem->Identifier );
			}
		})
		.FlyoutHeaderText(LOCTEXT("PaintModeHeader", "Paint Modes"))
		.InitialSelectionIndex(CurrentPaintModeIndex);

	PaintModeUpdateHandle = TargetTool.Get()->OnActivePaintModeModified.AddLambda([this](EModelGridPaintMode NewPaintMode) {
		PaintModeCombo->SetSelectionIndex( PaintModeToIndexMap[(int)NewPaintMode] );
	});


	// build overall UI

	DetailBuilder.EditDefaultProperty(PaintModePropertyHandle)->CustomWidget()
		.OverrideResetToDefault(FResetToDefaultOverride::Hide())
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, ModelingUIConstants::DetailRowVertPadding))
			.AutoWidth()
			[
				SNew(SBox)
				.HeightOverride(ComboIconSize + 14)
				[
					PaintModeCombo->AsShared()
				]
			]
		];
}







TSharedRef<IDetailCustomization> FGridEditorRSTCellPropertiesDetails::MakeInstance()
{
	return MakeShareable(new FGridEditorRSTCellPropertiesDetails);
}

FGridEditorRSTCellPropertiesDetails::~FGridEditorRSTCellPropertiesDetails()
{
}

void FGridEditorRSTCellPropertiesDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	check(ObjectsBeingCustomized.Num() > 0);
	UModelGridStandardRSTCellProperties* RSTProperties = CastChecked<UModelGridStandardRSTCellProperties>(ObjectsBeingCustomized[0]);
	UModelGridEditorTool* Tool = RSTProperties->Tool.Get();
	TargetTool = Tool;

	// currently not actually doing any customizations, but will need to
}






#undef LOCTEXT_NAMESPACE
