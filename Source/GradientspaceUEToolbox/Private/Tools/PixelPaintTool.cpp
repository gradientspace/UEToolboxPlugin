// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/PixelPaintTool.h"
#include "Engine/World.h"
#include "InteractiveToolManager.h"
#include "InteractiveGizmoManager.h"
#include "PreviewMesh.h"
#include "ToolDataVisualizer.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Util/ColorConstants.h"

#include "Utility/GSMaterialGraphUtil.h"
#include "Utility/GSUETextureUtil.h"
#include "Utility/GSTextureUtil.h"
#include "Math/GSVector4.h"
#include "Core/unsafe_vector.h"
#include "Core/FunctionRef.h"
#include "Mesh/TriangleMesh2.h"
#include "Math/GSIntAxisBox2.h"
#include "Image/GSImage.h"
#include "Image/GSPixelPainting.h"
#include "Sampling/ImageSampling.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"


#if WITH_EDITOR
#include "EditorIntegration/GSModelingModeSubsystem.h"
#endif

#define LOCTEXT_NAMESPACE "UGSMeshPixelPaintTool"

using namespace UE::Geometry;
using namespace GS;

class FPixelPaint_PixelsChange : public FToolCommandChange
{
public:
	struct PixelChangeInfo {
		int LinearPixelIndex;
		FLinearColor InitialColor;
		FLinearColor FinalColor;
	};
	TArray<PixelChangeInfo> ChangeList;

	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;
	virtual FString ToString() const override { return TEXT("FPixelPaint_PixelsChange"); }
};



UMeshSurfacePointTool* UGSMeshPixelPaintToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	UGSMeshPixelPaintTool* Tool = NewObject<UGSMeshPixelPaintTool>(SceneState.ToolManager);
	Tool->SetTargetWorld(SceneState.World);
	return Tool;
}


UGSMeshPixelPaintTool::UGSMeshPixelPaintTool()
{
	SetToolDisplayName(LOCTEXT("ToolName", "Pixel Paint"));
}

void UGSMeshPixelPaintTool::Setup()
{
	UGSBaseTextureEditTool::Setup();

	ColorSettings = NewObject<UGSTextureToolColorSettings>(this);
	ColorSettings->RestoreProperties(this, TEXT("PaintColors"));
	ColorSettings->Palette.Target = MakeShared<FGSColorPaletteTarget>();
	ColorSettings->Palette.Target->SetSelectedColor = [this](FLinearColor NewColor, bool bShiftDown) { this->SetActiveColor(NewColor, bShiftDown); };
	ColorSettings->Palette.Target->RemovePaletteColor = [this](FLinearColor RemoveColor) { this->ColorSettings->Palette.RemovePaletteColor(RemoveColor); };
	AddToolPropertySource(ColorSettings);

	Settings = NewObject<UPixelPaintToolSettings>(this);
	Settings->RestoreProperties(this);

	AddToolPropertySource(Settings);
	Settings->WatchProperty(Settings->ActiveTool, [&](EGSPixelPaintToolMode NewMode) { SetActiveToolMode(NewMode); });

	SetActiveToolMode(Settings->ActiveTool);

	SetNearestFilteringOverride(true);

	bool bHaveUVLayer = true;
	PreviewMesh->ProcessMesh([&](const FDynamicMesh3& Mesh) {
		const FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes()->PrimaryUV();
		if (UVOverlay == nullptr || UVOverlay->ElementCount() == 0)
			bHaveUVLayer = false;
	});
	if (bHaveUVLayer == false) {
		GetToolManager()->DisplayMessage(LOCTEXT("MissingUVs",
			"Could not build SurfaceCache for input mesh, so it is not paintable. Possibly no valid UVs?"), EToolMessageLevel::UserWarning);
	}

#if WITH_EDITOR
	if (UGSModelingModeSubsystem* Subsystem = UGSModelingModeSubsystem::Get())
		Subsystem->RegisterToolHotkeyBindings(this);
#endif
}



