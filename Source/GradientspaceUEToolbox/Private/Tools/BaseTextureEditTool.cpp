// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/BaseTextureEditTool.h"
#include "InteractiveToolManager.h"
#include "PreviewMesh.h"
#include "ModelingToolTargetUtil.h"
#include "ToolSetupUtil.h"
#include "DynamicMesh/MeshTransforms.h"
#include "Misc/EngineVersionComparison.h"

#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "TargetInterfaces/DynamicMeshProvider.h"
#include "TargetInterfaces/MaterialProvider.h"

#include "Engine/World.h"

#include "MaterialShared.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Rendering/Texture2DResource.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "RHICommandList.h"
#include "TextureCompiler.h"

#include "Image/ImageBuilder.h"
#include "Utility/GSTextureUtil.h"
#include "Utility/GSMaterialGraphUtil.h"
#include "Utility/GSUEMaterialUtil.h"



using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGSBaseTextureEditTool"





bool FToolMaterialTextureSet::FindTexture(const UMaterialInterface* Material, const UTexture2D* Texture, int& MaterialIndex, int& TextureIndex) const
{
	for (int k = 0; k < MaterialTextures.Num(); ++k) {
		if (MaterialTextures[k].Material != Material)
			continue;
		for (int j = 0; j < MaterialTextures[k].Textures.Num(); j++) {
			if (MaterialTextures[k].Textures[j].Texture == Texture) {
				MaterialIndex = k;
				TextureIndex = j;
				return true;
			}
		}
	}
	return false;
}


bool FToolMaterialTextureSet::FindTextureByIdentifier(int UniqueID, int& MaterialIndex, int& TextureIndex) const
{
	for (int k = 0; k < MaterialTextures.Num(); ++k) {
		for (int j = 0; j < MaterialTextures[k].Textures.Num(); j++) {
			if (MaterialTextures[k].Textures[j].UniqueID == UniqueID) {
				MaterialIndex = k;
				TextureIndex = j;
				return true;
			}
		}
	}
	return false;
}


int UTextureSelectionSetSettings::FindTextureUniqueID(const UMaterialInterface* Material, const UTexture2D* Texture) const
{
	int MatIdx = -1, TexIdx = -1;
	if (TextureSet.FindTexture(Material, Texture, MatIdx, TexIdx))
		return TextureSet.MaterialTextures[MatIdx].Textures[TexIdx].UniqueID;
	return -1;
}


static void InitializeTextureSet(FToolMaterialTextureSet& TextureSet, const TArray<UMaterialInterface*>& MaterialSet)
{
	// TODO need to hande overridden textures in MI's here...
	int UniqueIDCounter = 0;
	for (int k = 0; k < MaterialSet.Num(); ++k) {
		UMaterialInterface* IMaterial = MaterialSet[k];
		if (!IMaterial)
			continue;

		FMaterialTextureList TexList;
		TexList.Material = IMaterial;
		TexList.SourceMaterialSetIndex = k;


		TArray<GS::MaterialTextureInfo> FoundTextures;
		constexpr bool bIncludeBaseTexturesInMIs = true;
		GS::FindActiveTextureSetForMaterial(IMaterial, FoundTextures, bIncludeBaseTexturesInMIs);
		for (const GS::MaterialTextureInfo& TexInfo : FoundTextures)
		{
			FMaterialTextureListTex Tex;
			Tex.Texture = const_cast<UTexture2D*>(TexInfo.Texture);
			Tex.UniqueID = UniqueIDCounter++;

			if (TexInfo.bIsOverrideParameter) {
				Tex.bIsParameter = true;
				Tex.ParameterName = TexInfo.ParameterName;
			}

			TexList.Textures.Add(Tex);
		}

		if (TexList.Textures.Num() > 0)
			TextureSet.MaterialTextures.Add(TexList);
	}
}




class FPixelPaint_TextureChange : public FToolCommandChange
{
public:
	TWeakObjectPtr<UMaterialInterface> MatBefore;
	TWeakObjectPtr<UTexture2D> TexBefore;
	TWeakObjectPtr<UMaterialInterface> MatAfter;
	TWeakObjectPtr<UTexture2D> TexAfter;

	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;
	virtual FString ToString() const override { return TEXT("FPixelPaint_TextureChange"); }
};






void UGSBaseTextureEditTool::SetTargetWorld(UWorld* World)
{
	TargetWorld = World;
}

