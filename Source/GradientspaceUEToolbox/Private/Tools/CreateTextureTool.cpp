// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/CreateTextureTool.h"
#include "InteractiveToolManager.h"
#include "ModelingObjectsCreationAPI.h"

#include "Engine/World.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Misc/DefaultValueHelper.h"

#include "PixelFormat.h"
#include "RenderUtils.h"
#include "ImageCoreUtils.h"
#include "ImageUtils.h"
#include "TextureCompiler.h"

#include "Utility/GSTextureUtil.h"
#include "Math/GSMath.h"


#define LOCTEXT_NAMESPACE "UGSCreateTextureTool"

using namespace UE::Geometry;

UInteractiveTool* UGSCreateTextureToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UGSCreateTextureTool* NewTool = NewObject<UGSCreateTextureTool>(SceneState.ToolManager);
	NewTool->SetTargetWorld(SceneState.World);
	return NewTool;
}


UGSCreateTextureTool::UGSCreateTextureTool()
{
	SetToolDisplayName(LOCTEXT("ToolName", "Create Textures"));
}

void UGSCreateTextureTool::SetTargetWorld(UWorld* World) {
	TargetWorld = World;
}


void UGSCreateTextureTool::Setup()
{
	UInteractiveTool::Setup();

	Settings = NewObject<UGSCreateTextureToolSettings>(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);

	Settings->Resolution.AddOptions({ TEXT("1"), TEXT("2"), TEXT("4"), TEXT("8"),
		TEXT("16"), TEXT("32"), TEXT("64"), TEXT("128"), TEXT("256"), TEXT("512"), TEXT("1024"), TEXT("2048"), TEXT("4096")});
	Settings->Resolution.AddOptions({ TEXT("8192") });
	Settings->Resolution.SelectedIndex = Settings->SavedDefaultResolutionIndex;
	Settings->WatchProperty(Settings->Resolution.SelectedIndex, [&](int) { UpdateOnResolutionChange(); });
	Settings->WatchProperty(Settings->CustomWidth, [&](int) { UpdateOnResolutionChange(); });
	Settings->WatchProperty(Settings->CustomHeight, [&](int) { UpdateOnResolutionChange(); });
	Settings->WatchProperty(Settings->bCustomDimensions, [&](bool) { UpdateOnResolutionChange(); });
	Settings->WatchProperty(Settings->CompressionMode, [&](EGSCreateTextureToolCompression) { UpdateOnTexSettingsChange(); });
	Settings->WatchProperty(Settings->SourceFormat, [&](EGSCreateTextureToolSourceFormat) { UpdateOnTexSettingsChange(); });
	UpdateOnResolutionChange();
	UpdateOnTexSettingsChange();

	QuickActionTarget = MakeShared<FGSActionButtonTarget>();
	QuickActionTarget->ExecuteCommand = [this](FString CommandName) { OnQuickActionButtonClicked(CommandName); };
	QuickActionTarget->IsCommandEnabled = [this](FString CommandName) { return OnQuickActionButtonEnabled(CommandName); };
	Settings->Actions.Target = QuickActionTarget;
	Settings->Actions.AddAction(TEXT("CreateTexture"), LOCTEXT("CreateTextureLabel", "Create Texture"),
		LOCTEXT("CreateTextureTooltip", "Create a new Texture with the current Settings"), FColor(0,60,0) );
	Settings->Actions.SetUIPreset_LargeActionButton();

	Settings->ColorPresets.Target = QuickActionTarget;
	Settings->ColorPresets.AddAction(TEXT("Black"), LOCTEXT("BlackLabel", "Black"),
		LOCTEXT("BlackToolTip", "Set Fill Color to Solid Black RGBA=(0,0,0,255)"));
	Settings->ColorPresets.AddAction(TEXT("White"), LOCTEXT("BlackLabel", "White"),
		LOCTEXT("WhiteToolTip", "Set Fill Color to Solid White RGBA=(255,255,255,255)"));
	Settings->ColorPresets.AddAction(TEXT("Zero"), LOCTEXT("ZeroLabel", "Zero"),
		LOCTEXT("ZeroToolTip", "Set Fill Color to RGBA=(0,0,0,0)"));
	Settings->ColorPresets.SetUIPreset_DetailsSize();


	Settings->FormatPresets.Target = QuickActionTarget;
	Settings->FormatPresets.AddAction(TEXT("SRGBColor"), LOCTEXT("SRGBColor_Label", "SRGB Color"),
		LOCTEXT("SRGBColor_Tooltip", "8-bit BGRA Color, SRGB-encoded. Standard color texture."));
	Settings->FormatPresets.AddAction(TEXT("LinearColor"), LOCTEXT("LinearColor_Label", "Linear Color"),
		LOCTEXT("LinearColor_Tooltip", "8-bit BGRA Color, Linear-encoded. Suitable for (eg) packed MRS textures."));
	Settings->FormatPresets.AddAction(TEXT("EmissiveHDR"), LOCTEXT("EmissiveHDR_Label", "EXR"),
		LOCTEXT("EmissievHDR_Tooltip", "16-bit RGBA HDR Color, Linear-encoded. Same settings at 16-bit EXR, use for Emissive maps"));
	Settings->FormatPresets.AddAction(TEXT("Single"), LOCTEXT("SingleChannel_Label", "Single"),
		LOCTEXT("SingleChannel_Tooltip", "Single-channel 8-bit, Linear-encoded"));
	Settings->FormatPresets.SetUIPreset_DetailsSize();

	
	static const auto CVarVirtualTexturesEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTextures")); check(CVarVirtualTexturesEnabled);
	Settings->bEnableVTOption = (CVarVirtualTexturesEnabled->GetValueOnAnyThread() != 0);

	GetToolManager()->DisplayMessage(LOCTEXT("ToolHelp", "Configure texture settings and then click Create Texture in the properties panel to create a new Texture asset"), EToolMessageLevel::UserMessage);
}