void UGSMeshPixelPaintTool::RegisterActions(FInteractiveToolActionSet& ActionSet)
{
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 100,
		TEXT("ToggleNearest"),
		LOCTEXT("ToggleNearest", "Toggle Nearest filtering"),
		LOCTEXT("ToggleNearest_Tooltip", "Toggle the active Filtering mode between Nearest and the texture's Default mode"),
		EModifierKey::None, EKeys::N,
		[this]() { SetNearestFilteringOverride(TextureSetSettings->FilterMode != EGSTextureFilterMode::Nearest); });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 200,
		TEXT("PickColor"),
		LOCTEXT("PickColor", "Pick Paint Color"),
		LOCTEXT("PickColor_Tooltip", "Select the Color under the cursor as the Paint color"),
		EModifierKey::Shift, EKeys::G,
		[this]() { PickColorAtCursor(false); });
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 201,
		TEXT("PickErase"),
		LOCTEXT("PickErase", "Pick Erase Color"),
		LOCTEXT("PickErase_Tooltip", "Select the Color under the cursor as the Erase color"),
		EModifierKey::Shift | EModifierKey::Control, EKeys::G,
		[this]() { PickColorAtCursor(true); });

}


void UGSMeshPixelPaintTool::SetActiveToolMode(EGSPixelPaintToolMode NewToolMode)
{
	if (Settings->ActiveTool != NewToolMode) {
		Settings->ActiveTool = NewToolMode;
	}
	Settings->PixelToolSettings.bVisible = false;
	if (NewToolMode == EGSPixelPaintToolMode::Pixel)
	{
		Settings->PixelToolSettings.bVisible = true;
		SetPixelToolMode();
	}
	NotifyOfPropertyChangeByTool(Settings);
}

void UGSMeshPixelPaintTool::SetPixelToolMode()
{
	if (CurrentToolMode == EGSPixelPaintToolMode::Pixel)
		return;
	CurrentToolMode = EGSPixelPaintToolMode::Pixel;
}



void UGSMeshPixelPaintTool::PickColorAtCursor(bool bSecondaryColor)
{
	FColor PickedColor = FColor::White;
	bool bNearestSampling = (ActivePaintTexture->Filter == TF_Nearest);
	if (GS::SampleTextureBufferAtUV(&TextureColorBuffer4b[0], PaintTextureWidth, PaintTextureHeight, (FVector2d)LastHitUVPos, PickedColor, bNearestSampling))
	{
		if (bSecondaryColor)
			ColorSettings->EraseColor = ConvertToLinear(PickedColor);
		else
			ColorSettings->PaintColor = ConvertToLinear(PickedColor);

		ColorSettings->Palette.AddNewPaletteColor(ConvertToLinear(PickedColor));
	}
}



void UGSMeshPixelPaintTool::SetActiveColor(FLinearColor NewColor, bool bSetSecondary, bool bDisableChangeNotification)
{
	if (bSetSecondary)
		ColorSettings->EraseColor = NewColor;
	else
		ColorSettings->PaintColor = NewColor;
	if (bDisableChangeNotification)
		ColorSettings->SilentUpdateWatched();
	NotifyOfPropertyChangeByTool(ColorSettings);
}




void UGSMeshPixelPaintTool::Shutdown(EToolShutdownType ShutdownType) 
{
	ColorSettings->SaveProperties(this, TEXT("PaintColors"));
	Settings->SaveProperties(this);

#if WITH_EDITOR
	if (UGSModelingModeSubsystem* Subsystem = UGSModelingModeSubsystem::Get())
		Subsystem->UnRegisterToolHotkeyBindings(this);
#endif

	UGSBaseTextureEditTool::Shutdown(ShutdownType);
}


void UGSMeshPixelPaintTool::Render(IToolsContextRenderAPI* RenderAPI)
{
}


