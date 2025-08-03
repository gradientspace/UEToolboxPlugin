// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "IPropertyTypeCustomization.h"

class SWrapBox;
class SBox;
struct FGSColorPalette;

/** Details customization for FGSColorPalette struct*/
class FGSColorPaletteCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	TSharedPtr<SBox> PaletteWidget;
	void UpdatePalette(const FGSColorPalette* Palette);
};