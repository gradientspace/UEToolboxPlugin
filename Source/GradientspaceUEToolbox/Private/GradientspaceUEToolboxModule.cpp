// Copyright Gradientspace Corp. All Rights Reserved.
#include "GradientspaceUEToolboxModule.h"
#include "Features/IModularFeatures.h"
#include "GradientspaceUEToolboxStyle.h"
#include "GradientspaceUEToolboxCommands.h"
#include "GradientspaceUEToolboxHotkeys.h"
#include "EditorIntegration/GSModelingModeSubsystem.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "DetailsCustomizations/GridEditorToolCustomizations.h"
#include "DetailsCustomizations/PaintToolCustomizations.h"
#include "DetailsCustomizations/OutputDirectorySettingsCustomization.h"
#include "PropertyTypes/ActionButtonGroup.h"
#include "DetailsCustomizations/ActionButtonGroupCustomization.h"
#include "PropertyTypes/ToggleButtonGroup.h"
#include "DetailsCustomizations/ToggleButtonGroupCustomization.h"
#include "PropertyTypes/OrderedOptionSet.h"
#include "DetailsCustomizations/OrderedOptionSetCustomization.h"
#include "PropertyTypes/EnumButtonsGroup.h"
#include "DetailsCustomizations/EnumButtonsGroupCustomization.h"
#include "PropertyTypes/GSColorPalette.h"
#include "DetailsCustomizations/GSColorPaletteCustomization.h"

#include "Tools/HotSpotTool.h"
#include "Tools/MoverTool.h"
#include "Tools/ModelGridEditorTool_Editor.h"
#include "Tools/ModelGridRasterizeTool.h"
#include "Tools/ModelGridUtilityTool.h"
#include "Tools/ModelGridMeshGenTool.h"
#include "Tools/ReshapeModelGridTool.h"
#include "Tools/CreateModelGridTool_Editor.h"
#include "Tools/CreateTextureTool.h"
#include "Tools/PixelPaintTool.h"
#include "Tools/GSTexturePaintTool.h"
#include "Tools/ImportGridTool.h"
#include "Tools/ImportMeshTool.h"
#include "Tools/ExportMeshTool.h"
#include "Tools/GradientspaceFeedbackTool.h"
#include "Tools/GradientspaceSettingsTool.h"
#include "Tools/DummyTool.h"
#include "Tools/ParametricAssetTool.h"

#include "Assets/UGSImageStampAsset.h"
#include "Assets/UGSImageStampThumbnails.h"
#include "ThumbnailRendering/ThumbnailManager.h"

#include "Platform/GSPlatformSubsystem.h"
#include "EditorIntegration/GSEditorPlatformAPI.h"

#define LOCTEXT_NAMESPACE "FGradientspaceUEToolboxModule"