void UGSBaseTextureEditTool::Setup()
{
	UMeshSurfacePointTool::Setup();


	UToolTarget* UseTarget = GetTarget();

	if (HideTargetObjectOnStartup())
		UE::ToolTarget::HideSourceObject(UseTarget);

#if UE_VERSION_OLDER_THAN(5,5,0)
	FDynamicMesh3 InputMeshWithTangents = UE::ToolTarget::GetDynamicMeshCopy(UseTarget, true);
#else
	FGetMeshParameters Params;
	Params.bWantMeshTangents = true;
	FDynamicMesh3 InputMeshWithTangents = UE::ToolTarget::GetDynamicMeshCopy(UseTarget, Params);
#endif

	FTransformSRT3d InitialTargetTransform = UE::ToolTarget::GetLocalToWorldTransform(UseTarget);
#if 1
	// bake scale and rotation into mesh. This resolves various problems around local/world space
	// but it will cause problems for objects that use local-space coordinates in the material!
	// need to investigate...maybe needs to be a toggle in the tool?
	InitialTargetTransform.ClampMinimumScale(0.01);
	TargetTransform = FTransformSRT3d(InitialTargetTransform.GetTranslation());		// translation-only
	InitialTargetTransform.SetTranslation(FVector3d::Zero());
	MeshTransforms::ApplyTransform(InputMeshWithTangents, InitialTargetTransform, true);
	TargetScale = FVector3d::One();
#else
	TargetTransform = InitialTargetTransform;
	TargetScale = TargetTransform.GetScale3D();
#endif

	PreviewMesh = NewObject<UPreviewMesh>(this);
	PreviewMesh->CreateInWorld(TargetWorld.Get(), TargetTransform);
	PreviewMesh->bBuildSpatialDataStructure = true;
	PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::ExternallyProvided);
	PreviewMesh->ReplaceMesh(MoveTemp(InputMeshWithTangents));

	FComponentMaterialSet MaterialSet;
	Cast<IMaterialProvider>(UseTarget)->GetMaterialSet(MaterialSet);
	InitialMaterials = MaterialSet.Materials;
	PreviewMesh->SetMaterials(InitialMaterials);

	TextureVizMaterial = ToolSetupUtil::GetDefaultErrorMaterial(GetToolManager());
	if (UMaterial* LoadedMaterial = LoadObject<UMaterial>(nullptr, TEXT("/GradientspaceUEToolbox/Materials/M_GSTexVizMaterial"))) {
		TextureVizMaterial = UMaterialInstanceDynamic::Create(LoadedMaterial, this);
	}

	TextureSetSettings = NewObject<UTextureSelectionSetSettings>(this);
	TextureSetSettings->RestoreProperties(this, TEXT("BaseTextureEditTool"));
	TextureSetSettings->ActiveMaterial = nullptr;
	TextureSetSettings->ActiveTexture = nullptr;
	TextureSetSettings->OnUpdateActiveTextureInTool = [this](int UniqueID) {
		UpdateActiveTargetTextureFromExternal(UniqueID);
	};
	AddToolPropertySource(TextureSetSettings);

	FToolMaterialTextureSet& TextureSet = TextureSetSettings->TextureSet;
	InitializeTextureSet(TextureSet, InitialMaterials);
	if (TextureSet.MaterialTextures.Num() > 0)
	{
		bool bFound = false;
		if (TextureSetSettings->LastActiveTexture != nullptr) {
			int MatIndex, TexIndex;
			if (TextureSet.FindTexture(TextureSetSettings->LastActiveMaterial, TextureSetSettings->LastActiveTexture, MatIndex, TexIndex))
			{
				UpdateActiveTargetTexture(TextureSet.MaterialTextures[MatIndex].Material,
					TextureSet.MaterialTextures[MatIndex].Textures[TexIndex].Texture, true);
				bFound = true;
			}
		}
		if (!bFound) {
			UpdateActiveTargetTexture(TextureSet.MaterialTextures[0].Material,
				TextureSet.MaterialTextures[0].Textures[0].Texture, true);
		}
	}
	//else
		//GetToolManager()->DisplayMessage(LOCTEXT("NoTexturesMsg", "No Paintable Textures could be found in the available Materials."), EToolMessageLevel::UserError);

	TextureSetSettings->WatchProperty(TextureSetSettings->FilterMode, [&](EGSTextureFilterMode) { UpdateSamplingMode(); });
	UpdateSamplingMode();

	TSharedPtr<FGSEnumButtonsTarget> ViewModeTarget = TextureSetSettings->ViewMode.SimpleInitialize();
	TextureSetSettings->ViewMode.AddItem({ TEXT("ViewMode_Mat"), 0, LOCTEXT("ViewMode_Mat", "Material"),
		LOCTEXT("ViewMode_Mat_Tooltip", "Show full Material for selected Texture") });
	TextureSetSettings->ViewMode.AddItem({ TEXT("ViewMode_Tex"), 1, LOCTEXT("ViewMode_Tex", "Texture"),
		LOCTEXT("ViewMode_Tex_Tooltip", "Isolate active paintable Texture") });
	TextureSetSettings->ViewMode.AddItem({ TEXT("ViewMode_Mask"), 2, LOCTEXT("ViewMode_Mask", "Masked"),
		LOCTEXT("ViewMode_Mask_Tooltip", "Isolate paintable texture and apply active Channel mask") });
	TextureSetSettings->ViewMode.ValidateAfterSetup();
	TextureSetSettings->ViewMode.bShowLabel = true;
	TextureSetSettings->ViewMode.bCentered = false;
	TextureSetSettings->ViewMode.bCalculateFixedButtonWidth = true;
	TextureSetSettings->ViewMode.SetUIPreset_DetailsSize();

	TextureSetSettings->WatchProperty(TextureSetSettings->ViewMode.SelectedIndex, [&](int) { UpdateCurrentVisibleMaterials(); });
}

