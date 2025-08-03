// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

#include "Widgets/Views/SListView.h"

class UModelGridEditorTool;
class SComboPanel;
class SFlyoutSelectionPanel;
class SWidget;
class ITableRow;
class STableViewBase;
class FAssetThumbnailPool;
struct FGridMaterialListItem;


class FGridEditorPropertiesDetails : public IDetailCustomization
{
public:
	virtual ~FGridEditorPropertiesDetails();

	static TSharedRef<IDetailCustomization> MakeInstance();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
protected:
	TWeakObjectPtr<UModelGridEditorTool> TargetTool;
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;

	//FDelegateHandle EditingModeUpdateHandle;
	TSharedPtr<SWidget> MakeEditingModeWidget();
};



class FGridEditorModelPropertiesDetails : public IDetailCustomization
{
public:
	virtual ~FGridEditorModelPropertiesDetails();

	static TSharedRef<IDetailCustomization> MakeInstance();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	TWeakObjectPtr<UModelGridEditorTool> TargetTool;
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;

	TSharedPtr<SFlyoutSelectionPanel> CellTypeCombo;
	FDelegateHandle CellTypeUpdateHandle;
	TMap<int, int> CellTypeToIndexMap;

	TSharedPtr<SComboPanel> DrawModeCombo;
	FDelegateHandle DrawModeUpdateHandle;
	TMap<int, int> DrawModeToIndexMap;

	//FDelegateHandle DrawEditTypeUpdateHandle;
	TSharedPtr<SWidget> MakeDrawEditTypeWidget();
	TSharedPtr<SWidget> MakeWorkPlaneWidget();
	TSharedPtr<SWidget> MakeOrientationWidget();
};



class FGridEditorColorPropertiesDetails : public IDetailCustomization
{
public:
	FGridEditorColorPropertiesDetails();
	virtual ~FGridEditorColorPropertiesDetails();

	static TSharedRef<IDetailCustomization> MakeInstance();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	TWeakObjectPtr<UModelGridEditorTool> TargetTool;
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;

	TSharedPtr<SFlyoutSelectionPanel> SelectedMaterialFlyout;

	TSharedPtr<SListView<FGridMaterialListItem>> SelectedMaterialListView;
	//FDelegateHandle PaintModeUpdateHandle;
	//TMap<int, int> PaintModeToIndexMap;

	TSharedRef<ITableRow> GenerateItemRow(TSharedPtr<FGridMaterialListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	TSharedRef<SWidget> GenerateMaterialThumbnail(
		FGridMaterialListItem& MaterialItem);
};



class FGridEditorPaintPropertiesDetails : public IDetailCustomization
{
public:
	virtual ~FGridEditorPaintPropertiesDetails();

	static TSharedRef<IDetailCustomization> MakeInstance();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	TWeakObjectPtr<UModelGridEditorTool> TargetTool;
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;

	TSharedPtr<SComboPanel> PaintModeCombo;
	FDelegateHandle PaintModeUpdateHandle;
	TMap<int, int> PaintModeToIndexMap;
};






class FGridEditorRSTCellPropertiesDetails : public IDetailCustomization
{
public:
	virtual ~FGridEditorRSTCellPropertiesDetails();

	static TSharedRef<IDetailCustomization> MakeInstance();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	TWeakObjectPtr<UModelGridEditorTool> TargetTool;
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
};