void UGSMeshPixelPaintTool::OnTick(float DeltaTime)
{
	if (bInStroke)
	{
		bool bStampPending = true;
		//bool bStampPending = false;
		//double dt = 1.0 - ((double)Settings->Flow / 100.0);
		AccumStrokeTime += DeltaTime;
		//if (Settings->Flow == 100 || LastStampTime < 0 || (AccumStrokeTime - LastStampTime) > dt)
		//{
		//	bStampPending = true;
		//}

		if (bStampPending) {
			double dt = (LastStampTime < 0) ? 1.0 : (AccumStrokeTime - LastStampTime);
			AddStampFromWorldRay(LastWorldRay, dt);
			LastStampTime = AccumStrokeTime;
		}
	}

	CompletePendingPaintOps();
}


bool UGSMeshPixelPaintTool::HitTest(const FRay& WorldRay, FHitResult& OutHit)
{
	bInStroke = false;
	if (!ActivePaintTexture || !ActivePaintMaterial || PaintTextureWidth == 0 || PaintTextureHeight == 0)
		return false;

	return PreviewMesh->FindRayIntersection(WorldRay, OutHit);
}

void UGSMeshPixelPaintTool::OnBeginDrag(const FRay& WorldRay)
{
	ActiveStrokeInfo = MakeShared<GS::PixelPaintStroke>();
	FIndex4i ChannelFlags = Settings->ChannelFilter.AsFlags();
	ActiveStrokeInfo->ChannelFilter = GS::Vector4f((float)ChannelFlags.A, (float)ChannelFlags.B, (float)ChannelFlags.C, (float)ChannelFlags.D);
	ActiveStrokeInfo->BeginStroke();

	bInStroke = true;
	AccumStrokeTime = 0.0;
	LastStampTime = -99999;
	LastWorldRay = WorldRay;
}

void UGSMeshPixelPaintTool::OnUpdateDrag(const FRay& WorldRay)
{
	LastWorldRay = WorldRay;
}

void UGSMeshPixelPaintTool::OnEndDrag(const FRay& Ray)
{
	if (!bInStroke)
		return;

	CurrentTextureStrokeCount++;
	bInStroke = false;
	CompletePendingPaintOps();

	TUniquePtr<FPixelPaint_PixelsChange> NewChange = MakeUnique<FPixelPaint_PixelsChange>();
	NewChange->ChangeList.SetNum( (int)ActiveStrokeInfo->GetNumModifiedPixels() );
	int Index = 0;
	ActiveStrokeInfo->EnumerateModifiedPixels([&](const PixelPaintStroke::ModifiedPixel& PixelInfo) {
		NewChange->ChangeList[Index].LinearPixelIndex = PixelInfo.PixelIndex;
		NewChange->ChangeList[Index].InitialColor = (FLinearColor)PixelInfo.OriginalColor;
		NewChange->ChangeList[Index].FinalColor = (FLinearColor)ImageBuffer->GetPixel(PixelInfo.PixelIndex);
		Index++;
	});

	GetToolManager()->EmitObjectChange(this, MoveTemp(NewChange), LOCTEXT("PaintStroke", "Pixel Stroke"));

	ActiveStrokeInfo->EndStroke();
	// todo can probably re-use stroke...
	ActiveStrokeInfo.Reset();
}


void UGSMeshPixelPaintTool::CancelActiveStroke()
{
	PendingPixelStamps.Reset();

	// we are only currently doing this in the context of undo or changing texture, where
	// we push a full image update, so we just want to revert active stroke
	ActiveStrokeInfo->EnumerateModifiedPixels([&](const PixelPaintStroke::ModifiedPixel& PixelInfo) {
		ImageBuffer->SetPixel(PixelInfo.PixelIndex, (Vector4f)PixelInfo.OriginalColor);
		FColor NewFColor = ConvertToFColor((FLinearColor)PixelInfo.OriginalColor);
		TextureColorBuffer4b[PixelInfo.PixelIndex] = NewFColor;
	});

	ActiveStrokeInfo->EndStroke();
	ActiveStrokeInfo.Reset();

	bInStroke = false;
}

