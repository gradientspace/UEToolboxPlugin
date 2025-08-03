// Copyright Gradientspace Corp. All Rights Reserved.
#include "GridActor/UGSModelGridAssetFactory.h"

#include "Factories/Factory.h"
#include "AssetTypeCategories.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "UGSModelGridAssetFactory"

UGSModelGridAssetFactory::UGSModelGridAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UGSModelGridAsset::StaticClass();
	bCreateNew = true;
	bText = false;
	bEditAfterNew = false;
	bEditorImport = false;
}

UObject* UGSModelGridAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UGSModelGridAsset* NewAsset = NewObject<UGSModelGridAsset>(InParent, Class, Name, Flags);

	NewAsset->ModelGrid->InitializeNewGrid(false);

	check(NewAsset);
	return NewAsset;
}


uint32 UGSModelGridAssetFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.RegisterAdvancedAssetCategory("Gradientspace", LOCTEXT("AssetCategoryName", "Gradientspace"));
}

FText UGSModelGridAssetFactory::GetDisplayName() const
{
	return LOCTEXT("NewAssetMenuEntry", "ModelGrid Asset");
}

FString UGSModelGridAssetFactory::GetDefaultNewAssetName() const
{
	return TEXT("ModelGrid");
}


TConstArrayView<FAssetCategoryPath> UAssetDefinition_UGSModelGridAsset::GetAssetCategories() const 
{
	// should this return category above?
	static const auto Categories = { EAssetCategoryPaths::Misc };
	return Categories;
}



#undef LOCTEXT_NAMESPACE
