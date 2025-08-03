// Copyright Gradientspace Corp. All Rights Reserved.
#include "GradientspaceUEToolboxStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Interfaces/IPluginManager.h"
#include "SlateOptMacros.h"
#include "Styling/SlateStyleMacros.h"

#define IMAGE_PLUGIN_BRUSH( RelativePath, ... ) FSlateImageBrush( FGradientspaceUEToolboxStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )

// This is to fix the issue that SlateStyleMacros like IMAGE_BRUSH look for RootToContentDir but StyleSet->RootToContentDir is how this style is set up
#define RootToContentDir StyleSet->RootToContentDir

FString FGradientspaceUEToolboxStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("GradientspaceUEToolbox"))->GetContentDir();
	return (ContentDir / RelativePath) + Extension;
}

TSharedPtr<FSlateStyleSet> FGradientspaceUEToolboxStyle::StyleSet = nullptr;
TSharedPtr<class ISlateStyle> FGradientspaceUEToolboxStyle::Get() { return StyleSet; }

FName FGradientspaceUEToolboxStyle::GetStyleSetName()
{
	static FName ModelingToolsStyleName(TEXT("GradientspaceUEToolboxStyle"));
	return ModelingToolsStyleName;
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FGradientspaceUEToolboxStyle::Initialize()
{
	// Const icon sizes
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon120(120.0f, 120.0f);

	// Only register once
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	FString PluginContentDir = IPluginManager::Get().FindPlugin(TEXT("GradientspaceUEToolbox"))->GetContentDir();
	StyleSet->SetContentRoot(PluginContentDir);
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	{
		StyleSet->Set("GradientspaceUEToolboxCommands.GradientspaceToolsTabButton", new IMAGE_BRUSH_SVG("Icons/Gradientspace", Icon20x20));
		StyleSet->Set("GradientspaceUEToolboxCommands.GradientspaceToolsTabButton.Small", new IMAGE_BRUSH_SVG("Icons/Gradientspace", Icon20x20));



		StyleSet->Set("GridCellTypeIcons.Solid", new IMAGE_BRUSH("Icons/CellType_Solid", Icon120));
		StyleSet->Set("GridCellTypeIcons.Slab", new IMAGE_BRUSH("Icons/CellType_Slab", Icon120));
		StyleSet->Set("GridCellTypeIcons.Ramp", new IMAGE_BRUSH("Icons/CellType_Ramp", Icon120));
		StyleSet->Set("GridCellTypeIcons.Corner", new IMAGE_BRUSH("Icons/CellType_Corner", Icon120));
		StyleSet->Set("GridCellTypeIcons.Pyramid", new IMAGE_BRUSH("Icons/CellType_Pyramid", Icon120));
		StyleSet->Set("GridCellTypeIcons.Peak", new IMAGE_BRUSH("Icons/CellType_Peak", Icon120));
		StyleSet->Set("GridCellTypeIcons.CutCorner", new IMAGE_BRUSH("Icons/CellType_CutCorner", Icon120));
		StyleSet->Set("GridCellTypeIcons.Cylinder", new IMAGE_BRUSH("Icons/CellType_Cylinder", Icon120));

		StyleSet->Set("GridCellTypeIcons.BackgroundA", new IMAGE_BRUSH("Icons/CellType_Background", Icon120));
		StyleSet->Set("GridCellTypeIcons.BackgroundB", new IMAGE_BRUSH("Icons/CellType_Background_White", Icon120));

		//StyleSet->Set("GridDrawModeIcons.Single", new IMAGE_BRUSH_SVG("Icons/DrawMode_Single", Icon120));
		StyleSet->Set("GridDrawModeIcons.Single", new IMAGE_BRUSH("Icons/DrawMode_Single", Icon120));
		StyleSet->Set("GridDrawModeIcons.Brush2D", new IMAGE_BRUSH("Icons/DrawMode_Brush2D", Icon120));
		StyleSet->Set("GridDrawModeIcons.Brush3D", new IMAGE_BRUSH("Icons/DrawMode_Brush3D", Icon120));
		StyleSet->Set("GridDrawModeIcons.FillLayer", new IMAGE_BRUSH("Icons/DrawMode_AddLayer", Icon120));
		StyleSet->Set("GridDrawModeIcons.FloodFill2D", new IMAGE_BRUSH("Icons/DrawMode_FloodFill2D", Icon120));
		StyleSet->Set("GridDrawModeIcons.Rectangle2D", new IMAGE_BRUSH("Icons/DrawMode_Rectangle", Icon120));
		

		StyleSet->Set("GridPaintModeIcons.Single", new IMAGE_BRUSH("Icons/PaintMode_Single", Icon120));
		StyleSet->Set("GridPaintModeIcons.Brush2D", new IMAGE_BRUSH("Icons/PaintMode_Brush2D", Icon120));
		StyleSet->Set("GridPaintModeIcons.Brush3D", new IMAGE_BRUSH("Icons/PaintMode_Brush3D", Icon120));
		StyleSet->Set("GridPaintModeIcons.FillLayer", new IMAGE_BRUSH("Icons/PaintMode_FillLayer", Icon120));
		StyleSet->Set("GridPaintModeIcons.FillVolume", new IMAGE_BRUSH("Icons/PaintMode_FillVolume", Icon120));
		StyleSet->Set("GridPaintModeIcons.SingleFace", new IMAGE_BRUSH("Icons/PaintMode_SingleFace", Icon120));
		StyleSet->Set("GridPaintModeIcons.Rectangle2D", new IMAGE_BRUSH("Icons/DrawMode_Rectangle", Icon120));
		//StyleSet->Set("GridPaintModeIcons.SingleFace", new IMAGE_BRUSH("Icons/PaintMode_SingleFaceV2", Icon120));

		//StyleSet->Set("HairModelingToolCommands.BeginHotSpotTool", new IMAGE_BRUSH_SVG("Icons/HotSpotTool", Icon20x20));
		//StyleSet->Set("HairModelingToolCommands.BeginHotSpotTool.Small", new IMAGE_BRUSH_SVG("Icons/HotSpotTool", Icon20x20));
	}


	// make custom button style for FGSActionButtonGroup buttons, used in ActionButtonGroupCustomization - lets us control margin
	FButtonStyle ActionButtonStyle = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
	ActionButtonStyle.NormalPadding = FMargin(0, 0);
	ActionButtonStyle.PressedPadding = FMargin(0, 0);
	StyleSet->Set("ActionButtonStyle", ActionButtonStyle);
	// make variant w/ solid-white for button color. Button code defines the final color of the button background by multiplying various
	// other colors w/ this style color. So, this allows Button-creation/setup code to set a custom color that is not tinted down by the default dark-color.
	// (must use .ButtonColorAndOpacity() attribute on creation, or Button->SetBorderBackgroundColor() later
	FButtonStyle ActionButtonStyle_CustomColor = ActionButtonStyle;
	ActionButtonStyle_CustomColor.SetNormal(FSlateRoundedBoxBrush(FSlateColor(FLinearColor::White), 4.0f, FStyleColors::Input, 1.0f));
	ActionButtonStyle_CustomColor.SetHovered(FSlateRoundedBoxBrush(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)), 4.0f, FStyleColors::Input, 1.0f));
	ActionButtonStyle_CustomColor.SetPressed(FSlateRoundedBoxBrush(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)), 4.0f, FStyleColors::Input, 1.0f));
	StyleSet->Set("ActionButtonStyle_CustomColor", ActionButtonStyle_CustomColor);


	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
};

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef IMAGE_PLUGIN_BRUSH
#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef DEFAULT_FONT

void FGradientspaceUEToolboxStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}
