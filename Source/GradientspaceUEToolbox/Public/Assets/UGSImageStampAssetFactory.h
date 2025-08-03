// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Factories/Factory.h"
#include "AssetDefinitionDefault.h"
#include "Assets/UGSTextureImageStamp.h"
#include "Assets/UGSImageStampAsset.h"

#include "UGSImageStampAssetFactory.generated.h"

UCLASS(MinimalAPI, hidecategories = Object)
class UGSTextureImageStampAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	virtual FText GetDisplayName() const override;
	virtual FString GetDefaultNewAssetName() const override;
	virtual bool ShouldShowInNewMenu() const override { return true; }

	// optional initial texture
	UPROPERTY()
	TObjectPtr<UTexture2D> InitialTexture = nullptr;
};


UCLASS()
class UAssetDefinition_UGSTextureImageStampAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

private:
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_UGSTextureImageStampAsset", "Texture2D Stamp"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(175, 60, 128)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UGSTextureImageStampAsset::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual FAssetSupportResponse CanLocalize(const FAssetData& InAsset) const { return FAssetSupportResponse::NotSupported(); }
};

