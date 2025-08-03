// Copyright Gradientspace Corp. All Rights Reserved.
#include "Utility/GSUEMaterialUtil.h"

#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialShared.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"

bool GS::AssignMaterialToTarget(UMaterialInterface* Material, int MaterialIndex, UObject* ActorOrComponent, UObject* Asset, bool bUpdateAssetAndComponent)
{
	if (MaterialIndex < 0) 
		return false;
	bool bAssignedToAsset = false;
	if (Asset != nullptr)
	{
		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset)) {
			TArray<FStaticMaterial> AssetMaterials = StaticMesh->GetStaticMaterials();
			if (MaterialIndex < AssetMaterials.Num()) {
				AssetMaterials[MaterialIndex].MaterialInterface = Material;
				StaticMesh->Modify();
				StaticMesh->SetStaticMaterials(AssetMaterials);
				bAssignedToAsset = true;
			}
		}
	}
	if (bAssignedToAsset && bUpdateAssetAndComponent == false)
		return true;
	if (ActorOrComponent == nullptr) 
		return false;

	UPrimitiveComponent* AssignToComponent = nullptr;
	if (AActor* Actor = Cast<AActor>(ActorOrComponent))
		AssignToComponent = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
	else if (UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(ActorOrComponent))
		AssignToComponent = Component;
	if (!AssignToComponent)
		return false;

	if (MaterialIndex >= AssignToComponent->GetNumMaterials())
		return false;

	AssignToComponent->Modify();
	AssignToComponent->SetMaterial(MaterialIndex, Material);
	return true;
}


bool GS::SetMaterialTextureParameter(UMaterialInterface* MaterialInterface, const FString& ParameterName, UTexture2D* Texture)
{
	if (Texture == nullptr || MaterialInterface == nullptr)
		return false;

	if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MaterialInterface))
	{
		MID->SetTextureParameterValue(*ParameterName, Texture);
		return true;		// no way to detect failure here??
	}
	else if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MaterialInterface))
	{
#if WITH_EDITOR
		MIC->SetTextureParameterValueEditorOnly(*ParameterName, Texture);
#else
		return false;
#endif
	}
	else if (UMaterial* Material = Cast<UMaterial>(MaterialInterface))
	{
#if WITH_EDITOR
		Material->SetTextureParameterValueEditorOnly(*ParameterName, Texture);
#else
		return false;
#endif
	}
	return false;
}



void GS::DoInternalMaterialUpdates(UMaterialInterface* Material)
{
	// what to do in non-editor? anything?
#if WITH_EDITOR	
	if (Material != nullptr) {
		FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::SyncWithRenderingThread);
		UpdateContext.AddMaterialInterface(Material);
		Material->PreEditChange(NULL);
		Material->PostEditChange();
	}
#endif
}