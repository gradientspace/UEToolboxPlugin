// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "ActorFactories/ActorFactory.h"
#include "UGSModelGridActorFactory.generated.h"

class AActor;
struct FAssetData;

UCLASS()
class UGSModelGridActorFactory : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
};
