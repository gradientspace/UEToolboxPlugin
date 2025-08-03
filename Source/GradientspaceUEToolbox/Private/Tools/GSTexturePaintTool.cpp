// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/GSTexturePaintTool.h"
#include "Engine/World.h"
#include "InteractiveToolManager.h"
#include "InteractiveGizmoManager.h"
#include "BaseGizmos/BrushStampIndicator.h"
#include "PreviewMesh.h"
#include "ToolDataVisualizer.h"
#include "Async/ParallelFor.h"
#include "Core/UEVersionCompat.h"


#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Image/ImageBuilder.h"
#include "Selections/MeshConnectedComponents.h"
#include "Spatial/SparseDynamicPointOctree3.h"
#include "Distance/DistPoint3Triangle3.h"
#include "Util/ColorConstants.h"

#include "AssetUtils/Texture2DBuilder.h"

#include "Utility/GSMaterialGraphUtil.h"
#include "Utility/GSUETextureUtil.h"
#include "Math/GSVector4.h"
#include "Color/GSColorBlending.h"
#include "Core/unsafe_vector.h"
#include "Core/FunctionRef.h"
#include "Mesh/MeshTopology.h"
#include "Spatial/AxisBoxTree2.h"
#include "Mesh/TriangleMesh2.h"
#include "Math/GSIntAxisBox2.h"
#include "Grid/GSSubGridMask2.h"
#include "Sampling/SurfaceTexelSampling.h"
#include "Sampling/ImageSampling.h"
#include "Image/GSSurfacePainting.h"

#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Rendering/Texture2DResource.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PixelFormat.h"
#include "RenderUtils.h"
#include "ImageUtils.h"
#include "Utility/GSTextureUtil.h"

#include "Assets/UGSImageStamp.h"
#include "Assets/UGSTextureImageStamp.h"
#include "Assets/UGSImageStampAsset.h"

// temp 
#include "TextureCompiler.h"

#include <unordered_map>

#if WITH_EDITOR
#include "EditorIntegration/GSModelingModeSubsystem.h"
#endif


#define LOCTEXT_NAMESPACE "UGSMeshTexturePaintTool"

using namespace UE::Geometry;
using namespace GS;


namespace
{
	const FString TexturePaintIndicatorGizmoType = TEXT("TexturePaintIndicatorGizmoType");

	TAutoConsoleVariable<int> CVarShowPaintPoints(
		TEXT("gradientspace.PaintTool.ShowPaintPoints"),
		0,
		TEXT("top secret!"));



	static double ComputeFalloff3D(double UnitDistance, double FalloffParam)
	{
		double FalloffStartRadius = GS::Clamp(1.0 - FalloffParam, 0.0, 1.0);
		if (UnitDistance <= FalloffStartRadius)
			return 1.0;

		double FalloffDistance = GS::Clamp((UnitDistance - FalloffStartRadius) / (1.0 - FalloffStartRadius), 0.0, 1.0);
		double W = (1.0 - FalloffDistance * FalloffDistance);
		return W * W * W;
	}


	struct Stamp_Radial
	{
		Vector4f StrokeColor = Vector4f::One();
		double Falloff = 0.5;
		double StampPower = 1.0f;
		Vector4f Evaluate(Vector3d SamplePos, const Frame3d& StampFrame, double StampRadius) 
		{
			Vector4f ResultColor(StrokeColor.X, StrokeColor.Y, StrokeColor.Z, 0.0f);
			double Dist = Distance((Vector3d)StampFrame.Origin, SamplePos);
			if (Dist < StampRadius) {
				double BrushUnitDist = Dist / StampRadius;
				float BrushAlpha = (float)(ComputeFalloff3D(BrushUnitDist, Falloff) * StampPower);
				ResultColor.W = StrokeColor.W * BrushAlpha;
			}
			else
				ResultColor.W = 0;
			return ResultColor;
		}
	};


	struct Stamp_Box
	{
		Vector4f StrokeColor = Vector4f::One();
		double Falloff = 0.5;		// ignored currently!
		double StampPower = 1.0f;

		double WidthX = 0.5;
		double HeightY = 1.0;

		Vector4f Evaluate(Vector3d SamplePos, const Frame3d& StampFrame, double StampRadius)
		{
			Vector4f ResultColor(StrokeColor.X, StrokeColor.Y, StrokeColor.Z, 0.0f);

			double SquareDim = StampRadius * FMathd::Sqrt2 * 0.5;
			double Width = SquareDim * WidthX;
			double Height = SquareDim * HeightY;
			Vector3d LocalPoint = StampFrame.ToLocalPoint(SamplePos);
			Vector2d LocalUV(LocalPoint.X, LocalPoint.Y);
			if ( GS::Abs(LocalPoint.X) < Width && GS::Abs(LocalPoint.Y) < Height )
				ResultColor.W = StrokeColor.W;

			return ResultColor;
		}
	};


	struct Stamp_Image
	{
		Vector4f StrokeColor = Vector4f::One();
		double Falloff = 0.5;		// ignored currently!
		double StampPower = 1.0f;

		const TArray<FColor>* ImageBuffer;
		FVector2i ImageDims;
		bool bImageIsSRGB = true;

		EGSImageStampColorMode ColorMode = EGSImageStampColorMode::RGBAColor;
		EGSImageStampChannel AlphaChannel = EGSImageStampChannel::Alpha;
		bool bMultiplyWithBrushAlpha = false;

		Vector4f Evaluate(Vector3d SamplePos, const Frame3d& StampFrame, double StampRadius)
		{
			Vector4f ResultColor(StrokeColor.X, StrokeColor.Y, StrokeColor.Z, 0.0f);

			double SquareRadius = StampRadius * FMathd::Sqrt2 * 0.5;	// half-length of square-side
			Vector3d LocalPoint = StampFrame.ToLocalPoint(SamplePos);
			Vector2d LocalUV(LocalPoint.X, LocalPoint.Y);

			if (GS::Abs(LocalPoint.X) > SquareRadius || GS::Abs(LocalPoint.Y) > SquareRadius)
				return ResultColor;

			double Dist = LocalPoint.Length();
			if (bMultiplyWithBrushAlpha && Dist > SquareRadius)
				return ResultColor;

			double BrushUnitDist = Dist / SquareRadius;
			float BrushAlpha = (float)(ComputeFalloff3D(BrushUnitDist, Falloff) * StampPower);
			// maybe...
			//BrushAlpha *= StrokeColor.W;

			double dx = ((LocalUV.X / SquareRadius) + 1.0) / 2.0;
			double PixelX = GS::Clamp(dx * ImageDims.X, 0.0, (double)ImageDims.X);
			double dy = ((LocalUV.Y / SquareRadius) + 1.0) / 2.0;
			double PixelY = GS::Clamp(dy * ImageDims.Y, 0.0, (double)ImageDims.Y);

			FColor Result;
			if ( GS::SampleTextureBufferAtUV( &(*ImageBuffer)[0], ImageDims.X, ImageDims.Y, FVector2d(dx, dy), Result))
			//if (GS::BilinearSampleCenteredGrid2<uint8_t, 4>((const uint8_t*)&ImageBuffer[0], ImageDims.X, ImageDims.Y, Vector2d(PixelX,PixelY), (uint8_t*)&Result))
			{
				FLinearColor ResultLinear = (bImageIsSRGB) ? FLinearColor(Result) : Result.ReinterpretAsLinear();

				if (ColorMode == EGSImageStampColorMode::RGBAColor) {
					ResultColor = (Vector4f)ResultLinear;
				}
				else if (ColorMode == EGSImageStampColorMode::RGBColor) {
					ResultColor = Vector4f(ResultLinear.R, ResultLinear.G, ResultLinear.B, 1.0);
				}
				else if (ColorMode == EGSImageStampColorMode::ChannelAsAlpha) {
					float ChannelAlpha = (AlphaChannel == EGSImageStampChannel::Value) ?
						ResultLinear.LinearRGBToHSV().B : ((Vector4f)ResultLinear)[(int)AlphaChannel];

					ResultColor = Vector4f(StrokeColor.X, StrokeColor.Y, StrokeColor.Z, ChannelAlpha);
				}
				else if (ColorMode == EGSImageStampColorMode::RGBColorWithChannelAsAlpha) {
					float ChannelAlpha = (AlphaChannel == EGSImageStampChannel::Value) ?
						ResultLinear.LinearRGBToHSV().B : ((Vector4f)ResultLinear)[(int)AlphaChannel];

					ResultColor = Vector4f(ResultLinear.R, ResultLinear.G, ResultLinear.B, ChannelAlpha);
				}

				if (bMultiplyWithBrushAlpha)
					ResultColor.W *= BrushAlpha;
			}

			return ResultColor;
		}
	};


}



class FTexturePaint_BrushColorChange : public FToolCommandChange
{
public:
	struct ChangeInfo
	{
		int PointID;
		FLinearColor InitialColor;
		FLinearColor FinalColor;
	};
	double ChangeAlpha = 1.0f;
	TArray<ChangeInfo> ChangeList;
	int LayerIndex = 0;

	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;
	virtual FString ToString() const override { return TEXT("FTexturePaint_BrushColorChange"); }
};



class FSurfaceTexelCache
{
public:
	GS::SurfaceTexelSampling Sampling;

	const FDynamicMesh3* Mesh;
	FSparseDynamicPointOctree3 Octree;
	TArray<int32> TriUVIslandIndex;

