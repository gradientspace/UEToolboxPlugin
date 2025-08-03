// Copyright Gradientspace Corp. All Rights Reserved.
#include "Assets/UGSImageStampAssetFactory.h"

#include "Factories/Factory.h"
#include "AssetTypeCategories.h"
#include "AssetToolsModule.h"

#include "Engine/Texture2D.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ContentBrowserMenuContexts.h"

#include "GradientspaceUEToolboxStyle.h"

#define LOCTEXT_NAMESPACE "UGSImageStampAssetFactory"

UGSTextureImageStampAssetFactory::UGSTextureImageStampAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UGSTextureImageStampAsset::StaticClass();
	bCreateNew = true;
	bText = false;
	bEditAfterNew = false;
	bEditorImport = false;
}

UObject* UGSTextureImageStampAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UGSTextureImageStampAsset* NewAsset = NewObject<UGSTextureImageStampAsset>(InParent, Class, Name, Flags);
	ensure(NewAsset);

	if (NewAsset && InitialTexture != nullptr)
		NewAsset->Stamp->Texture = InitialTexture;
	return NewAsset;
}


uint32 UGSTextureImageStampAssetFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.RegisterAdvancedAssetCategory("Gradientspace", LOCTEXT("AssetCategoryName", "Gradientspace"));
}
FText UGSTextureImageStampAssetFactory::GetDisplayName() const {
	return LOCTEXT("NewAssetMenuEntry", "Texture Stamp Asset");
}
FString UGSTextureImageStampAssetFactory::GetDefaultNewAssetName() const {
	return TEXT("TextureStamp");
}
TConstArrayView<FAssetCategoryPath> UAssetDefinition_UGSTextureImageStampAsset::GetAssetCategories() const 
{
	static const auto Categories = { EAssetCategoryPaths::Misc };
	return Categories;
}


namespace MenuExtension_GSTextureImageStamp
{

void CreateStampFromTexture(const FToolMenuContext& MenuContext)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	UGSTextureImageStampAssetFactory* Factory = NewObject<UGSTextureImageStampAssetFactory>();
	FString DefaultSuffix = TEXT("_Stamp");

	// find selected textures from the CB
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	TArray<UTexture2D*> SelectedTextures = Context->LoadSelectedObjects<UTexture2D>();
		
	TArray<UObject*> StampsToSync;
	for (UTexture2D* Texture : SelectedTextures)
	{
		FString NewAssetName, NewPackageName;
		IAssetTools::Get().CreateUniqueAssetName(Texture->GetOutermost()->GetName(), DefaultSuffix, NewPackageName, NewAssetName);
		Factory->InitialTexture = Texture;
		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(
			NewAssetName, FPackageName::GetLongPackagePath(NewPackageName), UGSTextureImageStampAsset::StaticClass(), Factory);
		if( NewAsset )
			StampsToSync.Add(NewAsset);
	}

	if(StampsToSync.Num() > 0)
		IAssetTools::Get().SyncBrowserToAssets(StampsToSync);
}


// this is a kinda weird UE idiom where this static variable will, on construction, register
// this callback to be run at some point during engine initialization...
static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, [] {
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UTexture2D::StaticClass());
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			// disabling permission stuff, is that some UEFN-related thing?
			//const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection);
			//TSharedRef<FPathPermissionList> ClassPathPermissionList = IAssetTools::Get().GetAssetClassPathPermissionList(EAssetClassAction::CreateAsset);

			//if (ClassPathPermissionList->PassesFilter(USlateBrushAsset::StaticClass()->GetClassPathName().ToString()))
			//{
				TAttribute<FText> Label = LOCTEXT("Texture2D_GSCreateTextureStamp", "Create Gradientspace Stamp");
				TAttribute<FText> ToolTip = LOCTEXT("Texture2D_GSCreateTextureStampTooltip", "Creates a new Gradientspace TextureStamp asset");
				FSlateIcon Icon(FGradientspaceUEToolboxStyle::GetStyleSetName(), "GradientspaceUEToolboxCommands.GradientspaceToolsTabButton");
				//FSlateIcon Icon;		// will this use a default or just no icon?
				FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateStatic(&CreateStampFromTexture);

				InSection.AddMenuEntry("Texture2D_GSCreateTextureStamp", Label, ToolTip, Icon, UIAction);
			//}
		}));
	}));
});

}

#undef LOCTEXT_NAMESPACE