void UGSBaseTextureEditTool::UpdateCurrentVisibleMaterials()
{
	TArray<UMaterialInterface*> SetMaterials = InitialMaterials;
	SetMaterials[ActivePaintMaterialID] = ActivePaintMaterial;
	if (TextureSetSettings->ViewMode.SelectedIndex > 0)
		SetMaterials[ActivePaintMaterialID] = TextureVizMaterial;
	PreviewMesh->SetMaterials(SetMaterials);
}


void UGSBaseTextureEditTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	FIndex4i flags = (TextureSetSettings->ViewMode.SelectedIndex == 2) ? GetActiveChannelViewMask() : FIndex4i(1, 1, 1, 1);
	if (flags.A == 0 && flags.B == 0 && flags.C == 0) 
	{
		TextureVizMaterial->SetVectorParameterValue("ChannelMask", FVector(0, 0, 0));
		TextureVizMaterial->SetScalarParameterValue("AlphaToWhite", 1.0f);
	}
	else 
	{
		FVector RGBMask(flags.A ? 1.0 : 0.0, flags.B ? 1.0 : 0.0, flags.C ? 1.0 : 0.0);
		TextureVizMaterial->SetVectorParameterValue("ChannelMask", RGBMask);
		TextureVizMaterial->SetScalarParameterValue("AlphaToWhite", 0.0f);
	}

	this->CameraState = RenderAPI->GetCameraState();
	this->LastViewInfo.Initialize(RenderAPI);
}


void UGSBaseTextureEditTool::Shutdown(EToolShutdownType ShutdownType)
{
	UMeshSurfacePointTool::Shutdown(ShutdownType);

	PreviewMesh->Disconnect();
	PreviewMesh = nullptr;

	if (TextureSetSettings->ActiveMaterial)
		TextureSetSettings->LastActiveMaterial = TextureSetSettings->ActiveMaterial;
	if (TextureSetSettings->ActiveTexture)
		TextureSetSettings->LastActiveTexture = TextureSetSettings->ActiveTexture;
	TextureSetSettings->SaveProperties(this, TEXT("BaseTextureEditTool"));

	if (HideTargetObjectOnStartup())
		Cast<IPrimitiveComponentBackedTarget>(Target)->SetOwnerVisibility(true);

	if (!SourcePaintTexture) return;
	if (ShutdownType != EToolShutdownType::Cancel)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("OnAccept", "Update Texture"));
		SourcePaintTexture->Modify();
		GS::UpdateTextureSourceFromBuffer(TextureColorBuffer4b, SourcePaintTexture, true);
		GetToolManager()->EndUndoTransaction();
	}
}



void UGSBaseTextureEditTool::UpdateActiveTargetTextureFromExternal(int UniqueTextureID)
{
	int MatIdx, TexIdx;
	if (!TextureSetSettings->TextureSet.FindTextureByIdentifier(UniqueTextureID, MatIdx, TexIdx))
		return;
	const FMaterialTextureList& MatInfo = TextureSetSettings->TextureSet.MaterialTextures[MatIdx];
	const FMaterialTextureListTex& TexInfo = MatInfo.Textures[TexIdx];
	// already active, skip this update
	if (TextureSetSettings->ActiveMaterial == MatInfo.Material &&
		TextureSetSettings->ActiveTexture == TexInfo.Texture)
		return;

	UpdateActiveTargetTexture(MatInfo.Material, TexInfo.Texture, false);
}