	int Initialize(const FDynamicMesh3* MeshIn, FVector2i ImageSize, int UVLayerIndex, int LimitToMaterialID = -1)
	{
		Mesh = MeshIn;

		//ensure(ImageSize.X == ImageSize.Y);		// other case is untested...
		double PixelToU = (double)1.0 / (double)ImageSize.X;
		double PixelToV = (double)1.0 / (double)ImageSize.Y;
		double MaxGutterPixelWidth = 2.0;
		double MaxGutterUVWidth = MaxGutterPixelWidth*PixelToU;

		const FDynamicMeshUVOverlay* UVLayer = Mesh->Attributes()->GetUVLayer(UVLayerIndex);
		if (UVLayer->ElementCount() == 0)
			return -2;
		if (UVLayer->IsCompact() == false || Mesh->IsCompact() == false)
			return -3;

		const FDynamicMeshMaterialAttribute* MaterialIDs = Mesh->Attributes()->GetMaterialID();
		bool bFilterForMaterialID = (MaterialIDs != nullptr && LimitToMaterialID >= 0);

		// build UV-space mesh
		GS::GeneralTriMesh2d UVMeshBld;
		UVMeshBld.AddVertexColor4f();
		MeshColor4fEditor ColorEd = UVMeshBld.Attributes.EditVertexColor4f();
		UVMeshBld.ReserveNewVertexIDs(UVLayer->MaxElementID());
		for (int32 elemid : UVLayer->ElementIndicesItr()) {
			int newid = UVMeshBld.AddPosition((Vector2d)UVLayer->GetElement(elemid));
			check(newid == elemid);		// currently require compactness
			ColorEd.SetColor(newid, Vector4f::One());		// do we need colors?? or is this just test code?
		}
		UVMeshBld.SetNumTriangleIDs(Mesh->MaxTriangleID());
		UVMeshBld.AddTriangleMaterial();
		MeshTriMaterialEditor MatEd = UVMeshBld.Attributes.EditTriMaterial();
		for (int tid : Mesh->TriangleIndicesItr()) {
			bool bIsValidTri = UVLayer->IsSetTriangle(tid);
			if (bIsValidTri && bFilterForMaterialID && MaterialIDs->GetValue(tid) != LimitToMaterialID)
				bIsValidTri = false;

			if (bIsValidTri)
				UVMeshBld.Triangles.SetTriangle(tid, (Index3i)UVLayer->GetTriangle(tid), Mesh->GetTriangleGroup(tid));
			else
				UVMeshBld.SetTriangle(tid, Index3i::NegOne());

			int MaterialID = (MaterialIDs) ? MaterialIDs->GetValue(tid) : -1;
			MatEd.SetMaterial(tid, MaterialID);
		}
		GS::ConstMeshView2d UVMeshf = UVMeshBld.GetMeshView();

		// this could be done in parallel w/ sampling...
		// build topology for UV-space mesh
		GS::MeshTopology UVTopo;
		UVTopo.Build(UVMeshf.GetNumVertexIDs(),
			[&](int vid) { return UVMeshf.IsValidVertexID(vid); },
			UVMeshf.GetNumTriangleIDs(),
			[&](int tid, GS::Index3i& TriV) {
				if (UVMeshf.IsValidTriangleID(tid) == false) return false;
				TriV = UVLayer->GetTriangle(tid); 
				return (TriV.A >= 0 && TriV.B >= 0 && TriV.C >= 0);
			}
		);

		// find out UV island for each triangle
		FMeshConnectedComponents UVIslandComponents(Mesh);
		UVIslandComponents.FindConnectedTriangles([&](int32 t0, int32 t1) {
			// TODO: remove UVLayer check here...should never hit
			bool b1 = UVLayer->AreTrianglesConnected(t0, t1);
			bool b2 = UVTopo.AreTrisConnected(t0, t1);
			ensure(b1 == b2);
			return b2;
		});
		TriUVIslandIndex.SetNum(Mesh->MaxTriangleID());
		int IslandID = 1;
		for (auto& Component : UVIslandComponents.Components) {
			for (int tid : Component.Indices)
				TriUVIslandIndex[tid] = IslandID;
			IslandID++;
		}

		Sampling.Build(UVMeshf, ImageSize.X, ImageSize.Y,
			[&](int TriangleID, Vector3d BaryCoords) {
			return (Vector3d)Mesh->GetTriBaryPoint(TriangleID, BaryCoords.X, BaryCoords.Y, BaryCoords.Z);
		});
		int NumSamples = Sampling.NumSamples();

		// assign UV island values
		for (int k = 0; k < NumSamples; ++k)
			Sampling[k].UVIsland = TriUVIslandIndex[Sampling[k].TriangleID];

		// build octree
		double MaxDim = Sampling.SampleBounds.Diagonal().AbsMax();
		Octree.ConfigureFromPointCountEstimate(MaxDim, NumSamples);
		Octree.ParallelInsertDensePointSet(NumSamples, [&](int i) { return Sampling[i].SurfacePos; });

		return 0;
	}


	void ProcessTexelsInRadius(Vector3d Center, double Radius,
		TFunctionRef<void(int PointIndex, const GS::TexelPoint3d& Point)> ProcessPointFunc,
		TArray<int32>& TmpQueryBuffer,
		TArray<const FSparsePointOctreeCell*>& TmpOctreeQueryCache ) const
	{
		FAxisAlignedBox3d Bounds(Center, Radius);
		TmpQueryBuffer.Reset();
		Octree.ParallelRangeQuery(Bounds, [&](int idx) {
			return DistanceSquared(Sampling[idx].SurfacePos, Center) < Radius * Radius;
		}, TmpQueryBuffer, &TmpOctreeQueryCache);

		int N = TmpQueryBuffer.Num();
		for (int i = 0; i < N; ++i) {
			int idx = TmpQueryBuffer[i];
			ProcessPointFunc(idx, Sampling[idx]);
		}
	}
};




class FSurfacePaintStroke : public GS::SurfacePaintStroke
{
public:
	unsafe_vector<int> FramePoints;
	const FSurfaceTexelCache* CurCache = nullptr;
	TArray<int32> TmpBuffer1;
	TArray<const FSparsePointOctreeCell*> OctreeQueryCache;

	const FDynamicMesh3* TargetMesh = nullptr;
	int InitialTriangleID = -1;
	int InitialGroupID = -1;
	int UVIslandIndex = -1;
	bool bFilterByUVIsland = false;
	bool bFilterByGroup = false;

	// this is set to the Tool Settings StrokeAlpha value on construction. But it's not used in AppendStrokeStampEx below...
	// (maybe it should be now). This was to transmit the alpha to Undo, but maybe that is not necessary as the
	// Tool StrokeAlpha can't change during a single stroke
	double StokeAlpha = 1.0f;


	void BeginStrokeExt(const FSurfaceTexelCache* SurfaceCache)
	{
		CurCache = SurfaceCache;
		BeginStroke();
	}


	void ComputeStampPoints(Frame3d StampFrame, double Radius,
		const TSet<int32>* TriangleROIFilter = nullptr)
	{
		FramePoints.clear();
		TmpBuffer1.Reset();
		CurCache->ProcessTexelsInRadius(StampFrame.Origin, Radius,
			[&](int PointID, const GS::TexelPoint3d& Point) {
			if (TriangleROIFilter && TriangleROIFilter->Contains(Point.TriangleID) == false)
				return;
			if (bFilterByUVIsland && Point.UVIsland != UVIslandIndex)
				return;
			if (bFilterByGroup && TargetMesh->GetTriangleGroup(Point.TriangleID) != InitialGroupID)
				return;
			FramePoints.add(PointID);
		}, TmpBuffer1, OctreeQueryCache);
	}

	void AppendStrokeStamp_Radial(
		Frame3d StampFrame, double Radius, 
		double Falloff, double StampPower, Vector4f StrokeColor, 
		double StrokeAlpha,
		GS::SurfaceTexelColorCache& UseColorCache,
		TFunctionRef<void(int TexelPointID, const Vector2i& PixelPos, const Vector4f& NewColor)> WritePixelFunc,
		const TSet<int32>* TriangleROIFilter = nullptr ) 
	{
		// populates FramePoints
		FramePoints.clear();
		ComputeStampPoints(StampFrame, Radius, TriangleROIFilter);

		Stamp_Radial Stamp;
		//Stamp_Box Stamp;
		Stamp.StrokeColor = StrokeColor; Stamp.StampPower = StampPower; 
		Stamp.Falloff = Falloff;
		auto RadialStampFunc = [&Stamp](Vector3d SamplePos, const Frame3d& StampFrame, double StampRadius) { 
			return Stamp.Evaluate(SamplePos, StampFrame, StampRadius); };

		AppendStrokeStamp(StampFrame, Radius, RadialStampFunc, StrokeAlpha,
			CurCache->Sampling, FramePoints.get_view(), UseColorCache, WritePixelFunc);
	}


	void AppendStrokeStamp_Textured(
		Frame3d StampFrame, double Radius, 
		//double Falloff, double StampPower, 
		//const TArray<FColor>* ImageBuffer, FVector2i ImageDims, bool bIsSRGB,
		FunctionRef<Vector4f(Vector3d SamplePos, const Frame3d& StampFrame, double StampRadius)> StampFunc,
		double StrokeAlpha,
		GS::SurfaceTexelColorCache& UseColorCache,
		TFunctionRef<void(int TexelPointID, const Vector2i& PixelPos, const Vector4f& NewColor)> WritePixelFunc,
		const TSet<int32>* TriangleROIFilter = nullptr ) 
	{
		// populates FramePoints
		FramePoints.clear();
		ComputeStampPoints(StampFrame, Radius, TriangleROIFilter);

		//Stamp_Image Stamp;
		//Stamp.bImageIsSRGB = bIsSRGB;
		//Stamp.ImageBuffer = ImageBuffer;
		//Stamp.ImageDims = ImageDims;
		//Stamp.Falloff = Falloff;

		//Stamp.ColorMode = ;
		//Stamp.AlphaChannel = ;
		//Stamp.bMultiplyWithBrushAlpha = ;

		//auto RadialStampFunc = [&Stamp](Vector3d SamplePos, const Frame3d& StampFrame, double StampRadius) { 
		//	return Stamp.Evaluate(SamplePos, StampFrame, StampRadius); };

		AppendStrokeStamp(StampFrame, Radius, StampFunc, StrokeAlpha,
			CurCache->Sampling, FramePoints.get_view(), UseColorCache, WritePixelFunc);
	}

};







