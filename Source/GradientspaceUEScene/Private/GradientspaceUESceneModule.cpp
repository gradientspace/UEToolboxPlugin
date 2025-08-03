// Copyright Gradientspace Corp. All Rights Reserved.
#include "GradientspaceUESceneModule.h"
#include "UObject/CoreRedirects.h"

#define LOCTEXT_NAMESPACE "FGradientspaceUESceneModule"


void FGradientspaceUESceneModule::StartupModule()
{
	TArray<FCoreRedirect> Redirects;

	// old redirects for worldgrid plugin (not public)
	Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/Script/GradientspaceScene.WorldGridSettingsSubsystem"), TEXT("/Script/GradientspaceWorldGrid.WorldGridSettingsSubsystem"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/Script/GradientspaceScene.WorldGridSubsystem"), TEXT("/Script/GradientspaceWorldGrid.WorldGridSubsystem"));

	// redirects for rename of GradientspaceScene to GradientspaceUEScene
	Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/Script/GradientspaceScene.GSModelGrid"), TEXT("/Script/GradientspaceUEScene.GSModelGrid"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/Script/GradientspaceScene.GSModelGridAsset"), TEXT("/Script/GradientspaceUEScene.GSModelGridAsset"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/Script/GradientspaceScene.GSModelGridComponent"), TEXT("/Script/GradientspaceUEScene.GSModelGridComponent"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/Script/GradientspaceScene.GSModelGridActor"), TEXT("/Script/GradientspaceUEScene.GSModelGridActor"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/Script/GradientspaceScene.GSGridMaterialSet"), TEXT("/Script/GradientspaceUEScene.GSGridMaterialSet"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Struct, TEXT("/Script/GradientspaceScene.GSGridMaterialID"), TEXT("/Script/GradientspaceUEScene.GSGridMaterialID"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Struct, TEXT("/Script/GradientspaceScene.GSGridExternalMaterial"), TEXT("/Script/GradientspaceUEScene.GSGridExternalMaterial"));

	FCoreRedirects::AddRedirectList(Redirects, TEXT("GradientspaceScene"));

}

void FGradientspaceUESceneModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGradientspaceUESceneModule, GradientspaceUEScene)
