// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "IPropertyTypeCustomization.h"

class SBox;

/** Details customization for FGSOrderedOptionSet struct*/
class FGSOrderedOptionSetCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	TSharedPtr<SBox> SliderContainer;
};