UMeshSurfacePointTool* UGSMeshTexturePaintToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	UGSMeshTexturePaintTool* Tool = NewObject<UGSMeshTexturePaintTool>(SceneState.ToolManager);
	Tool->SetTargetWorld(SceneState.World);
	return Tool;
}


UGSMeshTexturePaintTool::UGSMeshTexturePaintTool()
{
	SetToolDisplayName(LOCTEXT("ToolName","Texture Paint"));
}

void UGSMeshTexturePaintTool::Setup()
{
	UGSBaseTextureEditTool::Setup();

	ColorSettings = NewObject<UGSTextureToolColorSettings>(this);
	ColorSettings->RestoreProperties(this, TEXT("PaintColors"));
	ColorSettings->Palette.Target = MakeShared<FGSColorPaletteTarget>();
	ColorSettings->Palette.Target->SetSelectedColor = [this](FLinearColor NewColor, bool bShiftDown) { this->SetActiveColor(NewColor, bShiftDown); };
	ColorSettings->Palette.Target->RemovePaletteColor = [this](FLinearColor RemoveColor) { this->ColorSettings->Palette.RemovePaletteColor(RemoveColor); };
	AddToolPropertySource(ColorSettings);

	Settings = NewObject<UTexturePaintToolSettings>(this);
	Settings->RestoreProperties(this);

	AddToolPropertySource(Settings);
	Settings->WatchProperty(Settings->ActiveTool, [&](EGSTexturePaintToolMode NewMode) { SetActiveToolMode(NewMode); });
	Settings->WatchProperty(Settings->BrushStamp, [&](TObjectPtr<UGSImageStampAsset> NewStamp) { OnStampTextureModified(); });

	TSharedPtr<FGSToggleButtonTarget> StampTogglesTarget = MakeShared<FGSToggleButtonTarget>();
	StampTogglesTarget->GetToggleValue = [this](FString ToggleName) {
		if (ToggleName == TEXT("IgnoreFalloff")) return Settings->bIgnoreFalloff;
		else if (ToggleName == TEXT("AlignToStroke")) return Settings->bAlignToStroke;
		else return false;
	};
	StampTogglesTarget->SetToggleValue = [this](FString ToggleName, bool bNewValue) {
		if (ToggleName == TEXT("IgnoreFalloff")) Settings->bIgnoreFalloff = bNewValue;
		else if (ToggleName == TEXT("AlignToStroke")) Settings->bAlignToStroke = bNewValue;
	};
	Settings->StampToggles.Target = StampTogglesTarget;
	Settings->StampToggles.AddToggle(TEXT("IgnoreFalloff"), LOCTEXT("IgnoreFalloff", "Ignore Falloff"),
		LOCTEXT("IgnoreFalloffTooltip", "Ignore the Brush Falloff settings when applying the Stamp"));
	Settings->StampToggles.AddToggle(TEXT("AlignToStroke"), LOCTEXT("AlignToStroke", "Align To Stroke"),
		LOCTEXT("AlignToStrokeTooltip", "Align Stamps with the brush-stroke path"));
	Settings->StampToggles.bShowLabel = false;
	Settings->StampToggles.bCentered = true;
	Settings->StampToggles.bCalculateFixedButtonWidth = true;


	TSharedPtr<FGSToggleButtonTarget> FiltersTarget = MakeShared<FGSToggleButtonTarget>();
	FiltersTarget->GetToggleValue = [this](FString ToggleName) {
		if (ToggleName == TEXT("UVIsland")) return Settings->bRestrictToUVIsland;
		else if (ToggleName == TEXT("Group")) return Settings->bRestrictToGroup;
		else return false;
	};
	FiltersTarget->SetToggleValue = [this](FString ToggleName, bool bNewValue) {
		if (ToggleName == TEXT("UVIsland")) Settings->bRestrictToUVIsland = bNewValue;
		else if (ToggleName == TEXT("Group")) Settings->bRestrictToGroup = bNewValue;
	};
	Settings->RegionFilters.Target = FiltersTarget;
	Settings->RegionFilters.AddToggle(TEXT("UVIsland"), LOCTEXT("UVIslandFilter", "UV Island"),
		LOCTEXT("UVIslandFilterTooltip", "Restrict painting to UV island containing initial cursor position"));
	Settings->RegionFilters.AddToggle(TEXT("Group"), LOCTEXT("GroupFilter", "Polygroup"),
		LOCTEXT("GroupFilterTooltip", "Restrict painting to Polygroup containing initial cursor position"));
	Settings->RegionFilters.bShowLabel = true;
	Settings->RegionFilters.bCentered = false;
	Settings->RegionFilters.bCalculateFixedButtonWidth = true;

	SetActiveToolMode(Settings->ActiveTool);

	// register and spawn brush indicator gizmo
	GetToolManager()->GetPairedGizmoManager()->RegisterGizmoType(TexturePaintIndicatorGizmoType, NewObject<UBrushStampIndicatorBuilder>());
	BrushIndicator = GetToolManager()->GetPairedGizmoManager()->CreateGizmo<UBrushStampIndicator>(TexturePaintIndicatorGizmoType, FString(), this);
	BrushIndicator->LineThickness = 1.0;
	BrushIndicator->bDrawIndicatorLines = true;
	BrushIndicator->bDrawSecondaryLines = true;
	BrushIndicator->bDrawRadiusCircle = true;
	BrushIndicator->LineColor = FLinearColor(0.9f, 0.4f, 0.4f);

	// configure adaptive brush size
	FAxisAlignedBox3d MeshBounds;
	PreviewMesh->ProcessMesh([&](const FDynamicMesh3& Mesh) {
		MeshBounds = Mesh.GetBounds();
	});
	FVector3d WorldDimensions = MeshBounds.Diagonal() * TargetScale.GetAbs();
	double AvgDimension = (WorldDimensions.X+WorldDimensions.Y+WorldDimensions.Z) / 3.0;
	FInterval1d BrushRelativeSizeRange = FInterval1d(AvgDimension * 0.01, AvgDimension * 0.5);


	BrushWorldSizeRange = TInterval<double>(BrushRelativeSizeRange.Min, BrushRelativeSizeRange.Max);
	// this function is not exported so we have to do it ourselves
	//Settings->BrushToolSettings.BrushSize.InitializeWorldSizeRange(
	//	TInterval<float>((float)BrushRelativeSizeRange.Min, (float)BrushRelativeSizeRange.Max));
	//Settings->BrushToolSettings.BrushSize.WorldSizeRange = TInterval<float>((float)BrushRelativeSizeRange.Min, (float)BrushRelativeSizeRange.Max);
	//if (Settings->BrushToolSettings.BrushSize.WorldRadius < Settings->BrushToolSettings.BrushSize.WorldSizeRange.Min)
	//	Settings->BrushToolSettings.BrushSize.WorldRadius = Settings->BrushToolSettings.BrushSize.WorldSizeRange.Interpolate(0.2);
	//else if (Settings->BrushToolSettings.BrushSize.WorldRadius > Settings->BrushToolSettings.BrushSize.WorldSizeRange.Max)
	//	Settings->BrushToolSettings.BrushSize.WorldRadius = Settings->BrushToolSettings.BrushSize.WorldSizeRange.Interpolate(0.8);

	LayerSettings = NewObject<UTexturePaintToolLayerSettings>(this);
	AddToolPropertySource(LayerSettings);
	
	TSharedPtr<FGSActionButtonTarget> LayerActionsTarget = MakeShared<FGSActionButtonTarget>();
	LayerActionsTarget->ExecuteCommand = [this](FString CommandName) { 
		if (CommandName == TEXT("Enable"))			EnableTemporaryPaintLayer(true);
		else if (CommandName == TEXT("Commit"))		CommitTemporaryPaintLayer(true);
		else if (CommandName == TEXT("Cancel"))		CancelTemporaryPaintLayer(true);
	};
	LayerActionsTarget->IsCommandEnabled = [this](FString CommandName) {
		return (CommandName == TEXT("Enable")) ? (!bPaintLayerEnabled) : (bPaintLayerEnabled);
	};
	LayerSettings->LayerActions.Target = LayerActionsTarget;
	LayerSettings->LayerActions.AddAction("Enable",		LOCTEXT("EnableTempLayer", "Enable OverLayer"),
		LOCTEXT("EnableTempLayerTooltip", "Enable a temporary painting layer that will be composited with the current image on Commit"));
	LayerSettings->LayerActions.AddAction("Cancel",		LOCTEXT("CancelTempLayer", "Cancel"),
		LOCTEXT("CancelTempLayerTooltip", "Discard the active temporary painting layer"), FColor(145, 0, 0));
	LayerSettings->LayerActions.AddAction("Commit",		LOCTEXT("CommitTempLayer", "Commit"),
		LOCTEXT("CommitTempLayerTooltip", "Composite the temporary painting layer into the base image"), FColor(0, 60, 0));

	LayerSettings->WatchProperty(LayerSettings->LayerAlpha, [this](double NewAlpha) {
		if (bPaintLayerEnabled)
			RebuildBufferFromLayerColorCache(true, LayerSettings->LayerAlpha);
	});

	EnableCompressionPreviewPanel();

#if WITH_EDITOR
	if (UGSModelingModeSubsystem* Subsystem = UGSModelingModeSubsystem::Get())
		Subsystem->RegisterToolHotkeyBindings(this);
#endif
}



