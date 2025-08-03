// Copyright Gradientspace Corp. All Rights Reserved.
#include "Utility/GSUEBakeUtil.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

#include "Sampling/MeshBakerCommon.h"
#include "Sampling/MeshMapBaker.h"
#include "Sampling/MeshVertexBaker.h"
#include "Sampling/MeshPropertyMapEvaluator.h"

#include "Image/ImageBuilder.h"

using namespace UE::Geometry;


void GS::BakeMeshVertexColorsToImage(
	const FDynamicMesh3& Mesh,
	int ImageResolution,
	double ProjectionDistance,
	int SamplesPerPixel,
	bool bLinearFiltering,
	TImageBuilder<FVector4f>& ResultImageOut,
	FDynamicMeshAABBTree3* MeshSpatial,
	int UseUVLayer)
{
	const FDynamicMesh3* UseMesh = &Mesh;
	FDynamicMeshAABBTree3 DetailSpatial;
	if (MeshSpatial == nullptr) {
		DetailSpatial.SetMesh(UseMesh, true);
		MeshSpatial = &DetailSpatial;
	}
	
	FMeshBakerDynamicMeshSampler DetailSampler(UseMesh, MeshSpatial);

	FMeshMapBaker Baker;
	Baker.SetTargetMesh(UseMesh);
	Baker.SetTargetMeshUVLayer(UseUVLayer);
	Baker.SetDetailSampler(&DetailSampler);
	FImageDimensions BakeDimensions(ImageResolution, ImageResolution);
	Baker.SetDimensions(BakeDimensions);
	Baker.SetProjectionDistance(ProjectionDistance);
	Baker.SetSamplesPerPixel(SamplesPerPixel);
	Baker.SetFilter( (bLinearFiltering) ? FMeshMapBaker::EBakeFilterType::Box : FMeshMapBaker::EBakeFilterType::None );
	Baker.SetCorrespondenceStrategy(FMeshBaseBaker::ECorrespondenceStrategy::Identity);

	TSharedPtr<FMeshPropertyMapEvaluator> PropertyEval = MakeShared<FMeshPropertyMapEvaluator>();
	PropertyEval->Property = EMeshPropertyMapType::VertexColor;

	Baker.AddEvaluator(PropertyEval);

	Baker.Bake();

	ResultImageOut = *Baker.GetBakeResults(0)[0];
}