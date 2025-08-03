// Copyright Gradientspace Corp. All Rights Reserved.
#include "GradientspaceUEToolboxCommands.h"
#include "Styling/AppStyle.h"
#include "InputCoreTypes.h"
#include "GradientspaceUEToolboxStyle.h"

#define LOCTEXT_NAMESPACE "GradientspaceUEToolboxCommands"


FGradientspaceUEToolboxCommands::FGradientspaceUEToolboxCommands() :
	TCommands<FGradientspaceUEToolboxCommands>(
		"GradientspaceUEToolboxCommands", 
		NSLOCTEXT("Contexts", "GradientspaceUEToolboxCommands", "Gradientspace Toolbox"),
		NAME_None,
		FGradientspaceUEToolboxStyle::Get()->GetStyleSetName()
		)
{
}


void FGradientspaceUEToolboxCommands::RegisterCommands()
{
	UI_COMMAND(GradientspaceToolsTabButton, "Gradient", "gradientspace Toolbox", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(BeginHotSpotTool, "Hotspot", "Hotspot Texturing", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginMoverTool, "QuickMove", "Fast 3D transform tools (Tab hotkey)", EUserInterfaceActionType::ToggleButton, FInputChord());
	

	UI_COMMAND(BeginCreateModelGridTool, "Create Grid", "Create new ModelGrid Objects and Assets", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginImportModelGridTool, "Import Grid", "Import various file formats as a ModelGrid", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginModelGridEditorTool, "Grid Editor", "Edit ModelGrids", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginReshapeModelGridTool, "Reshape Grid", "Resize ModelGrids, Reposition Pivot, etc", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginModelGridUtilityTool, "Grid Utility", "Manage ModelGrid Components and Assets", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginModelGridMeshingTool, "Grid to Mesh", "Create Mesh Component from ModelGrid", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginRasterizeToGridTool, "Rasterize to Grid", "Rasterize Meshes into a ModelGrid", EUserInterfaceActionType::ToggleButton, FInputChord());
	
	
	UI_COMMAND(BeginCreateTextureTool, "Create Textures", "Create Texture Images", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginPixelPaintTool, "PixelPaint", "Paint Texture Image Pixels", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginTexturePaintTool, "TexturePaint", "Texture Painter", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginImportMeshTool, "Import Mesh", "Import a Mesh in various formats", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginExportMeshTool, "Export Meshes", "Export Meshes in various formats", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginParametricAssetTool, "Edit Parametrics", "Edit Parametric Asset Stuff", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginFeedbackTool, "Feedback", "Send Feedback to the Developers, and/or Get Help", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginSettingsTool, "Settings", "Configure Project & Editor Settings", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginDummyTool, " ", " ", EUserInterfaceActionType::ToggleButton, FInputChord());
	
}




#undef LOCTEXT_NAMESPACE