void UGSMeshTexturePaintTool::RegisterActions(FInteractiveToolActionSet& ActionSet)
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

	//ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 200,
	//	TEXT("PixelMode"),
	//	LOCTEXT("PixelMode", "Pixel Painting"),
	//	LOCTEXT("PixelMode_Tooltip", "Set the active painting mode to Pixel Painting"),
	//	EModifierKey::None, EKeys::One,
	//	[this]() { SetActiveToolMode(EGSTexturePaintToolMode::Pixel); });
	//ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 201,
	//	TEXT("BrushMode"),
	//	LOCTEXT("BrushMode", "Brush Painting"),
	//	LOCTEXT("BrushMode_Tooltip", "Set the active painting mode to Brush Painting"),
	//	EModifierKey::None, EKeys::Two,
	//	[this]() { SetActiveToolMode(EGSTexturePaintToolMode::Brush); });

}


double UGSMeshTexturePaintTool::GetBrushSizeWorld() const
{
	// argh this function isn't exported!
	//double R = 2.0 * Settings->BrushToolSettings.BrushSize.GetWorldRadius();

	//double R = 2.0 * Settings->BrushToolSettings.BrushSize.WorldRadius;
	//if (Settings->BrushToolSettings.BrushSize.SizeType == EBrushToolSizeType::Adaptive) {
	//	double t = FMath::Max(0, Settings->BrushToolSettings.BrushSize.AdaptiveSize);
	//	t *= t;
	//	R = Settings->BrushToolSettings.BrushSize.WorldSizeRange.Interpolate(t);
	//}

	double t = FMath::Max(0, Settings->BrushToolSettings.BrushSize);
	t *= t;
	double R = BrushWorldSizeRange.Interpolate(t);

	return R;
}

double UGSMeshTexturePaintTool::GetBrushSizeLocal() const
{
	double WorldSize = GetBrushSizeWorld();
	double ScaleFactor = ( GS::Abs(TargetScale.X) + GS::Abs(TargetScale.Y) + GS::Abs(TargetScale.Z)) / 3.0;
	return WorldSize / ScaleFactor;
}

UE::Geometry::FIndex4i UGSMeshTexturePaintTool::GetActiveChannelViewMask() const
{
	return Settings->ChannelMask.AsFlags();
}

void UGSMeshTexturePaintTool::SetActiveToolMode(EGSTexturePaintToolMode NewToolMode)
{
	if (Settings->ActiveTool != NewToolMode) {
		Settings->ActiveTool = NewToolMode;
	}
	Settings->BrushToolSettings.bVisible = false;
	if (NewToolMode == EGSTexturePaintToolMode::Brush)
	{
		Settings->BrushToolSettings.bVisible = true;
		SetBrushToolMode();
	}
	NotifyOfPropertyChangeByTool(Settings);
}
void UGSMeshTexturePaintTool::SetBrushToolMode()
{
	if (CurrentToolMode == EGSTexturePaintToolMode::Brush)
		return;
	CurrentToolMode = EGSTexturePaintToolMode::Brush;
}




void UGSMeshTexturePaintTool::Shutdown(EToolShutdownType ShutdownType) 
{
	ColorSettings->SaveProperties(this, TEXT("PaintColors"));
	Settings->SaveProperties(this);

	GetToolManager()->GetPairedGizmoManager()->DestroyAllGizmosByOwner(this);
	BrushIndicator = nullptr;
	GetToolManager()->GetPairedGizmoManager()->DeregisterGizmoType(TexturePaintIndicatorGizmoType);

	// do not actually need to do this as the paint layer is already composited into 
	// the color buffer in the base texture and the base texture doens't know about
	// the layer, and we can't undo back into painting..
	//if (bPaintLayerEnabled)
	//	CommitTemporaryPaintLayer(false);

	// release memory
	SurfaceCache.Reset();
	ColorCache.Reset();

#if WITH_EDITOR
	if (UGSModelingModeSubsystem* Subsystem = UGSModelingModeSubsystem::Get())
		Subsystem->UnRegisterToolHotkeyBindings(this);
#endif

	UGSBaseTextureEditTool::Shutdown(ShutdownType);
}


void UGSMeshTexturePaintTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	UGSBaseTextureEditTool::Render(RenderAPI);

	FToolDataVisualizer Draw;
	Draw.BeginFrame(RenderAPI);
	Draw.PushTransform((FTransform)TargetTransform);

	if (CVarShowPaintPoints.GetValueOnAnyThread() != 0)
	{
		int NumTexels = SurfaceCache->Sampling.NumSamples();
		for (int k = 0; k < NumTexels; ++k)
		{
			const auto& Texel = SurfaceCache->Sampling[k];
			FLinearColor Color = (FLinearColor)ColorCache->GetColor(k);
			Draw.DrawPoint(Texel.SurfacePos, Color, 2.0, true);
		}
	}

	//if (true)
	//{
	//	for (int k = 0; k < AccumStrokeStamps.Num() - 1; ++k)
	//		Draw.DrawLine(
	//			AccumStrokeStamps[k].LocalFrame.PointAt(FVector3d(0,0,0.1)),
	//			AccumStrokeStamps[k+1].LocalFrame.PointAt(FVector3d(0,0,0.1)), FLinearColor::White, 1.0f);
	//}

	Draw.EndFrame();
}


void UGSMeshTexturePaintTool::OnTick(float DeltaTime)
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

		// If we are using lazy brush, give a bit of lead time before we start stamping.
		// This lets the cursor get further away before we start lerping.
		double lazy = (double)Settings->BrushToolSettings.Lazyness / 100.0;
		if (Settings->BrushToolSettings.Lazyness > 0 && AccumStrokeTime < 0.15*lazy )
		{
			bStampPending = false;
		}

		if (bStampPending) {
			double dt = (LastStampTime < 0) ? 1.0 : (AccumStrokeTime - LastStampTime);

			FRay NextStampRay = CurWorldRay;
			FVector2d NextStampCursorPos = CurWorldRayCursorPos;
			if (Settings->BrushToolSettings.Lazyness > 0) {

				// using simple lerp-based drag to do lazy is not good - should be using mass-spring sim
				// (and stamp-interp should also do that, based on 2D cursor positions...)
				double t = FMathd::Lerp(0.05, 1.0, GS::Pow(1.0-lazy, 4.0) );
				FVector2d LerpCursorPos = Lerp(LastStampWorldRayCursorPos, CurWorldRayCursorPos, t);
				if ( Distance(LerpCursorPos, CurWorldRayCursorPos) > 1.0) {		// this is pixel distance threshold
					NextStampCursorPos = LerpCursorPos;
					NextStampRay = LastViewInfo.GetRayAtPixel(NextStampCursorPos);
				}
			} 

			AddStampFromWorldRay(NextStampCursorPos, NextStampRay, dt);
			LastStampTime = AccumStrokeTime;
			LastStampWorldRay = NextStampRay;
			LastStampWorldRayCursorPos = NextStampCursorPos;
		}
	}

	// always using HoverStamp here because it's position should always be up-to-date for the current apply-stamp...
	BrushIndicator->bVisible = false;
	if (CurrentToolMode == EGSTexturePaintToolMode::Brush) {
		BrushIndicator->bVisible = true;
		float FalloffT = (float)Settings->BrushToolSettings.Falloff / 100.0f;
		BrushIndicator->Update((float)GetBrushSizeWorld() * 0.5f,
			IndicatorWorldFrame.ToFTransform(), 1.0f - FalloffT);
	}

	CompletePendingPaintOps();
}


void UGSMeshTexturePaintTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	CurWorldRayCursorPos = PressPos.ScreenPosition;
	Super::OnClickPress(PressPos);
}

void UGSMeshTexturePaintTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	CurWorldRayCursorPos = DragPos.ScreenPosition;
	Super::OnClickDrag(DragPos);
}


bool UGSMeshTexturePaintTool::HitTest(const FRay& WorldRay, FHitResult& OutHit)
{
	bInStroke = false;
	if (!ActivePaintTexture || !ActivePaintMaterial || PaintTextureWidth == 0 || PaintTextureHeight == 0)
		return false;

	return PreviewMesh->FindRayIntersection(WorldRay, OutHit);
}

void UGSMeshTexturePaintTool::OnBeginDrag(const FRay& WorldRay)
{
	EnsureCompressionPreviewDisabled();

	if (SurfaceCache->Sampling.NumSamples() == 0)		// cannot paint if we do not have valid sampling
		return;

	ActiveStrokeInfo = MakeShared<FSurfacePaintStroke>();
	FIndex4i ChannelFlags = Settings->ChannelMask.AsFlags();
	if (bPaintLayerEnabled == false) 
		ActiveStrokeInfo->ChannelMask = GS::Vector4f((float)ChannelFlags.A, (float)ChannelFlags.B, (float)ChannelFlags.C, (float)ChannelFlags.D);
	ActiveStrokeInfo->StokeAlpha = Settings->Alpha;

	if (LastHitTriangleID >= 0) {
		ActiveStrokeInfo->TargetMesh = PreviewMesh->GetMesh();
		ActiveStrokeInfo->InitialTriangleID = LastHitTriangleID;
		ActiveStrokeInfo->InitialGroupID = PreviewMesh->GetMesh()->GetTriangleGroup(LastHitTriangleID);
		ActiveStrokeInfo->UVIslandIndex = SurfaceCache->TriUVIslandIndex[LastHitTriangleID];
		ActiveStrokeInfo->bFilterByUVIsland = Settings->bRestrictToUVIsland;
		ActiveStrokeInfo->bFilterByGroup = Settings->bRestrictToGroup;
	}

	ActiveStrokeInfo->BeginStrokeExt(SurfaceCache.Get());

	bInStroke = true;
	AccumStrokeTime = 0.0;
	LastStampTime = -99999;
	CurWorldRay = WorldRay;
	LastStampWorldRay = CurWorldRay;
	LastStampWorldRayCursorPos = CurWorldRayCursorPos;
	PendingOrientedStrokeState = -1;
	AccumPathLength = 0;

	bIsOrientedStroke = (bHaveStampTexture && Settings->bAlignToStroke);
	PendingOrientedStrokeState = 0;
}

