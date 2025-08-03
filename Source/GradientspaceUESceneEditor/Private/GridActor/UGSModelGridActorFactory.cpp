// Copyright Gradientspace Corp. All Rights Reserved.
#include "GridActor/UGSModelGridActorFactory.h"
#include "GridActor/ModelGridActor.h"
#include "GridActor/UGSModelGridAsset.h"
#include "AssetRegistry/AssetData.h"


#define LOCTEXT_NAMESPACE "UGSModelGridActorFactory"

UGSModelGridActorFactory::UGSModelGridActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("DisplayName", "ModelGrid Actor");
	NewActorClass = AGSModelGridActor::StaticClass();
}


void UGSModelGridActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	AGSModelGridActor* GridActor = CastChecked<AGSModelGridActor>(NewActor);
	UGSModelGridComponent* GridComponent = GridActor->GetGridComponent();
	check(GridComponent);

	GridComponent->UnregisterComponent();
	if (UGSModelGridAsset* GridAsset = Cast<UGSModelGridAsset>(Asset))
	{
		GridComponent->SetGridAsset(GridAsset);
	}
	GridComponent->RegisterComponent();
}


UObject* UGSModelGridActorFactory::GetAssetFromActorInstance(AActor* Instance)
{
	check(Instance->IsA(NewActorClass));
	if (AGSModelGridActor* GridActor = Cast<AGSModelGridActor>(Instance))
	{
		if (UGSModelGridComponent* GridComponent = GridActor->GetGridComponent())
		{
			return GridComponent->GridObjectAsset;
		}
	}
	return nullptr;
}


bool UGSModelGridActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (AssetData.IsValid()) 
	{
		UClass* AssetClass = AssetData.GetClass();
		if ( AssetClass != nullptr && AssetClass->IsChildOf(UGSModelGridAsset::StaticClass()) ) {
			return true;
		} else {
			OutErrorMsg = LOCTEXT("NoAsset", "No Grid Asset was specified");
			return false;
		}
	}
	return true;
}



#undef LOCTEXT_NAMESPACE
