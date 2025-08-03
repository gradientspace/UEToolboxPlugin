// Copyright Gradientspace Corp. All Rights Reserved.
#include "Utility/GSUEModelGridUtil.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Core/DynamicMeshGenericAPI.h"

#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridMesher.h"
#include "ModelGrid/ModelGridMeshCache.h"

//#define GSUE_FORCE_SINGLE_THREAD

using namespace GS;
using namespace UE::Geometry;

void GS::ExtractGridFullMesh(
	const GS::ModelGrid& Grid,
	UE::Geometry::FDynamicMesh3& ResultMesh,
	bool bEnableMaterials,
	bool bEnableUVs,
	FProgressCancel* Progress)
{
	GS::ExtractGridFullMesh(Grid, ResultMesh, bEnableMaterials, bEnableUVs, GS::SharedPtr<ICellMaterialToIndexMap>(), Progress);
}

void GS::ExtractGridFullMesh(
	const GS::ModelGrid& Grid,
	UE::Geometry::FDynamicMesh3& ResultMesh,
	bool bEnableMaterials,
	bool bEnableUVs,
	GS::SharedPtr<ICellMaterialToIndexMap> GridMaterialMap,
	FProgressCancel* Progress)
{
#ifdef GSUE_FORCE_SINGLE_THREAD
	static FCriticalSection TempLock;
	TempLock.Lock();
#endif

	ResultMesh.Clear();
	ResultMesh.EnableTriangleGroups();
	ResultMesh.EnableAttributes();
	ResultMesh.Attributes()->EnablePrimaryColors();
	if ( bEnableMaterials )
		ResultMesh.Attributes()->EnableMaterialID();
	if (bEnableUVs)
		ResultMesh.Attributes()->SetNumUVLayers(1);

	// assuming full grid has been modified...

	FDynamicMesh3BuilderFactory BuilderFactory;
	BuilderFactory.bEnableMaterials = bEnableMaterials;
	BuilderFactory.bEnableUVs = bEnableUVs;

	GS::ModelGridMeshCache MeshCache;
	MeshCache.Initialize(Grid.GetCellDimensions(), &BuilderFactory);
	if (GridMaterialMap)
		MeshCache.SetMaterialMap(GridMaterialMap);

	if (Progress && Progress->Cancelled()) return;

	AxisBox3i ModifiedCellBounds = Grid.GetModifiedRegionBounds(0);
	AxisBox3d UpdateBox = Grid.GetCellLocalBounds(ModifiedCellBounds.Min);
	UpdateBox.Contain(Grid.GetCellLocalBounds(ModifiedCellBounds.Max));
	MeshCache.UpdateInBounds(Grid, UpdateBox, [&](Vector2i Column) {});

	if (Progress && Progress->Cancelled()) return;

	// update mesh...
	FDynamicMesh3Collector Collector(&ResultMesh, bEnableMaterials, bEnableUVs);
	MeshCache.ExtractFullMesh(Collector);

	// would ideally enable this in debug...but #if DEBUG doesn't seem to work in UE?
	//ResultMesh.CheckValidity();

#ifdef GSUE_FORCE_SINGLE_THREAD
	TempLock.Unlock();
#endif
}
