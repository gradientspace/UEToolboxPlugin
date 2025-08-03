// Copyright Gradientspace Corp. All Rights Reserved.
#include "DetailsCustomizations/OutputDirectorySettingsCustomization.h"

#include "Tools/ExportMeshTool.h"

#include "DetailWidgetRow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SDirectoryPicker.h"

TSharedRef<IPropertyTypeCustomization> FGSFolderPropertyCustomization::MakeInstance()
{
	return MakeShareable(new FGSFolderPropertyCustomization);
}

void FGSFolderPropertyCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const float XYZPadding = 10.f;

	TSharedPtr<IPropertyHandle> PathStringHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGSFolderProperty, Path));
	PathStringHandle->MarkHiddenByCustomization();

	FString InitialPath;
	PathStringHandle->GetValue(InitialPath);
	FPaths::NormalizeDirectoryName(InitialPath);

	HeaderRow.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0.f, 0.f, XYZPadding, 0.f)
		.FillWidth(1.0)
		[
			SNew(SDirectoryPicker)
				.Directory(InitialPath)
				.OnDirectoryChanged_Lambda([PathStringHandle](const FString& Directory) { PathStringHandle->SetValue(Directory); })
				
		]
	];
}

void FGSFolderPropertyCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