FInputRayHit UGSMeshPixelPaintTool::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	FHitResult OutHit;
	if (PreviewMesh->FindRayIntersection(PressPos.WorldRay, OutHit))
		return FInputRayHit(OutHit.Distance, OutHit.Normal);
	return FInputRayHit();
}
void UGSMeshPixelPaintTool::OnBeginHover(const FInputDeviceRay& DevicePos)
{
	OnUpdateHover(DevicePos);
}
bool UGSMeshPixelPaintTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	FHitResult OutHit;
	if (PreviewMesh->FindRayIntersection(DevicePos.WorldRay, OutHit)) {
		FVector3d LocalPoint, LocalNormal;
		ComputeHitPixel(DevicePos.WorldRay, OutHit, LocalPoint, LocalNormal, LastHitTriangleID, LastHitUVPos, LastHitPixel);
		//	IndicatorWorldFrame = FFrame3d(OutHit.ImpactPoint, OutHit.Normal);
	}
	return true;
}
void UGSMeshPixelPaintTool::OnEndHover()
{
}



void UGSMeshPixelPaintTool::OnActiveTargetTextureUpdated()
{
	FVector2i ImageDimensions(PaintTextureWidth, PaintTextureHeight);
	ImageBuffer = MakeShared<GS::Image4f>();
	ImageBuffer->Initialize(ImageDimensions.X, ImageDimensions.Y, [&](int64 LinearPixelIndex) {
		FColor SRGBColor = TextureColorBuffer4b[LinearPixelIndex];
		return (Vector4f)ConvertToLinear(SRGBColor);
	});

	if (SourcePaintTexture->Source.GetFormat() != ETextureSourceFormat::TSF_BGRA8)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("LossyMsg",
			"Only 8-bit Textures are fully supported. Painting on this Texture may lose precision"), EToolMessageLevel::UserWarning);
	}
}


void UGSMeshPixelPaintTool::PreTextureChangeOnUndoRedo()
{
	if (bInStroke)
		CancelActiveStroke();
}


void UGSMeshPixelPaintTool::AddStampFromWorldRay(const FRay& WorldRay, double dt)
{
	FHitResult OutHit;
	bool bHit = PreviewMesh->FindRayIntersection(WorldRay, OutHit);
	if (!bHit)
		return;

	int MaterialIDFilter = GetActivePaintMaterialID();

	int HitPixelX = -1, HitPixelY = -1;
	FVector3d LocalPoint = FVector3d::Zero(), LocalNormal = FVector3d::UnitZ();
	PreviewMesh->ProcessMesh([&](const FDynamicMesh3& Mesh) 
	{
		const FDynamicMeshMaterialAttribute* MaterialIDs = Mesh.Attributes()->GetMaterialID();
		if (MaterialIDs != nullptr && MaterialIDFilter >= 0 && MaterialIDs->GetValue(OutHit.FaceIndex) != MaterialIDFilter)
			return;

		LocalPoint = TargetTransform.InverseTransformPosition(OutHit.ImpactPoint);

		FTriangle3d Tri;
		Mesh.GetTriVertices(OutHit.FaceIndex, Tri.V[0], Tri.V[1], Tri.V[2]);
		FVector3d BaryCoords = Tri.GetBarycentricCoords(LocalPoint);

		const FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes()->PrimaryUV();
		if (UVOverlay->IsSetTriangle(OutHit.FaceIndex) == false)
			return;
		FVector2f U, V, W;
		UVOverlay->GetTriElements(OutHit.FaceIndex, U, V, W);

		FVector2f UVPos = (float)BaryCoords.X * U + (float)BaryCoords.Y * V + (float)BaryCoords.Z * W;

		// apply wrapping to UV coords  (could also support clamping?)
		UVPos = GS::WrapToUnitRect((Vector2f)UVPos);

		HitPixelX = (int)(UVPos.X * (float)PaintTextureWidth);
		HitPixelY = (int)(UVPos.Y * (float)PaintTextureHeight);

		LocalNormal = Mesh.GetTriNormal(OutHit.FaceIndex);
	});
	if (HitPixelX < 0 || HitPixelY < 0)
		return;

	FFrame3d HitFrame(LocalPoint);

	bool bApplyStamp = true;
	if (LastStampTime > 0) {		// not first stamp
		//if (CurrentToolMode == EGSPixelPaintToolMode::Brush) {
		//	double StepDist = Distance(LastHitFrame.Origin, HitFrame.Origin);
		//	AccumPathLength += StepDist;
		//	double StampSpacing = ((double)Settings->BrushToolSettings.Spacing / 100.0) * Settings->BrushToolSettings.BrushSize;
		//	if (AccumPathLength < StampSpacing)
		//		bApplyStamp = false;
		//}
	}

	LastHitFrame = HitFrame;
	if (bApplyStamp)
	{
		PendingPixelStamps.Add(PendingStamp{ WorldRay, HitFrame, OutHit.FaceIndex, FVector2i(HitPixelX, HitPixelY), dt });
		LastStampFrame = HitFrame;
		AccumPathLength = 0;
	}
}