void FGradientspaceUEToolboxModule::StartupModule()
{
	FGradientspaceModelingToolHotkeys::RegisterAllToolActions();
	FGradientspaceModelingModeHotkeys::Register();
	FGradientspaceUEToolboxStyle::Initialize();
	FGradientspaceUEToolboxCommands::Register();

	IModularFeatures::Get().RegisterModularFeature(IModelingModeToolExtension::GetModularFeatureName(), this);

	PropertiesToUnregisterOnShutdown.Reset();
	ClassesToUnregisterOnShutdown.Reset();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("ModelGridEditorToolProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FGridEditorPropertiesDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UModelGridEditorToolProperties::StaticClass()->GetFName());
	PropertyModule.RegisterCustomClassLayout("ModelGridEditorTool_ModelProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FGridEditorModelPropertiesDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UModelGridEditorTool_ModelProperties::StaticClass()->GetFName());
	PropertyModule.RegisterCustomClassLayout("ModelGridEditorTool_PaintProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FGridEditorPaintPropertiesDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UModelGridEditorTool_PaintProperties::StaticClass()->GetFName());
	PropertyModule.RegisterCustomClassLayout("ModelGridEditorTool_ColorProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FGridEditorColorPropertiesDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UModelGridEditorTool_ColorProperties::StaticClass()->GetFName());
	PropertyModule.RegisterCustomClassLayout("ModelGridStandardRSTCellProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FGridEditorRSTCellPropertiesDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UModelGridStandardRSTCellProperties::StaticClass()->GetFName());
	PropertyModule.RegisterCustomClassLayout("TextureSelectionSetSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FTextureSelectionSetSettingsDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UTextureSelectionSetSettings::StaticClass()->GetFName());


	PropertyModule.RegisterCustomPropertyTypeLayout("GSFolderProperty", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGSFolderPropertyCustomization::MakeInstance));
	PropertiesToUnregisterOnShutdown.Add(FGSFolderProperty::StaticStruct()->GetFName());
	PropertyModule.RegisterCustomPropertyTypeLayout("GSActionButtonGroup", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGSActionButtonGroupCustomization::MakeInstance));
	PropertiesToUnregisterOnShutdown.Add(FGSActionButtonGroup::StaticStruct()->GetFName());
	PropertyModule.RegisterCustomPropertyTypeLayout("GSToggleButtonGroup", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGSToggleButtonGroupCustomization::MakeInstance));
	PropertiesToUnregisterOnShutdown.Add(FGSToggleButtonGroup::StaticStruct()->GetFName());
	PropertyModule.RegisterCustomPropertyTypeLayout("GSOrderedOptionSet", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGSOrderedOptionSetCustomization::MakeInstance));
	PropertiesToUnregisterOnShutdown.Add(FGSOrderedOptionSet::StaticStruct()->GetFName());
	PropertyModule.RegisterCustomPropertyTypeLayout("GSEnumButtonsGroup", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGSEnumButtonsGroupCustomization::MakeInstance));
	PropertiesToUnregisterOnShutdown.Add(FGSEnumButtonsGroup::StaticStruct()->GetFName());
	PropertyModule.RegisterCustomPropertyTypeLayout("GSColorPalette", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGSColorPaletteCustomization::MakeInstance));
	PropertiesToUnregisterOnShutdown.Add(FGSColorPalette::StaticStruct()->GetFName());

	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FGradientspaceUEToolboxModule::OnAllModulesLoadingComplete);

	if (GIsEditor) {
		UThumbnailManager::Get().RegisterCustomRenderer(UGSImageStampAsset::StaticClass(), UGSImageStampThumbnailRenderer::StaticClass());
	}

	//FEditorModeTools& ModeTools = GLevelEditorModeTools();
}

void FGradientspaceUEToolboxModule::OnAllModulesLoadingComplete()
{
	// register platform API
	TSharedPtr<GS::GSEditorPlatformAPI> EditorPlatformAPI = MakeShared<GS::GSEditorPlatformAPI>();
	UGSPlatformSubsystem::PushPlatformAPI(EditorPlatformAPI);

	UGSModelingModeSubsystem::Get()->TrySwapToStartupEditorMode();
}


void FGradientspaceUEToolboxModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(IModelingModeToolExtension::GetModularFeatureName(), this);

	FGradientspaceModelingToolHotkeys::UnregisterAllToolActions();
	FGradientspaceUEToolboxCommands::Unregister();
	FGradientspaceModelingModeHotkeys::Unregister();

	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		for (FName ClassName : ClassesToUnregisterOnShutdown)
		{
			PropertyEditorModule->UnregisterCustomClassLayout(ClassName);
		}
		for (FName PropertyName : PropertiesToUnregisterOnShutdown)
		{
			PropertyEditorModule->UnregisterCustomPropertyTypeLayout(PropertyName);
		}
	}

	if (UObjectInitialized() && GIsEditor) {
		UThumbnailManager::Get().UnregisterCustomRenderer(UGSImageStampAsset::StaticClass());
	}

	FGradientspaceUEToolboxStyle::Shutdown();
}


FText FGradientspaceUEToolboxModule::GetExtensionName()
{
	return LOCTEXT("ExtensionName", "GradientspaceToolbox");
}

FText FGradientspaceUEToolboxModule::GetToolSectionName()
{
	return LOCTEXT("SectionName", "Gradientspace");
}