void UGSMeshTexturePaintTool::OnUpdateDrag(const FRay& WorldRay)
{
	CurWorldRay = WorldRay;
}

void UGSMeshTexturePaintTool::OnEndDrag(const FRay& Ray)
{
	if (!bInStroke)
		return;

	// On mouse-up in an oriented stroke, we may have not actually placed any stamps yet.
	// In that case, drop a single stamp at the initial click-point
	if (bIsOrientedStroke) {
		if (PendingOrientedStrokeState >= 0)
			PendingPixelStamps.Add(PendingInitialStamp);
		else
			PushPendingOrientedStrokesToQueue(true);
	}
	bIsOrientedStroke = false;
	PendingOrientedStrokeState = -1;


	CurrentTextureStrokeCount++;
	bInStroke = false;
	CompletePendingPaintOps();


	// TODO: this change needs to be emitted against color cache...
	TSharedPtr<GS::SurfaceTexelColorCache>& ActiveColorCache = GetActiveColorCache();

	TUniquePtr<FTexturePaint_BrushColorChange> NewChange = MakeUnique<FTexturePaint_BrushColorChange>();
	NewChange->ChangeList.SetNum((int)ActiveStrokeInfo->GetNumStrokePoints());
	NewChange->LayerIndex = (bPaintLayerEnabled) ? 1 : 0;
	NewChange->ChangeAlpha = ActiveStrokeInfo->StokeAlpha;
	int Index = 0;
	ActiveStrokeInfo->EnumerateStrokePoints([&](const FSurfacePaintStroke::StrokePoint& Point) {
		NewChange->ChangeList[Index].PointID = Point.Index;
		NewChange->ChangeList[Index].InitialColor = (FLinearColor)Point.OriginalColor;
		NewChange->ChangeList[Index].FinalColor = (FLinearColor)ActiveColorCache->GetColor(Point.Index);
		Index++;
	});

	GetToolManager()->EmitObjectChange(this, MoveTemp(NewChange), LOCTEXT("PaintStroke", "Paint Stroke"));

	AccumStrokeStamps.Reset();
	ActiveStrokeInfo->EndStroke();
	ActiveStrokeInfo.Reset();

	// add stroke color to palette if it doesn't exist
	ColorSettings->Palette.AddNewPaletteColor(ColorSettings->PaintColor);
}


void UGSMeshTexturePaintTool::CancelActiveStroke()
{
	PendingPixelStamps.Reset();

	// we are only currently doing this in the context of undo or changing texture, where
	// we push a full image update, so we just want to revert active stroke
	TSharedPtr<GS::SurfaceTexelColorCache>& UpdateColorCache = GetActiveColorCache();
	ActiveStrokeInfo->EnumerateStrokePoints([&](const FSurfacePaintStroke::StrokePoint& Point) {
		UpdateColorCache->SetColor(Point.Index, (Vector4f)Point.OriginalColor);

		FLinearColor NewColor = CalcCompositedLayerColor(Point.Index, (FLinearColor)Point.OriginalColor, ActiveStrokeInfo->StokeAlpha);

		FVector2i PixelPos = SurfaceCache->Sampling[Point.Index].PixelPos;
		int ImageIndex = PixelPos.Y * PaintTextureWidth + PixelPos.X;
		FColor NewFColor = ConvertToFColor(NewColor);
		TextureColorBuffer4b[ImageIndex] = NewFColor;
	});

	ActiveStrokeInfo->EndStroke();
	ActiveStrokeInfo.Reset();

	bInStroke = false;
}

FInputRayHit UGSMeshTexturePaintTool::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	FHitResult OutHit;
	if (PreviewMesh->FindRayIntersection(PressPos.WorldRay, OutHit))
		return FInputRayHit(OutHit.Distance, OutHit.Normal);
	return FInputRayHit();
}
void UGSMeshTexturePaintTool::OnBeginHover(const FInputDeviceRay& DevicePos)
{
	OnUpdateHover(DevicePos);
}
bool UGSMeshTexturePaintTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	FHitResult OutHit;
	if (PreviewMesh->FindRayIntersection(DevicePos.WorldRay, OutHit)) {
		IndicatorWorldFrame = FFrame3d(OutHit.ImpactPoint, OutHit.Normal);

		FVector3d LocalPoint, LocalNormal;
		ComputeHitPixel(DevicePos.WorldRay, OutHit, LocalPoint, LocalNormal, LastHitTriangleID, LastHitUVPos, LastHitPixel);
	}
	return true;
}
void UGSMeshTexturePaintTool::OnEndHover()
{
}



void UGSMeshTexturePaintTool::PreActiveTargetTextureUpdate(bool bInTransaction)
{
	if (bInTransaction) {
		if (bPaintLayerEnabled)
			CommitTemporaryPaintLayer(true);
	}
}

void UGSMeshTexturePaintTool::OnActiveTargetTextureUpdated()
{
	ImageUpdateTracker = MakeShared<GS::SubGridMask2>();
	ImageUpdateTracker->Initialize(PaintTextureWidth, PaintTextureHeight, 64);

	int MaterialIDFilter = GetActivePaintMaterialID();
	FVector2i ImageDimensions(PaintTextureWidth, PaintTextureHeight);
	SurfaceCache = MakeShared<FSurfaceTexelCache>();
	int RetCode = SurfaceCache->Initialize(PreviewMesh->GetMesh(), ImageDimensions, 0, MaterialIDFilter);
	if (RetCode < 0) {
		GetToolManager()->DisplayMessage(LOCTEXT("MissingUVs",
			"Could not build SurfaceCache for input mesh, so it is not paintable. Possibly no valid UVs?"), EToolMessageLevel::UserWarning);
		return;
	}

	ColorCache = MakeShared<GS::SurfaceTexelColorCache>();
	ColorCache->Initialize(SurfaceCache->Sampling.NumSamples(), [&](int PointID) {
		const auto& TexelInfo = SurfaceCache->Sampling[PointID];
		FColor ByteColor = TextureColorBuffer4b[TexelInfo.PixelPos.Y * ImageDimensions.X + TexelInfo.PixelPos.X];
		return (GS::Vector4f)ConvertToLinear(ByteColor);
	});

	if (SourcePaintTexture->Source.GetFormat() != ETextureSourceFormat::TSF_BGRA8)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("LossyMsg",
			"Only 8-bit RGBA Textures are fully supported. Painting on this Texture may lose precision"), EToolMessageLevel::UserWarning);
	}
}


void UGSMeshTexturePaintTool::PreTextureChangeOnUndoRedo()
{
	if (bInStroke)
		CancelActiveStroke();
}