void UGSMeshPixelPaintTool::CompletePendingPaintOps()
{
	if (CurrentToolMode == EGSPixelPaintToolMode::Pixel)
		ApplyPendingStamps_Pixel();
}

void UGSMeshPixelPaintTool::ApplyPendingStamps_Pixel()
{
	if (PendingPixelStamps.Num() == 0)
		return;
	FLinearColor PaintColor = (GetShiftToggle() || GetCtrlToggle()) ? ColorSettings->EraseColor : ColorSettings->PaintColor;

	AxisBox2i AccumPixelRect = AxisBox2i::Empty();
	bool bImageModified = false;
	for (PendingStamp& Stamp : PendingPixelStamps)
	{
		double StrokeAlpha = Settings->Alpha;
		//double StampPower = GS::Pow((double)Settings->BrushToolSettings.Flow / 100.0 + 0.05, 4.0);
		double StampPower = 1.0;
		ActiveStrokeInfo->AppendStrokeStamp(Stamp.Pixel, StampPower, (Vector4f)PaintColor, StrokeAlpha, *ImageBuffer,
			[&](const FVector2i& PixelPos, const Vector4f& NewLinearColor)
		{
			FColor NewFColor = ConvertToFColor((FLinearColor)NewLinearColor);
			int ImageIndex = PixelPos.Y * PaintTextureWidth + PixelPos.X;
			if (TextureColorBuffer4b[ImageIndex] != NewFColor) {
				TextureColorBuffer4b[ImageIndex] = NewFColor;
				bImageModified = true;
			}

			AccumPixelRect.Contain(PixelPos);
		});
	}
	PendingPixelStamps.Reset();
	if (!bImageModified)
		return;

	PushRegionImageUpdate(AccumPixelRect.Min.X, AccumPixelRect.Min.Y, AccumPixelRect.CountX(), AccumPixelRect.CountY());
}




void UGSMeshPixelPaintTool::UndoRedoPixelsChange(const FPixelPaint_PixelsChange& Change, bool bUndo)
{
	if (bInStroke)
		CancelActiveStroke();

	int N = Change.ChangeList.Num();
	for (int k = 0; k < N; ++k)
	{
		const auto& V = Change.ChangeList[k];
		FLinearColor NewColor = (bUndo) ? V.InitialColor : V.FinalColor;
		ImageBuffer->SetPixel(V.LinearPixelIndex, (Vector4f)NewColor);
		FColor NewFColor = ConvertToFColor(NewColor);
		TextureColorBuffer4b[V.LinearPixelIndex] = NewFColor;
	}

	PushFullImageUpdate();
}



void FPixelPaint_PixelsChange::Apply(UObject* Object)
{
	if (UGSMeshPixelPaintTool* Tool = Cast<UGSMeshPixelPaintTool>(Object))
		Tool->UndoRedoPixelsChange(*this, false);
}
void FPixelPaint_PixelsChange::Revert(UObject* Object)
{
	if (UGSMeshPixelPaintTool* Tool = Cast<UGSMeshPixelPaintTool>(Object))
		Tool->UndoRedoPixelsChange(*this, true);
}






#undef LOCTEXT_NAMESPACE