void FGradientspaceUEToolboxModule::GetExtensionTools(const FExtensionToolQueryInfo& QueryInfo, TArray<FExtensionToolDescription>& ToolsOut)
{
	//FExtensionToolDescription HotSpotToolInfo;
	//HotSpotToolInfo.ToolName = LOCTEXT("HotSpotUVTool", "HotSpot");
	//HotSpotToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginHotSpotTool;
	//HotSpotToolInfo.ToolBuilder = NewObject<UHotSpotToolBuilder>();
	//ToolsOut.Add(HotSpotToolInfo);

	int SpacerCounter = 0;
	auto AppendSpacerTool = [&]()
	{
		FExtensionToolDescription SpacerToolTmp;
		SpacerToolTmp.ToolName = FText::FromString(FString::Printf(TEXT("SpacerTool%d"), SpacerCounter++));
		SpacerToolTmp.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginDummyTool;
		SpacerToolTmp.ToolBuilder = NewObject<UDummyToolBuilder>();
		ToolsOut.Add(SpacerToolTmp);
	};
	auto AppendSectionBreak = [&]()
	{
		AppendSpacerTool();
		AppendSpacerTool();
		if (ToolsOut.Num() % 2 == 1)
			AppendSpacerTool();
	};


	FExtensionToolDescription ImportMeshToolInfo;
	ImportMeshToolInfo.ToolName = LOCTEXT("ImportMeshTool", "Mesh Import");
	ImportMeshToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginImportMeshTool;
	ImportMeshToolInfo.ToolBuilder = NewObject<UImportMeshToolBuilder>();
	ToolsOut.Add(ImportMeshToolInfo);

	FExtensionToolDescription ExportMeshToolInfo;
	ExportMeshToolInfo.ToolName = LOCTEXT("ExportMeshTool", "Mesh Export");
	ExportMeshToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginExportMeshTool;
	ExportMeshToolInfo.ToolBuilder = NewObject<UGSExportMeshesToolBuilder>();
	ToolsOut.Add(ExportMeshToolInfo);

	FExtensionToolDescription MoverToolInfo;
	MoverToolInfo.ToolName = LOCTEXT("Mover", "QuickMove");
	MoverToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginMoverTool;
	MoverToolInfo.ToolBuilder = NewObject<UGSMoverToolBuilder>();
	ToolsOut.Add(MoverToolInfo);

	AppendSectionBreak();

	FExtensionToolDescription CreateGridToolInfo;
	CreateGridToolInfo.ToolName = LOCTEXT("CreateGrid", "Create Grid");
	CreateGridToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginCreateModelGridTool;
	CreateGridToolInfo.ToolBuilder = NewObject<UCreateModelGridToolEditorBuilder>();
	ToolsOut.Add(CreateGridToolInfo);

	FExtensionToolDescription ImportGridToolInfo;
	ImportGridToolInfo.ToolName = LOCTEXT("ImportGridTool", "Import Grid");
	ImportGridToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginImportModelGridTool;
	ImportGridToolInfo.ToolBuilder = NewObject<UModelGridImportToolBuilder>();
	ToolsOut.Add(ImportGridToolInfo);

	FExtensionToolDescription RasterizeToGridToolInfo;
	RasterizeToGridToolInfo.ToolName = LOCTEXT("RasterizeToGridTool", "Rasterize To Grid");
	RasterizeToGridToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginRasterizeToGridTool;
	RasterizeToGridToolInfo.ToolBuilder = NewObject<UModelGridRasterizeToolBuilder>();
	ToolsOut.Add(RasterizeToGridToolInfo);

	FExtensionToolDescription EditGridToolInfo;
	EditGridToolInfo.ToolName = LOCTEXT("GridTool", "Grid Editor");
	EditGridToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginModelGridEditorTool;
	EditGridToolInfo.ToolBuilder = NewObject<UModelGridEditorToolEditorBuilder>();
	ToolsOut.Add(EditGridToolInfo);

	FExtensionToolDescription ReshapeGridToolInfo;
	ReshapeGridToolInfo.ToolName = LOCTEXT("ReshapeGridTool", "Reshape Grid");
	ReshapeGridToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginReshapeModelGridTool;
	ReshapeGridToolInfo.ToolBuilder = NewObject<UReshapeModelGridToolBuilder>();
	ToolsOut.Add(ReshapeGridToolInfo);

	FExtensionToolDescription GridUtilityToolInfo;
	GridUtilityToolInfo.ToolName = LOCTEXT("GridUtilityTool", "Grid Utilities");
	GridUtilityToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginModelGridUtilityTool;
	GridUtilityToolInfo.ToolBuilder = NewObject<UModelGridUtilityToolBuilder>();
	ToolsOut.Add(GridUtilityToolInfo);

	FExtensionToolDescription GridMesherToolInfo;
	GridMesherToolInfo.ToolName = LOCTEXT("GridMesherTool", "Grid To Mesh");
	GridMesherToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginModelGridMeshingTool;
	GridMesherToolInfo.ToolBuilder = NewObject<UModelGridMeshGenToolBuilder>();
	ToolsOut.Add(GridMesherToolInfo);

	AppendSectionBreak();

	FExtensionToolDescription CreateTextureToolInfo;
	CreateTextureToolInfo.ToolName = LOCTEXT("CreateTextureTool", "Make Tex");
	CreateTextureToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginCreateTextureTool;
	CreateTextureToolInfo.ToolBuilder = NewObject<UGSCreateTextureToolBuilder>();
	ToolsOut.Add(CreateTextureToolInfo);

	FExtensionToolDescription TexturePaintToolInfo;
	TexturePaintToolInfo.ToolName = LOCTEXT("TexturePaintTool", "TexturePaint");
	TexturePaintToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginTexturePaintTool;
	TexturePaintToolInfo.ToolBuilder = NewObject<UGSMeshTexturePaintToolBuilder>();
	ToolsOut.Add(TexturePaintToolInfo);

	FExtensionToolDescription PixelPaintToolInfo;
	PixelPaintToolInfo.ToolName = LOCTEXT("PixelPaintTool", "PixelPaint");
	PixelPaintToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginPixelPaintTool;
	PixelPaintToolInfo.ToolBuilder = NewObject<UGSMeshPixelPaintToolBuilder>();
	ToolsOut.Add(PixelPaintToolInfo);

	AppendSectionBreak();

	FExtensionToolDescription ParametricAssetToolInfo;
	ParametricAssetToolInfo.ToolName = LOCTEXT("ParametricAssetTool", "Edit Parametrics");
	ParametricAssetToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginParametricAssetTool;
	ParametricAssetToolInfo.ToolBuilder = NewObject<UGSParametricAssetToolBuilder>();
	ToolsOut.Add(ParametricAssetToolInfo);

	AppendSectionBreak();

	FExtensionToolDescription FeedbackToolInfo;
	FeedbackToolInfo.ToolName = LOCTEXT("FeedbackTool", "Feedback/Help");
	FeedbackToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginFeedbackTool;
	FeedbackToolInfo.ToolBuilder = NewObject<UGradientspaceFeedbackToolBuilder>();
	ToolsOut.Add(FeedbackToolInfo);

	FExtensionToolDescription SettingsToolInfo;
	SettingsToolInfo.ToolName = LOCTEXT("SettingsTool", "Settings");
	SettingsToolInfo.ToolCommand = FGradientspaceUEToolboxCommands::Get().BeginSettingsTool;
	SettingsToolInfo.ToolBuilder = NewObject<UGradientspaceSettingsToolBuilder>();
	ToolsOut.Add(SettingsToolInfo);

	UGSModelingModeSubsystem::Get()->BeginListeningForModeChanges();

}

bool FGradientspaceUEToolboxModule::GetExtensionExtendedInfo(FModelingModeExtensionExtendedInfo& InfoOut)
{
	InfoOut.ExtensionCommand = FGradientspaceUEToolboxCommands::Get().GradientspaceToolsTabButton;
	InfoOut.ToolPaletteButtonTooltip = LOCTEXT("GradienspaceToolboxExtensionTooltip", "Tools from Gradientspace!");
	return true;
}



#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGradientspaceUEToolboxModule, GradientspaceUEToolbox)