void UGSCreateTextureTool::Shutdown(EToolShutdownType ShutdownType)
{
	Settings->SavedDefaultResolutionIndex = Settings->Resolution.SelectedIndex;
	Settings->SaveProperties(this);
	UInteractiveTool::Shutdown(ShutdownType);
}


bool UGSCreateTextureTool::OnQuickActionButtonEnabled(FString CommandName)
{
	return true;
}
void UGSCreateTextureTool::OnQuickActionButtonClicked(FString CommandName)
{
	if (CommandName == TEXT("CreateTexture")) {
		CreateTextureWithActiveSettings();
	}
	else if (CommandName == TEXT("Black")) {
		Settings->FillColor = FColor::Black;
	}
	else if (CommandName == TEXT("White")) {
		Settings->FillColor = FColor::White;
	}
	else if (CommandName == TEXT("Zero")) {
		Settings->FillColor = FColor(0, 0, 0, 0);
	}
	else if (CommandName == TEXT("SRGBColor")) {
		Settings->CompressionMode = EGSCreateTextureToolCompression::Default;
		Settings->SourceFormat = EGSCreateTextureToolSourceFormat::BGRA8;
		Settings->bSRGB = true;
	}
	else if (CommandName == TEXT("LinearColor")) {
		Settings->CompressionMode = EGSCreateTextureToolCompression::Default;
		Settings->SourceFormat = EGSCreateTextureToolSourceFormat::BGRA8;
		Settings->bSRGB = false;
	}
	else if (CommandName == TEXT("EmissiveHDR")) {
		Settings->CompressionMode = EGSCreateTextureToolCompression::HDR_Compressed;
		Settings->SourceFormat = EGSCreateTextureToolSourceFormat::RGBA16F;
		Settings->bSRGB = false;
	}
	else if (CommandName == TEXT("Single")) {
		Settings->CompressionMode = EGSCreateTextureToolCompression::Default;
		Settings->SourceFormat = EGSCreateTextureToolSourceFormat::G8;
		Settings->bSRGB = false;
	}
}