void UGSBaseTextureEditTool::UpdateActiveTargetTexture(UMaterialInterface* SourceMaterial, const UTexture2D* SourceTexture, bool bBroadcastChange)
{
	int MatIdx, TexIdx;
	if (!TextureSetSettings->TextureSet.FindTexture(SourceMaterial, SourceTexture, MatIdx, TexIdx))
		return;

	const FMaterialTextureList& MatInfo = TextureSetSettings->TextureSet.MaterialTextures[MatIdx];
	const FMaterialTextureListTex& TexInfo = MatInfo.Textures[TexIdx];

	// already active, skip this update
	if (TextureSetSettings->ActiveMaterial == MatInfo.Material &&
		TextureSetSettings->ActiveTexture == TexInfo.Texture)
		return;

	PreActiveTargetTextureUpdate(false);

	bool bTransact = (SourcePaintTexture != nullptr) && (!GIsTransacting);
	if (bTransact && CurrentTextureStrokeCount > 0)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("CommitTextureOnChange", "Commit Texture"));

		PreActiveTargetTextureUpdate(true);

		SourcePaintTexture->Modify();
		GS::UpdateTextureSourceFromBuffer(TextureColorBuffer4b, SourcePaintTexture, /*bWaitForCompile=*/true);

		TUniquePtr<FPixelPaint_TextureChange> TexChange = MakeUnique<FPixelPaint_TextureChange>();
		TexChange->MatBefore = TextureSetSettings->ActiveMaterial;
		TexChange->TexBefore = TextureSetSettings->ActiveTexture;
		TexChange->MatAfter = MatInfo.Material;
		TexChange->TexAfter = TexInfo.Texture;
		GetToolManager()->EmitObjectChange(this, MoveTemp(TexChange), LOCTEXT("ChangeTexture", "Change Texture"));

		GetToolManager()->EndUndoTransaction();

		// let material respond to new source texture if necessary
		GS::DoInternalMaterialUpdates(TextureSetSettings->ActiveMaterial);
	}
	CurrentTextureStrokeCount = 0;

	InitializeActivePaintTextureMaterial(MatInfo.Material, TexInfo);
	TextureSetSettings->ActiveMaterial = MatInfo.Material;
	TextureSetSettings->ActiveTexture = TexInfo.Texture;
	this->SourcePaintTexture = TexInfo.Texture;
	if (bBroadcastChange)
		TextureSetSettings->OnActiveTextureModifiedByTool.Broadcast(TextureSetSettings);

	this->ActivePaintMaterialID = MatInfo.SourceMaterialSetIndex;

	TextureVizMaterial->SetTextureParameterValue("ColorTexture", this->ActivePaintTexture);

	UpdateCurrentVisibleMaterials();

	FString Resolution = FString::Printf(TEXT("%dx%d %s"), PaintTextureWidth, PaintTextureHeight,
		(TextureSetSettings->ActiveTexture->SRGB) ? TEXT("sRGB") : TEXT("Linear"));
	switch (TextureSetSettings->ActiveTexture->Source.GetFormat())
	{
	case TSF_G8:		TextureSetSettings->SourceFormat = FString::Printf(TEXT("G8 %s"), *Resolution); break;
	case TSF_BGRA8:		TextureSetSettings->SourceFormat = FString::Printf(TEXT("BGRA8 %s"), *Resolution); break;
	case TSF_BGRE8:		TextureSetSettings->SourceFormat = FString::Printf(TEXT("BGRE8 %s"), *Resolution); break;
	case TSF_RGBA16:	TextureSetSettings->SourceFormat = FString::Printf(TEXT("RGBA16 %s"), *Resolution); break;
	case TSF_RGBA16F:	TextureSetSettings->SourceFormat = FString::Printf(TEXT("RGBA16F %s"), *Resolution); break;
	case TSF_G16:		TextureSetSettings->SourceFormat = FString::Printf(TEXT("G16 %s"), *Resolution); break;
	case TSF_RGBA32F:	TextureSetSettings->SourceFormat = FString::Printf(TEXT("RGBA32F %s"), *Resolution); break;
	case TSF_R16F:		TextureSetSettings->SourceFormat = FString::Printf(TEXT("R16F %s"), *Resolution); break;
	case TSF_R32F:		TextureSetSettings->SourceFormat = FString::Printf(TEXT("R32F %s"), *Resolution); break;
	}
	switch (TextureSetSettings->ActiveTexture->CompressionSettings)
	{
	case TC_Default: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("Default (DXT1/5, BC1/3 on DX11+)")); break;
	case TC_Normalmap: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("Normalmap (DXT5, BC5 on DX11+)")); break;
	case TC_Masks: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("Masks (no sRGB)")); break;
	case TC_Grayscale: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("Grayscale (G8/16, RGB8 sRGB)")); break;
	case TC_Displacementmap: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("Displacementmap (G8/16)")); break;
	case TC_VectorDisplacementmap: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("VectorDisplacementmap (RGBA8)")); break;
	case TC_HDR: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("HDR (RGBA16F, no sRGB)")); break;
	case TC_EditorIcon: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("UserInterface2D (RGBA)")); break;
	case TC_Alpha: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("Alpha (no sRGB, BC4 on DX11+)")); break;
	case TC_DistanceFieldFont: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("DistanceFieldFont (G8)")); break;
	case TC_HDR_Compressed: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("HDR Compressed (RGB, BC6H, DX11)")); break;
	case TC_BC7: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("BC7 (DX11, optional A)")); break;
	case TC_HalfFloat: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("Half Float (R16F)")); break;
	case TC_SingleFloat: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("Single Float (R32F)")); break;
	case TC_HDR_F32: TextureSetSettings->CompressedFormat = FString::Printf(TEXT("HDR High Precision (RGBA32F)")); break;
	}

	// allow subclass to respond to new texture
	OnActiveTargetTextureUpdated();
}