void UGSMeshTexturePaintTool::AddStampFromWorldRay(const FVector2d& CursorPos, const FRay& WorldRay, double dt)
{
	FHitResult OutHit;
	bool bHit = PreviewMesh->FindRayIntersection(WorldRay, OutHit);
	if (!bHit)
		return;

	FVector3d LocalPoint, LocalNormal;
	FVector2i HitPixel;
	bool bOK = ComputeHitPixel(WorldRay, OutHit, LocalPoint, LocalNormal, LastHitTriangleID, LastHitUVPos, HitPixel);
	if (!bOK)
		return;

	// trying to give stamp a stable frame. Not sure that using camera vectors works...
	// it's view dependent so if you stamp near silhouette of a sphere (for example) and then
	// rotate and stamp in the same spot, you get different orientations.
	// Using world-up doesn't have this property, but it degenerates if LocalUp is parallel to FrameZ...
	// (should we incorporate previous frame??)
	FVector3d LocalViewRight = TargetTransform.InverseTransformVector(CameraState.Right());
	//FVector3d LocalUp = TargetTransform.InverseTransformVector(FVector::UnitZ());
	FFrame3d HitFrame(LocalPoint, LocalNormal);
	//HitFrame.ConstrainedAlignAxis(1, LocalUp, HitFrame.Z());
	HitFrame.ConstrainedAlignAxis(0, LocalViewRight, HitFrame.Z());

	IndicatorWorldFrame = FFrame3d(TargetTransform.TransformPosition(LocalPoint),
		TargetTransform.TransformNormal(LocalNormal));

	bool bApplyStamp = true;

	// apply stamp alignment
	if (bIsOrientedStroke) {
		if (PendingOrientedStrokeState >= 0)
		{
			if (PendingOrientedStrokeState == 0) {		// first stamp
				PendingInitialStamp = PendingStamp{ WorldRay, CursorPos, HitFrame, OutHit.FaceIndex, HitPixel, dt };
				bApplyStamp = false;
				PendingOrientedStrokeState = 1;
			}
			else if (PendingOrientedStrokeState > 0 && Distance(CursorPos,PendingInitialStamp.CursorPos) < 5.0 ) {
				bApplyStamp = false;		// wait for tangent to get more stable
				PendingOrientedStrokeState++;
			}
			else {
				FVector3d InitialTangent = HitFrame.Origin - PendingInitialStamp.LocalFrame.Origin;
				if (InitialTangent.Normalize())
					PendingInitialStamp.LocalFrame.AlignAxis(0, InitialTangent);
				AccumStrokeStamps.Add(PendingInitialStamp);
				PendingPixelStamps.Add(PendingInitialStamp);
				PendingOrientedStrokeState = -1;
			}
		}

		if (AccumStrokeStamps.Num() > 0) {
			int N = AccumStrokeStamps.Num();
			int NPrev = GS::Min(N, 1);
			//if (NPrev > 20)
			//	__debugbreak();
			FVector3d AccumTangent = FVector3d::Zero();
			for (int k = 1; k <= NPrev; ++k) {
				FVector3d Tangent = HitFrame.Origin - AccumStrokeStamps[N-k].LocalFrame.Origin;
				if (Tangent.Normalize()) {
					double Weight = 1.0; // / (double)k;
					AccumTangent += Weight * Tangent;
				}
			}
			if ( AccumTangent.Normalize() )
				HitFrame.ConstrainedAlignAxis(0, AccumTangent, HitFrame.Z());
			else
				bApplyStamp = false;
		}
	}


	if (LastStampTime > 0) {		// not first stamp. If we haven't moved far enough, skip this stamp

		if (CurrentToolMode == EGSTexturePaintToolMode::Brush) {
			double StepDist = Distance(LastHitFrame.Origin, HitFrame.Origin);
			AccumPathLength += StepDist;
			double StampSpacing = ((double)Settings->BrushToolSettings.Spacing / 100.0) * GetBrushSizeLocal();
			if (AccumPathLength < StampSpacing)
				bApplyStamp = false;
		}
	}

	TArray<PendingStamp>& UseStampQueue = (bIsOrientedStroke) ? OrientedStampQueue : PendingPixelStamps;

	LastHitFrame = HitFrame;
	LastHitPixel = HitPixel;
	if (bApplyStamp)
	{
		PendingStamp NewStamp{ WorldRay, CursorPos, HitFrame, OutHit.FaceIndex, HitPixel, dt };

		// if stamp moved too far, add additional in-between stamps
		int UseInterpSpacing = GS::Max(2, Settings->BrushToolSettings.Spacing);
		double StampSpacing = ((double)UseInterpSpacing / 100.0) * GetBrushSizeLocal();
		if (AccumStrokeStamps.Num() > 0) {

			const PendingStamp& LastStamp = AccumStrokeStamps.Last();
			double StampGap = Distance(NewStamp.LocalFrame.Origin, LastStamp.LocalFrame.Origin);
			if (StampGap > 1.9 * StampSpacing) {
				int N = GS::Min( (int)(StampGap / StampSpacing), 1000 );		
				for (int k = 1; k < N; ++k) {		// 1 is prev point, N is NewStamp

					double t = (double)k / (double)N;

					FVector2d InterpCursorPos = Lerp(LastStamp.CursorPos, NewStamp.CursorPos, t);
					FRay3d InterpRay = LastViewInfo.GetRayAtPixel(InterpCursorPos);

					bool bInterpHit = PreviewMesh->FindRayIntersection(InterpRay, OutHit);
					if (bInterpHit) {
						FVector3d InterpLocalPoint, InterpLocalNormal;
						FVector2i InterpHitPixel; FVector2f InterpUVPos; int InterpHitTriangleID;
						if (ComputeHitPixel(InterpRay, OutHit, InterpLocalPoint, InterpLocalNormal, InterpHitTriangleID, InterpUVPos, InterpHitPixel)) {

							// does this make sense? slerp the start/end rotations and then locally align to Z.
							// this could result in quite different orientations if the interp stroke crosses weird areas...
							FQuaterniond InterpRotation(LastStampFrame.Rotation, HitFrame.Rotation, t);
							FFrame3d InterpHitFrame(InterpLocalPoint, InterpRotation);
							InterpHitFrame.AlignAxis(2, InterpLocalNormal);

							UseStampQueue.Add(PendingStamp{ InterpRay, InterpCursorPos, InterpHitFrame, InterpHitTriangleID, InterpHitPixel, t * dt });
						}
					}
				}

			}
		}


		AccumStrokeStamps.Add(NewStamp);
		UseStampQueue.Add(NewStamp);
		LastStampFrame = HitFrame;
		AccumPathLength = 0;
	}
}



void UGSMeshTexturePaintTool::CompletePendingPaintOps()
{
	if (CurrentToolMode == EGSTexturePaintToolMode::Brush) {
		if (bIsOrientedStroke)
			PushPendingOrientedStrokesToQueue(false);
		ApplyPendingStamps_Brush();
	}
}


void UGSMeshTexturePaintTool::PushPendingOrientedStrokesToQueue(bool bIsEndStroke)
{
	ensure(bIsOrientedStroke);
	int N = OrientedStampQueue.Num();
	if (N == 0 || (N == 1 && bIsEndStroke == false) )
		return;

	for (int k = 0; k < N-1; ++k)
	{
		PendingStamp& Cur = OrientedStampQueue[k];
		const PendingStamp& Prev = (k == 0) ? LastAppliedStamp : OrientedStampQueue[k-1];
		const PendingStamp& Next = OrientedStampQueue[k + 1];
		FVector3d Tangent = Next.LocalFrame.Origin - Prev.LocalFrame.Origin;
		if (Tangent.Normalize())
			Cur.LocalFrame.ConstrainedAlignAxis(0, Tangent, Cur.LocalFrame.Z());
		PendingPixelStamps.Add(Cur);
	}
	if (bIsEndStroke) {
		PendingPixelStamps.Add(OrientedStampQueue[N-1]);
		OrientedStampQueue.Reset();
	} else {
		PendingStamp Tmp = OrientedStampQueue.Last();
		GSUE::TArraySetNum(OrientedStampQueue, 1, false);
		//OrientedStampQueue.SetNum(1, false);
		OrientedStampQueue[0] = Tmp;
	}
}

