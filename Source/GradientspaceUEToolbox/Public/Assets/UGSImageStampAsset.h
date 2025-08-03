// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "Assets/UGSImageStamp.h"
#include "Assets/UGSTextureImageStamp.h"
#include "UGSImageStampAsset.generated.h"


UCLASS(Abstract, MinimalAPI)
class UGSImageStampAsset : public UObject
{
	GENERATED_BODY()
public:

	virtual UGSImageStamp* GetImageStamp() const { return nullptr; }

};

UCLASS(MinimalAPI)
class UGSTextureImageStampAsset : public UGSImageStampAsset
{
	GENERATED_BODY()
public:
	UGSTextureImageStampAsset();

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Instanced, Category="ImageStamp")
	TObjectPtr<UGSTextureImageStamp> Stamp;


public:
	virtual UGSImageStamp* GetImageStamp() const override { 
		return Stamp; 
	}
};