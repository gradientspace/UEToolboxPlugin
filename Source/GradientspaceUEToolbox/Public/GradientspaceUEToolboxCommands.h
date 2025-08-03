// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/**
 * TInteractiveToolCommands implementation for this module that provides standard Editor hotkey support
 */
class GRADIENTSPACEUETOOLBOX_API FGradientspaceUEToolboxCommands : public TCommands<FGradientspaceUEToolboxCommands>
{
public:
	FGradientspaceUEToolboxCommands();

	TSharedPtr<FUICommandInfo> GradientspaceToolsTabButton;

	TSharedPtr<FUICommandInfo> BeginHotSpotTool;
	TSharedPtr<FUICommandInfo> BeginMoverTool;

	TSharedPtr<FUICommandInfo> BeginModelGridEditorTool;
	TSharedPtr<FUICommandInfo> BeginModelGridUtilityTool;
	TSharedPtr<FUICommandInfo> BeginModelGridMeshingTool;
	TSharedPtr<FUICommandInfo> BeginRasterizeToGridTool;
	TSharedPtr<FUICommandInfo> BeginCreateModelGridTool;
	TSharedPtr<FUICommandInfo> BeginImportModelGridTool;
	TSharedPtr<FUICommandInfo> BeginReshapeModelGridTool;
	
	TSharedPtr<FUICommandInfo> BeginCreateTextureTool;
	TSharedPtr<FUICommandInfo> BeginPixelPaintTool;
	TSharedPtr<FUICommandInfo> BeginTexturePaintTool;

	TSharedPtr<FUICommandInfo> BeginImportMeshTool;
	TSharedPtr<FUICommandInfo> BeginExportMeshTool;

	TSharedPtr<FUICommandInfo> BeginParametricAssetTool;

	TSharedPtr<FUICommandInfo> BeginFeedbackTool;
	TSharedPtr<FUICommandInfo> BeginSettingsTool;
	TSharedPtr<FUICommandInfo> BeginDummyTool;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