void UGSBaseTextureEditTool::InitializeActivePaintTextureMaterial(UMaterialInterface* SourceMaterial, FMaterialTextureListTex SourceTextureInfo)
{
	// TODO:
	// To support this properly, we need to duplicate both the base material and the MI. Seems like to do that we will need to be able 
	//    to copy parameters from one MI to another...
	UMaterialInterface* UseSourceMaterial = SourceMaterial;
	if (SourceTextureInfo.bIsParameter == false && Cast<UMaterialInstance>(SourceMaterial) != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TextureEditTool] When a Material Instance is assigned to target Object, a live preview can only be shown for Textures that are exposed as Parameters. Base Material preview must be shown for non-Parameter Textures."));
		UseSourceMaterial = SourceMaterial->GetBaseMaterial();
	}

	// create duplicate of existing texture
	// NOTE: this copies the source data too...do we need to do that in this context?
	//  (we do to show compression preview...)
	UTexture2D* SourceTexture = SourceTextureInfo.Texture;
	ActivePaintTexture = DuplicateObject<UTexture2D>(SourceTexture, this);

	// Doing this DuplicateObject is going to kick off a DDC build for this new texture.
	// We have to wait for that to finish before we start modifying settings, because
	// if it is a VT texture we hit some kind of race where in the PostEditChange()/UpdateResource()
	// below, even though we have disabled VT, we end up w/ VT cooked data...
	FTextureCompilingManager::Get().FinishCompilation({ ActivePaintTexture });

	ETextureSourceFormat SourceFormat = SourceTexture->Source.GetFormat();

	// reconfigure duplicate Texture to be maximally live-updatable during painting (and set desired Filter mode)
	//    - Uncompressed RGBA8
	//    - No Mips
	//	  - Disable Virtual Texturing
	//ActivePaintTexture->Filter = TF_Nearest;
	ActivePaintTexture->Filter = SourceTexture->Filter;
	TextureSetSettings->FilterMode = (EGSTextureFilterMode)(int)SourceTexture->Filter;
	TextureSetSettings->SilentUpdateWatched();
	ActivePaintTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	ActivePaintTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	if (ActivePaintTexture->VirtualTextureStreaming != 0)
	{
		ActivePaintTexture->VirtualTextureStreaming = 0;
		// note: found this distressing comment in TextureDerivedDataTask.cpp...
		//    "// Texture.VirtualTextureStreaming is more a hint that might be overruled by the buildsettings"
	}

	ActivePaintTexture->PostEditChange();		// this will call UpdateResource()
	//ActivePaintTexture->UpdateResource();
	FTextureCompilingManager::Get().FinishCompilation({ ActivePaintTexture });

	FTexturePlatformData* PlatformData = ActivePaintTexture->GetPlatformData();
	if (PlatformData->VTData != nullptr)
	{
		// for some reason disabling VT didn't work...
		UE_LOG(LogTemp, Warning, TEXT("UGSBaseTextureEditTool::InitializeActivePaintTextureMaterial - VT is still enabled on Texture, cannot paint!"));
		this->PaintTextureWidth = 1;
		this->PaintTextureHeight = 1;
		TextureColorBuffer4b.SetNum(1);
		TextureColorBuffer4b[0] = FColor::White;
		bActivePaintTextureValid = false;
		GetToolManager()->DisplayMessage(LOCTEXT("VTError", "Virtual Texturing is interfering with painting this Texture. Try disabling 'Virtual Texture Streaming' in the Texture Settings."), EToolMessageLevel::UserError);
	}
	else
	{
		// make a local copy of uncompressed texture image
		const FColor* FormattedImageData = reinterpret_cast<const FColor*>(PlatformData->Mips[0].BulkData.LockReadOnly());

		EPixelFormat UsingPixelFormat = PlatformData->GetLayerPixelFormat(0);
		const int32 Width = PlatformData->Mips[0].SizeX;
		const int32 Height = PlatformData->Mips[0].SizeY;

		this->PaintTextureWidth = Width;
		this->PaintTextureHeight = Height;
		this->bIsSRGB = ActivePaintTexture->SRGB;

		const int64 Num = (int64)Width * (int64)Height;
		TextureColorBuffer4b.SetNum(Num);
		for (int64 i = 0; i < Num; ++i) {
			TextureColorBuffer4b[i] = FormattedImageData[i];
		}

		PlatformData->Mips[0].BulkData.Unlock();

		bActivePaintTextureValid = true;
		GetToolManager()->DisplayMessage(FText::GetEmpty(), EToolMessageLevel::UserError);
	}


	// Now duplicate the Material and assign the new Texture as necessary in the duplicate.
	// This is very complicated and not all cases are handled...specifically
	//   - currently if the Texture is used in multiple separate nodes in the Material, it may not be replaced in all of them
	//   - no handling of more complex situations like VT, Nanite, etc


	// make duplicate material or material-instance
	UObject* DuplicatedMaterial = DuplicateObject<UObject>(UseSourceMaterial, this);
	ActivePaintMaterial = Cast<UMaterialInterface>(DuplicatedMaterial);

	// only one of these should be null...
	UMaterial* NewAsMaterial = Cast<UMaterial>(ActivePaintMaterial);
	UMaterialInstanceConstant* NewAsMIC = Cast<UMaterialInstanceConstant>(ActivePaintMaterial);

	if (NewAsMIC != nullptr && SourceTextureInfo.bIsParameter)
	{
		// easy case - texture is a parameter in (duplicate) MIC, just assign it
		// (does this work if parameter is for a VT sampler?)
		NewAsMIC->SetTextureParameterValueEditorOnly(SourceTextureInfo.ParameterName, ActivePaintTexture);
	}
	else if (NewAsMaterial != nullptr && SourceTextureInfo.bIsParameter)
	{
		// easy case - texture is a parameter in (duplicate) Material, just assign it
		// (does this work if parameter is for a VT sampler?)
		NewAsMaterial->SetTextureParameterValueEditorOnly(SourceTextureInfo.ParameterName, ActivePaintTexture);
	}
	else if ( NewAsMaterial != nullptr )
	{
		UMaterialGraph* MaterialGraph = GS::GetMaterialGraphForMaterial(NewAsMaterial);
		ensure(MaterialGraph != nullptr);

		//FoundExpression->GetTexturesForceMaterialRecompile  (look into this function of MaterialExpression...)
		// also maybe look at UMaterialExpression::GetReferencedTexture()

		UMaterialGraphNode* FoundNode = nullptr;
		UMaterialExpressionTextureBase* FoundExpression = nullptr;
		// todo: this currently only returns a single node, it should return multiple...
		if (GS::FindTextureNodeInMaterial(MaterialGraph, SourceTexture, FoundNode, FoundExpression))
		{
			FoundExpression->Modify();
			FoundExpression->Texture = ActivePaintTexture;
			FoundExpression->AutoSetSampleType();		// seems like this automatically handles linear vs srgb, VT, etc

			ActivePaintTexture->PostEditChange();		// why PostEditChange here?
			FTextureCompilingManager::Get().FinishCompilation({ ActivePaintTexture });
		}
	}

	GS::DoInternalMaterialUpdates(ActivePaintMaterial);


	NestedCancelCount = 0;
}