void UGSMeshTexturePaintTool::ApplyPendingStamps_Brush()
{
	if (PendingPixelStamps.Num() == 0)
		return;
	FLinearColor PaintColor = (GetShiftToggle() || GetCtrlToggle()) ? ColorSettings->EraseColor : ColorSettings->PaintColor;

	AxisBox2i AccumPixelRect = AxisBox2i::Empty();

	bool bImageModified = false;
	for (PendingStamp& Stamp : PendingPixelStamps)
	{
		double UseRadius = GetBrushSizeLocal()*0.5;

		// This is a quick-and-dirty hack to compute connected ROI. Needs to
		// be configurable. Could be computed at the same time as texelpoint query inside AppendStrokeStamp,
		// if the set was used to filter the ROI in a second step...
		StampStartROI.Reset();
		StampTempROIBuffer.Reset();
		StampTriangleROI.Reset();
		if (Settings->BrushToolSettings.AreaType == ETexturePaintBrushAreaType::Connected)
		{
			FRay3d LocalRay = TargetTransform.InverseTransformRay(Stamp.WorldRay);
			//double BrushROIRadiusSqr = (UseRadius*1.05f)*(UseRadius*1.05f);
			double BrushROIRadiusSqr = UseRadius * UseRadius;
			StampStartROI.Add(Stamp.TriangleID);
			const FDynamicMesh3* SurfaceMesh = PreviewMesh->GetMesh();
			FMeshConnectedComponents::GrowToConnectedTriangles(SurfaceMesh, StampStartROI, StampTriangleROI, &StampTempROIBuffer,
				[&](int t1, int t2)
			{
				FVector3d A, B, C;
				SurfaceMesh->GetTriVertices(t2, A, B, C);
				if (VectorUtil::NormalDirection(A, B, C).Dot(LocalRay.Direction) > 0)
					return false;

				//A = Stamp.LocalFrame.ToPlane(A);
				//B = Stamp.LocalFrame.ToPlane(B);
				//C = Stamp.LocalFrame.ToPlane(C);

				bool bInsideROI =
					(FSegment3d(A, B).DistanceSquared(Stamp.LocalFrame.Origin) < BrushROIRadiusSqr) ||
					(FSegment3d(B, C).DistanceSquared(Stamp.LocalFrame.Origin) < BrushROIRadiusSqr) ||
					(FSegment3d(C, A).DistanceSquared(Stamp.LocalFrame.Origin) < BrushROIRadiusSqr);

				if (!bInsideROI)
					return false;

				//return CheckEdgeCriteria(t1, t2);
				return true;
			});
		}
		const TSet<int32>* UseTriangleROIFilter = (StampTriangleROI.Num() > 0) ? &StampTriangleROI : nullptr;


		double UseFalloff = (double)Settings->BrushToolSettings.Falloff / 100.0;
		double StampPower = GS::Pow((double)Settings->BrushToolSettings.Flow / 100.0 + 0.05, 4.0);
		//StampPower *= (Stamp.dt * 5);
		//double StampPower = GS::Sqrt((double)Settings->Flow / 100.0) + 0.05;
		StampPower = GS::Clamp(StampPower, 0.0, 1.0);

		TSharedPtr<GS::SurfaceTexelColorCache>& UseColorCache = GetActiveColorCache();
		float UseStrokeAlpha = Settings->Alpha;

		auto WriteBufferFunc = [&](int TexelPointID, const Vector2i& PixelPos, const Vector4f& NewLinearColor)
		{
			FLinearColor SetColor = CalcCompositedLayerColor(TexelPointID, (FLinearColor)NewLinearColor, ActiveStrokeInfo->StokeAlpha);

			FColor NewFColor = ConvertToFColor((FLinearColor)SetColor);
			int ImageIndex = PixelPos.Y * PaintTextureWidth + PixelPos.X;
			if (TextureColorBuffer4b[ImageIndex] != NewFColor) {
				TextureColorBuffer4b[ImageIndex] = NewFColor;
				bImageModified = true;
			}

			AccumPixelRect.Contain(PixelPos);
			// WARNING CURRENTLY NOT THREAD-SAFE! should use atomics??
			ImageUpdateTracker->MarkCell(PixelPos, 0xFF);

		};

		FFrame3d StampFrame = Stamp.LocalFrame;
		if (Settings->Rotation != 0)
			StampFrame.Rotate(FQuaterniond(StampFrame.Z(), (double)Settings->Rotation, true));

		if (bHaveStampTexture)
		{
			Stamp_Image ImageStamp;
			ImageStamp.StrokeColor = (Vector4f)PaintColor;
			ImageStamp.bImageIsSRGB = bStampIsSRGB;
			ImageStamp.ImageBuffer = &StampImageBuffer4b;
			ImageStamp.ImageDims = StampImageDims;
			ImageStamp.Falloff = UseFalloff;

			ImageStamp.ColorMode = StampColorMode;
			ImageStamp.AlphaChannel = StampAlphaChannel;
			ImageStamp.bMultiplyWithBrushAlpha = !Settings->bIgnoreFalloff;

			auto StampFunc = [&ImageStamp](Vector3d SamplePos, const Frame3d& StampFrame, double StampRadius) {
				return ImageStamp.Evaluate(SamplePos, StampFrame, StampRadius); };

			ActiveStrokeInfo->AppendStrokeStamp_Textured((GS::Frame3d)StampFrame, UseRadius,
				StampFunc,
				//UseFalloff, StampPower, &StampImageBuffer4b, StampImageDims, bStampIsSRGB,
				UseStrokeAlpha, *UseColorCache,
				WriteBufferFunc, UseTriangleROIFilter);
		}
		else
		{
			ActiveStrokeInfo->AppendStrokeStamp_Radial((GS::Frame3d)StampFrame, UseRadius,
				UseFalloff, StampPower, (Vector4f)PaintColor,
				UseStrokeAlpha, *UseColorCache,
				WriteBufferFunc, UseTriangleROIFilter);
		}

		LastAppliedStamp = Stamp;
	}

	PendingPixelStamps.Reset();
	if (!bImageModified)
		return;

	const bool bPushFullImageUpdate = false;
	if (bPushFullImageUpdate)
	{
		PushFullImageUpdate();
	}
	else
	{
		// at some point it may become more economical to push entire image update? 
		// could enumerate regions first, check area sum, and compare w/ entire image?
		// (conceivably can also PushRegionImageUpdate in parallel...)

		int AccumChangeArea = 0, NumRegions = 0;
		AxisBox2i AccumChangeRect = AxisBox2i::Empty();
		ImageUpdateTracker->EnumerateMaskedRegions([&](const AxisBox2i& Region)
		{
			AccumChangeRect.Contain(Region);
			AccumChangeArea += Region.AreaCount();
			NumRegions++;
			PushRegionImageUpdate(Region.Min.X, Region.Min.Y, Region.CountX(), Region.CountY());
		} );
		ImageUpdateTracker->MarkAll(0);
		//UE_LOG(LogTemp, Warning, TEXT("PixelRect %dx%d  GridCellsRegion %dx%d (Area %d)   NumSubAreas %d SubAreaSum %d"),
		//	AccumPixelRect.CountX(), AccumPixelRect.CountY(),
		//	AccumChangeRect.CountX(), AccumChangeRect.CountY(),
		//	AccumChangeRect.AreaCount(), NumRegions, AccumChangeArea);
	}

}



FLinearColor UGSMeshTexturePaintTool::CalcCompositedLayerColor(int TexelPointID, FLinearColor StrokeColor, double LayerAlpha) const
{
	if (bPaintLayerEnabled)
	{
		//float LayerAlpha = (float)LayerSettings->LayerAlpha;
		Vector4f ImageColor = ColorCache->GetColor(TexelPointID);
		Vector4f NewBlendColor;
		GS::CombineColors4f_OverBlend(
			ImageColor.AsPointer(), &StrokeColor.R, LayerAlpha, NewBlendColor.AsPointer());
		
		// todo apply channel lerp...
		
		return (FLinearColor)NewBlendColor;
	}
	else
		return (FLinearColor)StrokeColor;
}


TSharedPtr<GS::SurfaceTexelColorCache>& UGSMeshTexturePaintTool::GetActiveColorCache()
{
	if (bPaintLayerEnabled)
		return LayerColorCache;
	else
		return ColorCache;
}






class FTexturePaint_TempLayerChange : public FToolCommandChange
{
public:
	bool bEnable = false;
	bool bCancel = false;
	bool bCommit = false;

	TArray<int> PointIDs;
	TArray<Vector4f> Colors;
	double LayerAlpha = 1.0;

	virtual void Apply(UObject* Object) override {
		if (UGSMeshTexturePaintTool* Tool = Cast<UGSMeshTexturePaintTool>(Object))
			Tool->UndoRedoLayerChange(*this, false);
	}
	virtual void Revert(UObject* Object) override {
		if (UGSMeshTexturePaintTool* Tool = Cast<UGSMeshTexturePaintTool>(Object))
			Tool->UndoRedoLayerChange(*this, true);
	}
	virtual FString ToString() const override { return TEXT("FTexturePaint_TempLayerChange"); }
};




void UGSMeshTexturePaintTool::EnableTemporaryPaintLayer(bool bEmitChange)
{
	if (bPaintLayerEnabled)
		return;

	LayerColorCache = MakeShared<GS::SurfaceTexelColorCache>();
	LayerColorCache->Initialize(SurfaceCache->Sampling.NumSamples(), [&](int PointID) {
		return GS::Vector4f(0, 0, 0, 0);
	});
	bPaintLayerEnabled = true;

	if (bEmitChange)
	{
		TUniquePtr<FTexturePaint_TempLayerChange> EnableLayerChange = MakeUnique<FTexturePaint_TempLayerChange>();
		EnableLayerChange->bEnable = true;
		EnableLayerChange->LayerAlpha = Settings->Alpha;
		GetToolManager()->EmitObjectChange(this, MoveTemp(EnableLayerChange), LOCTEXT("EnableTempLayerChange", "Enable Layer"));
	}
}

void UGSMeshTexturePaintTool::RebuildBufferFromColorCache(bool bPushFullUpdate)
{
	int N = SurfaceCache->Sampling.NumSamples();
	for (int k = 0; k < N; ++k)
	{
		Vector4f ImageColor = ColorCache->GetColor(k);
		FLinearColor NewColor = (FLinearColor)ImageColor;

		FColor NewFColor = ConvertToFColor(NewColor);
		FVector2i PixelPos = SurfaceCache->Sampling[k].PixelPos;
		int ImageIndex = PixelPos.Y * PaintTextureWidth + PixelPos.X;
		TextureColorBuffer4b[ImageIndex] = NewFColor;
	}

	if (bPushFullUpdate)
		PushFullImageUpdate();
}


void UGSMeshTexturePaintTool::RebuildBufferFromLayerColorCache(bool bPushFullUpdate, double LayerAlpha)
{
	ensure(bPaintLayerEnabled);

	int N = SurfaceCache->Sampling.NumSamples();
	for (int k = 0; k < N; ++k)
	{
		Vector4f LayerColor = LayerColorCache->GetColor(k);
		FLinearColor CombinedColor = CalcCompositedLayerColor(k, (FLinearColor)LayerColor, LayerAlpha);

		FColor NewFColor = ConvertToFColor(CombinedColor);
		FVector2i PixelPos = SurfaceCache->Sampling[k].PixelPos;
		int ImageIndex = PixelPos.Y * PaintTextureWidth + PixelPos.X;
		TextureColorBuffer4b[ImageIndex] = NewFColor;
	}

	if (bPushFullUpdate)
		PushFullImageUpdate();
}


static void SaveLayerColors(GS::SurfaceTexelColorCache& LayerColorCache, FTexturePaint_TempLayerChange& LayerChange)
{
	int N = (int)LayerColorCache.Colors.size();
	for (int k = 0; k < N; ++k)
	{
		Vector4f Color = LayerColorCache.GetColor(k);
		if (Color.W > 0) {
			LayerChange.PointIDs.Add(k);
			LayerChange.Colors.Add(Color);
		}
	}
}

void UGSMeshTexturePaintTool::CancelTemporaryPaintLayer(bool bEmitChange)
{
	if (!ensure(bPaintLayerEnabled))
		return;

	TUniquePtr<FTexturePaint_TempLayerChange> CancelLayerChange = MakeUnique<FTexturePaint_TempLayerChange>();
	CancelLayerChange->bCancel = true;
	CancelLayerChange->LayerAlpha = Settings->Alpha;

	if (bEmitChange)
	{
		// save layer colors
		// note: not necessary if layer has not been painted...
		SaveLayerColors(*LayerColorCache, *CancelLayerChange);
	}

	LayerColorCache.Reset();
	bPaintLayerEnabled = false;

	RebuildBufferFromColorCache(true);

	if (bEmitChange)
	{
		GetToolManager()->EmitObjectChange(this, MoveTemp(CancelLayerChange), LOCTEXT("DiscardTempLayerChange", "Discard Layer"));
	}

}

