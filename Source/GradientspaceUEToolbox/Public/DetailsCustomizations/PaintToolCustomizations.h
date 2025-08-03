// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class UPixelPaintTool;
class UTextureSelectionSetSettings;
class SFlyoutSelectionPanel;
class SWidget;
class STextBlock;
class UTexture2D;
class UMaterialInterface;

class FTextureSelectionSetSettingsDetails : public IDetailCustomization
{
public:
	virtual ~FTextureSelectionSetSettingsDetails();

	static constexpr float ComboIconSize = 60;
	static constexpr float FlyoutIconSize = 100;
	static constexpr float FlyoutWidth = 840;

	static TSharedRef<IDetailCustomization> MakeInstance();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
protected:
	TWeakObjectPtr<UTextureSelectionSetSettings> ActiveSettings;

	TSharedPtr<SFlyoutSelectionPanel> TexturePicker;
	TMap<int, int> TextureUniqueIDToIndexMap;
	FDelegateHandle TexturePickerModifiedHandle;

	TSharedPtr<SWidget> MakeTexturePickerWidget(const FText& UseTooltipText);
	TSharedPtr<SWidget> MakeTextureInfoWidget();
	TSharedPtr<STextBlock> TexPathInfo;
	TSharedPtr<STextBlock> MatPathInfo;
	TSharedPtr<STextBlock> SourceFormat;
	TSharedPtr<STextBlock> PlatformFormat;
	void UpdateTextureInfo();
};