int UGSBaseTextureEditTool::GetActivePaintMaterialID() const
{
	return (InitialMaterials.Num() > 1) ? this->ActivePaintMaterialID : -1;	// don't need this if only one material
}


void UGSBaseTextureEditTool::PushFullImageUpdate()
{
	if (IsActivePaintTextureValid() == false)
		return;
	FTextureResource* CheckResource = ActivePaintTexture->GetResource();
	if (!CheckResource)
		return;

	UTexture2D* LambdaTexture = this->ActivePaintTexture;
	int LambdaWidth = this->PaintTextureWidth, LambdaHeight = this->PaintTextureHeight;
	const FColor* LambdaBuffer = &TextureColorBuffer4b[0];
	ENQUEUE_RENDER_COMMAND(UpdateTextureRegionsData)(
		[LambdaTexture, LambdaWidth, LambdaHeight, LambdaBuffer](FRHICommandListImmediate& RHICmdList)
	{
		FTexture2DResource* Resource = (FTexture2DResource*)LambdaTexture->GetResource();

		// full copy works...
		FUpdateTextureRegion2D UpdateRegion(0, 0, 0, 0, LambdaWidth, LambdaHeight);
		RHIUpdateTexture2D(
			Resource->GetTexture2DRHI(),
			0,
			UpdateRegion,
			LambdaWidth * 4,		// "pitch" is length of row which we store w/ no padding
			(const uint8*)LambdaBuffer
		);
	});

	NestedCancelCount = 0;
}

