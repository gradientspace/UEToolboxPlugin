// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "ThumbnailRendering/TextureThumbnailRenderer.h"
#include "Assets/UGSImageStampAsset.h"
#include "Engine/Texture2D.h"
#include "UGSImageStampThumbnails.generated.h"

UCLASS()
class UGSImageStampThumbnailRenderer : public UTextureThumbnailRenderer
{
	GENERATED_BODY()
public:
	virtual bool CanVisualizeAsset(UObject* Object) override {
		return GetThumbnailTextureOrNull(Object) != nullptr;
	}

	virtual void GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const override {
		Super::GetThumbnailSize(GetThumbnailTextureOrNull(Object), Zoom, OutWidth, OutHeight);
	}

	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override {
		Super::Draw(GetThumbnailTextureOrNull(Object), X, Y, Width, Height, RenderTarget, Canvas, bAdditionalViewFamily);
	}
protected:
	//! return a UTexture2D for the relevant asset type
	virtual UTexture2D* GetThumbnailTextureOrNull(UObject* Object) const {

		UTexture2D* UseTexture = nullptr;

		if (UGSTextureImageStampAsset* TextureAsset = Cast<UGSTextureImageStampAsset>(Object)) {
			if (TextureAsset->Stamp != nullptr)
				UseTexture = TextureAsset->Stamp->Texture;
		}

		if (UseTexture)
			UseTexture->FinishCachePlatformData();

		// possibly need to call these on the texture before returning...
		//FinishCachePlatformData();
		//UpdateResource();

		return UseTexture;
	}

};

