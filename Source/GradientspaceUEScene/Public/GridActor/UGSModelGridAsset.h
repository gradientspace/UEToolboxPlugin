// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GridActor/UGSModelGrid.h"
#include "GridActor/UGSGridMaterialSet.h"

#include "UGSModelGridAsset.generated.h"

UCLASS(BlueprintType, MinimalAPI)
class UGSModelGridAsset : public UObject
{
	GENERATED_BODY()

public:
	UGSModelGridAsset();

	/**
	 * ModelGrid for this Asset
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Instanced, Category=Grid)
	TObjectPtr<UGSModelGrid> ModelGrid;

	/**
	 * Grid Material Set used for the associated ModelGrid stored in this Asset
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Instanced, Category=Grid)
	TObjectPtr<UGSGridMaterialSet> InternalMaterialSet;


public:

	UFUNCTION(BlueprintCallable, Category = ModelGridAsset)
	GRADIENTSPACEUESCENE_API UGSModelGrid* GetGrid() {
		return ModelGrid;
	}

};