void UGSBaseTextureEditTool::PushRegionImageUpdate(int PixelX, int PixelY, int Width, int Height)
{
	if (IsActivePaintTextureValid() == false)
		return;
	FTextureResource* CheckResource = ActivePaintTexture->GetResource();
	if (!CheckResource)
		return;

	UTexture2D* LambdaTexture = this->ActivePaintTexture;
	int LambdaWidth = this->PaintTextureWidth, LambdaHeight = this->PaintTextureHeight;
	const FColor* LambdaBuffer = &TextureColorBuffer4b[0];
	ENQUEUE_RENDER_COMMAND(UpdateTextureRegionsData)(
		[LambdaTexture, LambdaWidth, PixelX, PixelY, Width, Height, LambdaBuffer](FRHICommandListImmediate& RHICmdList)
	{
		FTexture2DResource* Resource = (FTexture2DResource*)LambdaTexture->GetResource();

		// SrcX and SrcY are apparently ignored by the D3D12 RHI CopyTexture implementations.
		// So instead of passing the pointer to the start-of-buffer and the source-rect-origin,
		// instead you pass the pointer to the source-rect-origin, and a stride ("pitch") that
		// will increment in the buffer to the next row. wtf.
		const uint8* TmpBuffer = (const uint8*)&LambdaBuffer[PixelY * LambdaWidth + PixelX];
		FUpdateTextureRegion2D UpdateRegion;
		UpdateRegion.DestX = PixelX;
		UpdateRegion.DestY = PixelY;
		UpdateRegion.SrcX = 0;
		UpdateRegion.SrcY = 0;
		UpdateRegion.Width = Width;
		UpdateRegion.Height = Height;
		RHIUpdateTexture2D(
			Resource->GetTexture2DRHI(),
			0,
			UpdateRegion,
			LambdaWidth * 4 * sizeof(uint8),		// "pitch" is length of row which we store w/ no padding
			TmpBuffer
		);
	});
}




void UGSBaseTextureEditTool::SetNearestFilteringOverride(bool bEnable)
{
	if (bEnable)
		TextureSetSettings->FilterMode = EGSTextureFilterMode::Nearest;
	else if (ActivePaintTexture != nullptr)
		TextureSetSettings->FilterMode = (EGSTextureFilterMode)(int)SourcePaintTexture->Filter;
	NotifyOfPropertyChangeByTool(TextureSetSettings);
}

void UGSBaseTextureEditTool::UpdateSamplingMode()
{
	if (!ActivePaintTexture) return;

	int SetFilterMode = (int)TextureSetSettings->FilterMode;
	if ((int)ActivePaintTexture->Filter != SetFilterMode)
	{
		ActivePaintTexture->Filter = (TextureFilter)SetFilterMode;
		ActivePaintTexture->PostEditChange();		// will call UpdateResource if it's necessary
		//ActivePaintTexture->UpdateResource();
		FTextureCompilingManager::Get().FinishCompilation({ ActivePaintTexture });

		PushFullImageUpdate();
		GS::DoInternalMaterialUpdates(ActivePaintMaterial);
	}
}



void UGSBaseTextureEditTool::UndoRedoTextureChange(const FPixelPaint_TextureChange& Change, bool bUndo)
{
	EnsureCompressionPreviewDisabled();
	PreTextureChangeOnUndoRedo();

	if (bUndo)
		UpdateActiveTargetTexture(Change.MatBefore.Get(), Change.TexBefore.Get(), true);
	else
		UpdateActiveTargetTexture(Change.MatAfter.Get(), Change.TexAfter.Get(), true);

	NestedCancelCount = 0;
}



void FPixelPaint_TextureChange::Apply(UObject* Object)
{
	if (UGSBaseTextureEditTool* Tool = Cast<UGSBaseTextureEditTool>(Object))
		Tool->UndoRedoTextureChange(*this, false);
}
void FPixelPaint_TextureChange::Revert(UObject* Object)
{
	if (UGSBaseTextureEditTool* Tool = Cast<UGSBaseTextureEditTool>(Object))
		Tool->UndoRedoTextureChange(*this, true);
}



FColor UGSBaseTextureEditTool::ConvertToFColor(const FLinearColor& LinearColor) const
{
	return LinearColor.ToFColor(this->bIsSRGB);
}

FLinearColor UGSBaseTextureEditTool::ConvertToLinear(const FColor& Color) const
{
	return (this->bIsSRGB) ?
		FLinearColor::FromSRGBColor(Color) :
		Color.ReinterpretAsLinear();
}


