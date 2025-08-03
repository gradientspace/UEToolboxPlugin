// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Factories/Factory.h"
#include "AssetDefinitionDefault.h"
#include "GridActor/UGSModelGridAsset.h"

#include "UGSModelGridAssetFactory.generated.h"

UCLASS(MinimalAPI, hidecategories = Object)
class UGSModelGridAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	virtual FText GetDisplayName() const override;
	virtual FString GetDefaultNewAssetName() const override;
	virtual bool ShouldShowInNewMenu() const override { return true; }
};


UCLASS()
class UAssetDefinition_UGSModelGridAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

private:
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_UGSModelGridAsset", "ModelGrid"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(175, 60, 128)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UGSModelGridAsset::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual FAssetSupportResponse CanLocalize(const FAssetData& InAsset) const { return FAssetSupportResponse::NotSupported(); }
};