static bool SupportsSRGB(EGSCreateTextureToolCompression CompressionMode)
{
	return CompressionMode == EGSCreateTextureToolCompression::Default
		|| CompressionMode == EGSCreateTextureToolCompression::BC7
		|| CompressionMode == EGSCreateTextureToolCompression::UserInterface
		|| CompressionMode == EGSCreateTextureToolCompression::DistanceFieldFont
		|| CompressionMode == EGSCreateTextureToolCompression::Uncompressed
		|| CompressionMode == EGSCreateTextureToolCompression::DisplacementMap
		|| CompressionMode == EGSCreateTextureToolCompression::Grayscale;
}
static bool SupportsSRGB(EGSCreateTextureToolSourceFormat SourceFormat)
{
	return SourceFormat == EGSCreateTextureToolSourceFormat::G8
		|| SourceFormat == EGSCreateTextureToolSourceFormat::BGRA8;
}


template<typename FormatType>
void FillArrayView(TArrayView64<FormatType> Array, FormatType& FillValue, int NumPixels)
{
	for (int k = 0; k < NumPixels; ++k)
	{
		FormatType& Value = Array[k];
		Value = FillValue;
	}
}

void UGSCreateTextureTool::CreateTextureWithActiveSettings()
{
	bool bCancel = false;
	UTexture2D* NewTexture = nullptr;

	// figure out relative object (todo)
	UObject* StoreRelativeToObject = nullptr;

	// figure out name
	FString UseBaseName = Settings->BaseName;

	int UseWidth = 8, UseHeight = 8;
	if (Settings->bCustomDimensions) {
		UseWidth = Settings->CustomWidth;
		UseHeight = Settings->CustomHeight;
	}
	else {
		FString Label = Settings->Resolution.GetLabelForIndex(Settings->Resolution.SelectedIndex);
		if (FDefaultValueHelper::ParseInt(Label, UseWidth))
			UseHeight = UseWidth;
	}

	bool bSRGB = false;
	if (SupportsSRGB(Settings->CompressionMode) && SupportsSRGB(Settings->SourceFormat))
		bSRGB = Settings->bSRGB;

	bool bEnableVT = false;
	if (Settings->bEnableVTOption && Settings->bVirtualTexturing)
		bEnableVT = true;

	// resolve format types
	const ERawImageFormat::Type OutputRawImageFormat =
		FImageCoreUtils::ConvertToRawImageFormat( (ETextureSourceFormat)(int)Settings->SourceFormat );

	bool bSRGBSource = bSRGB &&
		(OutputRawImageFormat == ERawImageFormat::G8 || OutputRawImageFormat == ERawImageFormat::BGRA8);

	// create FImage to represent source image data
	FImage SourceImage;
	SourceImage.Init(UseWidth, UseHeight, 
		OutputRawImageFormat, (bSRGBSource) ? EGammaSpace::sRGB : EGammaSpace::Linear);
	int NumPixels = UseWidth * UseHeight;

	FColor FillByteColor = Settings->FillColor;
	if (Settings->bOverrideAlpha)
		FillByteColor.A = Settings->OverrideAlpha;
	FLinearColor FillLinearColor = (bSRGBSource) ? FLinearColor(FillByteColor) : FillByteColor.ReinterpretAsLinear();
	FFloat16Color Fill16BitFloat(FillLinearColor);
	uint16 Red16 = (uint16)((int)FillByteColor.R * 256);

	// populate source image with the appropriate color (many options...)
	switch (SourceImage.Format)
	{
		case ERawImageFormat::G8: 
			FillArrayView<uint8>(SourceImage.AsG8(), FillByteColor.R, NumPixels); break;
		case ERawImageFormat::G16:
			ensure(false);					// not sure what to do here...
			//FillArrayView<uint16>(SourceImage.AsG16(), ColorInt16, NumPixels); break;
			// break;
			bCancel = true; break;
		case ERawImageFormat::BGRA8:
			FillArrayView<FColor>(SourceImage.AsBGRA8(), FillByteColor, NumPixels); break;
		case ERawImageFormat::BGRE8:		// what is this??
			FillArrayView<FColor>(SourceImage.AsBGRE8(), FillByteColor, NumPixels); break;
		case ERawImageFormat::RGBA16:
			ensure(false);					// not sure what to do here...
			bCancel = true; break;
		case ERawImageFormat::RGBA16F:
			FillArrayView<FFloat16Color>(SourceImage.AsRGBA16F(), Fill16BitFloat, NumPixels); break;
		case ERawImageFormat::RGBA32F:
			FillArrayView<FLinearColor>(SourceImage.AsRGBA32F(), FillLinearColor, NumPixels); break;
		case ERawImageFormat::R16F:
			FillArrayView<FFloat16>(SourceImage.AsR16F(), Fill16BitFloat.R, NumPixels); break;
		case ERawImageFormat::R32F:
			FillArrayView<float>(SourceImage.AsR32F(), FillLinearColor.R, NumPixels); break;
	}
	if (!ensure(bCancel == false))
		return;

	// resolve some image conversion complication, don't 100% understand... comment for GetPixelFormatForRawImageFormat says to do this?
	ERawImageFormat::Type OutEquivalentFormat = OutputRawImageFormat;
	const EPixelFormat OutputPixelFormat =
		FImageCoreUtils::GetPixelFormatForRawImageFormat(OutputRawImageFormat, &OutEquivalentFormat);
	if (OutEquivalentFormat != OutputRawImageFormat) {
		FImage TempImage;
		SourceImage.CopyTo(TempImage, OutEquivalentFormat, bSRGB ? EGammaSpace::sRGB : EGammaSpace::Linear);
		SourceImage = MoveTemp(TempImage);
	}

	// create the new texture. This populates the Platform data from the SourceImage
	NewTexture = FImageUtils::CreateTexture2DFromImage(SourceImage);
	if (!NewTexture)
		return;

	NewTexture->WaitForPendingInitOrStreaming();
	NewTexture->PreEditChange(nullptr);

	NewTexture->SRGB = bSRGBSource;

	if (bEnableVT)
		NewTexture->VirtualTextureStreaming = 1;

	// now populate the Source data from the Platform Data mip0
	if (GS::InitializeSourceDataFromMip0(NewTexture, UseWidth, UseHeight, OutEquivalentFormat) == false)
		return;

	// set up final texture  settings
	NewTexture->Filter = (TextureFilter)(int)Settings->FilterMode;
	NewTexture->CompressionSettings = (TextureCompressionSettings)(int)Settings->CompressionMode;

	NewTexture->UpdateResource();
	NewTexture->PostEditChange();

	GetToolManager()->BeginUndoTransaction(LOCTEXT("CreateTexture", "Create Texture"));

	// write the temporary NewTexture to a new Asset
	FCreateTextureObjectParams NewTexObjectParams;
	NewTexObjectParams.TargetWorld = TargetWorld.Get();
	NewTexObjectParams.StoreRelativeToObject = StoreRelativeToObject;
	NewTexObjectParams.BaseName = UseBaseName;
	NewTexObjectParams.GeneratedTransientTexture = NewTexture;
	FCreateTextureObjectResult CreateTexResult = UE::Modeling::CreateTextureObject(GetToolManager(), MoveTemp(NewTexObjectParams));
	if (CreateTexResult.IsOK() == false)
		GetToolManager()->DisplayMessage(LOCTEXT("CreateError", "Unknown Error creating new Texture!"), EToolMessageLevel::UserError);

	GetToolManager()->EndUndoTransaction();
}



void UGSCreateTextureTool::UpdateOnResolutionChange()
{
	FString NewMessage = "";
	bool bShowWarning = false;

	if (Settings->bCustomDimensions) {
		if (GS::IsPowerOfTwo(Settings->CustomWidth) == false || GS::IsPowerOfTwo(Settings->CustomHeight) == false) {
			bShowWarning = true;
			NewMessage = TEXT("The Custom Width/Height is not a Power of Two. This may cause performance/stability issues!");
		}
		else if (Settings->CustomWidth != Settings->CustomHeight) {
			bShowWarning = true;
			NewMessage = TEXT("Non-square textures may cause performance issues");
		}
	}
	
	Settings->ResolutionWarning = NewMessage;
	Settings->bShowResolutionWarning = bShowWarning;
}

void UGSCreateTextureTool::UpdateOnTexSettingsChange()
{
	Settings->bFormatSupportsSRGB = SupportsSRGB(Settings->CompressionMode) && SupportsSRGB(Settings->SourceFormat);
}


#undef LOCTEXT_NAMESPACE