bool UGSBaseTextureEditTool::ComputeHitPixel(const FRay& WorldRay, const FHitResult& HitResult,
	FVector3d& LocalPoint, FVector3d& LocalNormal,
	int& HitTriangleID,
	FVector2f& HitUVPos, UE::Geometry::FVector2i& HitPixelCoords) const
{
	LocalPoint = FVector3d::Zero();
	LocalNormal = FVector3d::UnitZ();
	HitPixelCoords = FVector2i(-1, -1);
	HitUVPos = FVector2f(-1, -1);
	HitTriangleID = -1;

	bool bOK = false;
	PreviewMesh->ProcessMesh([&](const FDynamicMesh3& Mesh) {
		HitTriangleID = HitResult.FaceIndex;
		if (Mesh.IsTriangle(HitTriangleID) == false)
			return;
		auto UVOverlay = Mesh.Attributes()->PrimaryUV();
		if (UVOverlay->IsSetTriangle(HitTriangleID) == false)
			return;

		LocalPoint = TargetTransform.InverseTransformPosition(HitResult.ImpactPoint);
		LocalNormal = Mesh.GetTriNormal(HitTriangleID);

		FTriangle3d Tri;
		Mesh.GetTriVertices(HitTriangleID, Tri.V[0], Tri.V[1], Tri.V[2]);
		FVector3d BaryCoords = Tri.GetBarycentricCoords(LocalPoint);

		FVector2f U, V, W;
		UVOverlay->GetTriElements(HitTriangleID, U, V, W);

		HitUVPos = (float)BaryCoords.X * U + (float)BaryCoords.Y * V + (float)BaryCoords.Z * W;

		HitPixelCoords.X = (int)(HitUVPos.X * (float)PaintTextureWidth);
		HitPixelCoords.Y = (int)(HitUVPos.Y * (float)PaintTextureHeight);
		if (HitPixelCoords.X >= 0 && HitPixelCoords.X < PaintTextureWidth &&
			HitPixelCoords.Y >= 0 && HitPixelCoords.Y < PaintTextureHeight)
			bOK = true;
	});
	return bOK;
}




bool UGSBaseTextureEditTool::CanCurrentlyNestedCancel()
{
	return (NestedCancelCount < 3);
}

bool UGSBaseTextureEditTool::ExecuteNestedCancelCommand()
{
	if ( NestedCancelCount == 0 )
		GetToolManager()->DisplayMessage(LOCTEXT("CancelMsg", "To avoid accidental Cancellation of your edit session, this tool requires you to hit Escape 3 times in a row to Exit."), EToolMessageLevel::UserMessage);
	NestedCancelCount++;
	return NestedCancelCount < 3;
}




void UGSBaseTextureEditTool::EnableCompressionPreviewPanel()
{
	if (CompressionPreviewSettings == nullptr) {
		CompressionPreviewSettings = NewObject<UCompressionPreviewSettings>(this);
		AddToolPropertySource(CompressionPreviewSettings);

		TSharedPtr<FGSActionButtonTarget> ActionsTarget = MakeShared<FGSActionButtonTarget>();
		ActionsTarget->ExecuteCommand = [this](FString CommandName) { 
			ToggleCompressionPreview(CommandName.StartsWith(TEXT("Enable")) ? true : false); };
		ActionsTarget->IsCommandVisible = [this](FString CommandName) {
			return CommandName.StartsWith(TEXT("Enable")) ? (!bCompressionPreviewEnabled) : bCompressionPreviewEnabled; };
		CompressionPreviewSettings->Actions.Target = ActionsTarget;
		CompressionPreviewSettings->Actions.AddAction("EnablePreview",
			LOCTEXT("EnablePreview", "Enable Compression Preview"),
			LOCTEXT("EnablePreviewTooltip", "Enable a preview of Texture Compresion. Painting is disabled while Compression Preview is active."));
		CompressionPreviewSettings->Actions.AddAction("DisablePreview",
			LOCTEXT("DisablePreview", "Disable Compression Preview"),
			LOCTEXT("DisablePreviewTooltip", "Disable Texture Compression preview"), FColor(175,0,0) );
	}
}


void UGSBaseTextureEditTool::ToggleCompressionPreview(bool bSetToEnabled)
{
	if (bSetToEnabled == bCompressionPreviewEnabled) return;
	if (!ActivePaintTexture) return;

	if (bSetToEnabled)
	{
		ActivePaintTexture->CompressionSettings = SourcePaintTexture->CompressionSettings;
		ActivePaintTexture->MipGenSettings = SourcePaintTexture->MipGenSettings;
	}
	else
	{
		ActivePaintTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
		ActivePaintTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	}

	// have to write current color buffer to source texture, because that is what gets compressed!
	GS::UpdateTextureSourceFromBuffer(TextureColorBuffer4b, ActivePaintTexture, /*bWaitForCompile=*/true);
	// notify material
	GS::DoInternalMaterialUpdates(ActivePaintMaterial);

	bCompressionPreviewEnabled = bSetToEnabled;
}

void UGSBaseTextureEditTool::EnsureCompressionPreviewDisabled()
{
	ToggleCompressionPreview(false);
}




#undef LOCTEXT_NAMESPACE