void UGSMeshTexturePaintTool::CommitTemporaryPaintLayer(bool bEmitChange)
{
	if (!ensure(bPaintLayerEnabled))
		return;

	double LayerAlpha = LayerSettings->LayerAlpha;

	TUniquePtr<FTexturePaint_TempLayerChange> CommitLayerChange = MakeUnique<FTexturePaint_TempLayerChange>();
	CommitLayerChange->bCommit = true;
	CommitLayerChange->LayerAlpha = LayerAlpha;

	// save layer colors
	// note: not necessary if layer has not been painted...but then we should also cancel instead of commit!
	if (bEmitChange) {
		SaveLayerColors(*LayerColorCache, *CommitLayerChange);
		if (CommitLayerChange->PointIDs.Num() == 0) {
			CancelTemporaryPaintLayer(true);
			return;
		}
	}
	int NumColors = CommitLayerChange->PointIDs.Num();

	TUniquePtr<FTexturePaint_BrushColorChange> BaseImageChange = MakeUnique<FTexturePaint_BrushColorChange>();
	BaseImageChange->ChangeList.Reserve(NumColors);
	BaseImageChange->ChangeAlpha = LayerAlpha;

	int N = SurfaceCache->Sampling.NumSamples();
	for (int k = 0; k < N; ++k)
	{
		Vector4f LayerColor = LayerColorCache->GetColor(k);
		if (LayerColor.W > 0) {
			Vector4f ImageColor = ColorCache->GetColor(k);
			Vector4f NewBlendColor;
			GS::CombineColors4f_OverBlend(
				ImageColor.AsPointer(), LayerColor.AsPointer(), (float)LayerAlpha, NewBlendColor.AsPointer());
			if (ImageColor.EpsilonEqual(NewBlendColor) == false) {
				if (bEmitChange)
					BaseImageChange->ChangeList.Add(FTexturePaint_BrushColorChange::ChangeInfo{ k, (FLinearColor)ImageColor, (FLinearColor)NewBlendColor });
				ColorCache->SetColor(k, NewBlendColor);
			}
		}
	}

	if (bEmitChange)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("CommitLayer", "Commit Layer"));
		GetToolManager()->EmitObjectChange(this, MoveTemp(CommitLayerChange), LOCTEXT("CommitLayer", "Commit Layer"));
		GetToolManager()->EmitObjectChange(this, MoveTemp(BaseImageChange), LOCTEXT("CommitLayer", "Commit Layer"));
		GetToolManager()->EndUndoTransaction();
	}

	LayerColorCache.Reset();
	bPaintLayerEnabled = false;
}


void UGSMeshTexturePaintTool::UndoRedoLayerChange(const FTexturePaint_TempLayerChange& Change, bool bUndo)
{
	auto RestoreLayer = [&]() {
		ensure(bPaintLayerEnabled);
		int N = Change.PointIDs.Num();
		for (int k = 0; k < N; ++k)
			LayerColorCache->SetColor(Change.PointIDs[k], Change.Colors[k]);
	};


	if (Change.bEnable) {
		if (bUndo) {
			// possibly just need to discard layer here as it should be empty...
			CancelTemporaryPaintLayer(false);
		} else {
			EnableTemporaryPaintLayer(false);
		}

	}
	else if (Change.bCancel) {
		if (bUndo) {
			EnableTemporaryPaintLayer(false);
			RestoreLayer();
			RebuildBufferFromLayerColorCache(true, Change.LayerAlpha);
		}
		else {
			CancelTemporaryPaintLayer(false);
			//RebuildBufferFromColorCache(true);    // CancelTemporaryPaintLayer already does this...
		}
	}
	else if (Change.bCommit) {
		if (bUndo) {
			EnableTemporaryPaintLayer(false);
			RestoreLayer();
			RebuildBufferFromLayerColorCache(true, Change.LayerAlpha);
		}
		else {
			CancelTemporaryPaintLayer(false);
		}
	}

}






void UGSMeshTexturePaintTool::UndoRedoBrushStroke(const FTexturePaint_BrushColorChange& Change, bool bUndo)
{
	if (bInStroke)
		CancelActiveStroke();

	TSharedPtr<GS::SurfaceTexelColorCache>& UpdateColorCache = GetActiveColorCache();
	if (Change.LayerIndex == 1)
		ensure(bPaintLayerEnabled);

	int N = Change.ChangeList.Num();
	for (int k = 0; k < N; ++k)
	{
		const auto& V = Change.ChangeList[k];
		FLinearColor NewColorLinear = (bUndo) ? V.InitialColor : V.FinalColor;
		UpdateColorCache->SetColor(V.PointID, (GS::Vector4f)NewColorLinear);

		FLinearColor NewColor = CalcCompositedLayerColor(V.PointID, NewColorLinear, Change.ChangeAlpha);

		FColor NewFColor = ConvertToFColor(NewColor);
		FVector2i PixelPos = SurfaceCache->Sampling[V.PointID].PixelPos;
		int ImageIndex = PixelPos.Y * PaintTextureWidth + PixelPos.X;
		TextureColorBuffer4b[ImageIndex] = NewFColor;
	}

	PushFullImageUpdate();
}



void FTexturePaint_BrushColorChange::Apply(UObject* Object)
{
	if (UGSMeshTexturePaintTool* Tool = Cast<UGSMeshTexturePaintTool>(Object))
		Tool->UndoRedoBrushStroke(*this, false);
}
void FTexturePaint_BrushColorChange::Revert(UObject* Object)
{
	if (UGSMeshTexturePaintTool* Tool = Cast<UGSMeshTexturePaintTool>(Object))
		Tool->UndoRedoBrushStroke(*this, true);
}




void UGSMeshTexturePaintTool::PickColorAtCursor(bool bSecondaryColor)
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


void UGSMeshTexturePaintTool::SetActiveColor(FLinearColor NewColor, bool bSetSecondary, bool bDisableChangeNotification)
{
	if (bSetSecondary)
		ColorSettings->EraseColor = NewColor;
	else
		ColorSettings->PaintColor = NewColor;
	if (bDisableChangeNotification)
		ColorSettings->SilentUpdateWatched();
	NotifyOfPropertyChangeByTool(ColorSettings);
}


void UGSMeshTexturePaintTool::OnStampTextureModified()
{
	UGSImageStamp* ImageStamp = (Settings->BrushStamp != nullptr) ? Settings->BrushStamp->GetImageStamp() : nullptr;
	UGSTextureImageStamp* TexStamp = Cast<UGSTextureImageStamp>(ImageStamp);
	UTexture2D* NewUseStampTexture = (TexStamp != nullptr) ? TexStamp->Texture : nullptr;

	if (NewUseStampTexture == nullptr) {
		bHaveStampTexture = false;
		StampImageDims = Vector2i(0, 0);
		StampImageBuffer4b.Reset();
		return;
	}

	UTexture2D* TempDupeTexture = nullptr;
	UTexture2D* PlatformDataTexture = NewUseStampTexture;
	const FTexturePlatformData* CurPlatformData = NewUseStampTexture->GetPlatformData();

	bool bIsUncompressed =
		(NewUseStampTexture->VirtualTextureStreaming == 0) &&
		(CurPlatformData->VTData == nullptr) &&
		(NewUseStampTexture->CompressionSettings == TextureCompressionSettings::TC_VectorDisplacementmap) &&
		(NewUseStampTexture->MipGenSettings == TextureMipGenSettings::TMGS_NoMipmaps);

	if (!bIsUncompressed) {
		TempDupeTexture = DuplicateObject<UTexture2D>(NewUseStampTexture, this);
		FTextureCompilingManager::Get().FinishCompilation({ TempDupeTexture });
		TempDupeTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
		TempDupeTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
		TempDupeTexture->VirtualTextureStreaming = 0;
		TempDupeTexture->PostEditChange();
		FTextureCompilingManager::Get().FinishCompilation({ TempDupeTexture });
		PlatformDataTexture = TempDupeTexture;
		CurPlatformData = TempDupeTexture->GetPlatformData();
	}

	auto SetToFailedImage = [this]() {
		StampImageBuffer4b.Reset();
		StampImageBuffer4b.Init(FColor::Red, 4);
		StampImageDims = FVector2i(2, 2);
		bStampIsSRGB = true;
		bHaveStampTexture = true;
	};

	if (CurPlatformData == nullptr || CurPlatformData->VTData != nullptr) {
		SetToFailedImage();
		return;
	}

	const FColor* FormattedImageData = reinterpret_cast<const FColor*>(CurPlatformData->Mips[0].BulkData.LockReadOnly());
	if (FormattedImageData == nullptr) {
		SetToFailedImage();
		return;
	}

	EPixelFormat UsingPixelFormat = CurPlatformData->GetLayerPixelFormat(0);
	StampImageDims = Vector2i(CurPlatformData->Mips[0].SizeX, CurPlatformData->Mips[0].SizeY);
	bStampIsSRGB = ActivePaintTexture->SRGB;

	const int64 Num = (int64)StampImageDims.X * (int64)StampImageDims.Y;
	StampImageBuffer4b.SetNum(Num);
	for (int64 i = 0; i < Num; ++i) 
		StampImageBuffer4b[i] = FormattedImageData[i];

	CurPlatformData->Mips[0].BulkData.Unlock();

	// get rid of temporary duplicate-texture that we don't need anymore
	if (TempDupeTexture != nullptr) {
		TempDupeTexture->MarkAsGarbage();		
	}

	StampColorMode = TexStamp->ColorMode;
	StampAlphaChannel = TexStamp->AlphaChannel;

	bHaveStampTexture = true;
}



#undef LOCTEXT_NAMESPACE
