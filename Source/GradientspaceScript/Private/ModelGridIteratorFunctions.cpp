// Copyright Gradientspace Corp. All Rights Reserved.
#include "ModelGridIteratorFunctions.h"
#include "GradientspaceScriptUtil.h"

#include "ModelGrid/ModelGridTypes.h"
#include "ModelGrid/ModelGridCell.h"
#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridEditor.h"

using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UGSScriptLibrary_ModelGridIterators"


UGSModelGrid* UGSScriptLibrary_ModelGridIterators::ForEachCellInRangeV1(
	UGSModelGrid* TargetGridInOut,
	FGSIntBox3 CellIndexRange, 
	const FProcessGridCellDelegate& ProcessCellFunc)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("ForEachCellInRangeV1"));

	// maybe we should cache this in UGSModelGrid...
	AxisBox3i ValidCellIndexRange = AxisBox3i::Empty();
	TargetGridInOut->ProcessGrid([&](const ModelGrid& Grid) {
		ValidCellIndexRange = Grid.GetCellIndexRange();
	});

	auto PerCellFunc = [&](GS::Vector3i CellIndex) {
		ProcessCellFunc.ExecuteIfBound(TargetGridInOut, CellIndex);
	};
	Vector3i MinIndex = GS::Max( (Vector3i)CellIndexRange.Min, ValidCellIndexRange.Min );
	Vector3i MaxIndex = GS::Min( (Vector3i)CellIndexRange.Max, ValidCellIndexRange.Max );
	for (int zi = MinIndex.Z; zi <= MaxIndex.Z; ++zi) {
		for (int yi = MinIndex.Y; yi <= MaxIndex.Y; ++yi) {
			for (int xi = MinIndex.X; xi <= MaxIndex.X; ++xi) {
				PerCellFunc(Vector3i(xi, yi, zi));
			}
		}
	}
	return TargetGridInOut;
}






#undef LOCTEXT_NAMESPACE