// Copyright Gradientspace Corp. All Rights Reserved.
#include "ModelGridEditFunctions.h"
#include "GradientspaceScriptUtil.h"

#include "ModelGrid/ModelGridTypes.h"
#include "ModelGrid/ModelGridCell.h"
#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridEditor.h"
#include "Grid/GSGridUtil.h"

#include "GSJobSubsystem.h"

using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UGSScriptLibrary_ModelGridEdits"


UGSModelGrid* UGSScriptLibrary_ModelGridEdits::SetDeferUpdatesUntilNextTick(UGSModelGrid* TargetGridInOut)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("RemoveAllCellsFromGrid"));

	UGSJobSubsystem::EnqueueGameThreadTickJob(TargetGridInOut, TargetGridInOut,
		[TargetGridInOut]() {
			if (IsValid(TargetGridInOut))
				TargetGridInOut->EndAllPendingGridEdits();
		});
	TargetGridInOut->BeginGridEdits();

	return TargetGridInOut;
}


UGSModelGrid* UGSScriptLibrary_ModelGridEdits::RemoveAllCellsFromGrid(UGSModelGrid* TargetGridInOut)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("RemoveAllCellsFromGrid"));

	TargetGridInOut->EditGrid([&](ModelGrid& Grid) {
		ModelGrid EmptyGrid;
		EmptyGrid.Initialize(Grid.GetCellDimensions());
		Grid = MoveTemp(EmptyGrid);
	});
	return TargetGridInOut;
}


UGSModelGrid* UGSScriptLibrary_ModelGridEdits::SetGridCellDimensions(UGSModelGrid* TargetGridInOut, FVector Dimensions)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("SetGridCellDimensions"));

	Dimensions.X = (double)GS::Clamp((float)Dimensions.X, GS::Mathf::ZeroTolerance(), GS::Mathf::SafeMaxExtent());
	Dimensions.Y = (double)GS::Clamp((float)Dimensions.Y, GS::Mathf::ZeroTolerance(), GS::Mathf::SafeMaxExtent());
	Dimensions.Z = (double)GS::Clamp((float)Dimensions.Z, GS::Mathf::ZeroTolerance(), GS::Mathf::SafeMaxExtent());
	TargetGridInOut->EditGrid([&](ModelGrid& Grid) {
		Grid.SetNewCellDimensions((GS::Vector3d)Dimensions);
	});
	return TargetGridInOut;
}


UGSModelGrid* UGSScriptLibrary_ModelGridEdits::EraseGridCell(UGSModelGrid* TargetGridInOut, FIntVector CellIndex)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("EraseGridCell"));

	TargetGridInOut->EditGrid([&](ModelGrid& Grid) {
		ModelGridEditor Editor(Grid);
		Editor.EraseCell(CellIndex);
	});
	return TargetGridInOut;
}


UGSModelGrid* UGSScriptLibrary_ModelGridEdits::EraseGridCellsInRange(UGSModelGrid* TargetGridInOut, const FGSIntBox3& IndexRange)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("EraseGridCellsInRange"));

	TargetGridInOut->EditGrid([&](ModelGrid& Grid) {
		ModelGridEditor Editor(Grid);
		GS::EnumerateCellsInRangeInclusive(IndexRange.Min, IndexRange.Max, [&](Vector3i CellIndex) {
			Editor.EraseCell(CellIndex);
		});
	});
	return TargetGridInOut;
}


UGSModelGrid* UGSScriptLibrary_ModelGridEdits::FillGridCell(UGSModelGrid* TargetGridInOut, FIntVector CellIndex,
	const FColor& CellColor)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("FillGridCell"));

	ModelGridCell NewCell = MakeDefaultCellFromType(EModelGridCellType::Filled);
	NewCell.SetToSolidColor(GS::Color3b(CellColor));
	TargetGridInOut->EditGrid([&](ModelGrid& Grid) {
		ModelGridEditor Editor(Grid);
		Editor.UpdateCell(CellIndex, NewCell);
	});
	return TargetGridInOut;
}


UGSModelGrid* UGSScriptLibrary_ModelGridEdits::FillGridCellsInRangeV1(UGSModelGrid* TargetGridInOut, const FGSIntBox3& IndexRange,
	const FColor& CellColor, bool bOnlyFillEmpty)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("FillGridCellsInRange"));

	ModelGridCell NewCell = MakeDefaultCellFromType(EModelGridCellType::Filled);
	NewCell.SetToSolidColor(GS::Color3b(CellColor));
	TargetGridInOut->EditGrid([&](ModelGrid& Grid) {
		ModelGridEditor Editor(Grid);
		GS::EnumerateCellsInRangeInclusive(IndexRange.Min, IndexRange.Max, [&](Vector3i CellIndex) {
			bool bIsInGrid = false;
			EModelGridCellType CellType = Grid.GetCellType(CellIndex, bIsInGrid);
			if ( (!bOnlyFillEmpty) || CellType == EModelGridCellType::Empty)
				Editor.UpdateCell(CellIndex, NewCell);
		});
	});
	return TargetGridInOut;
}


UGSModelGrid* UGSScriptLibrary_ModelGridEdits::SetGridCellShapeV1(UGSModelGrid* TargetGridInOut, FIntVector CellIndex,
	EModelGridBPCellType CellType,
	const FModelGridCellTransformV1& CellTransform,
	const FColor& CellColor)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("SetGridCellShapeV1"));

	ModelGridCell NewCell = MakeDefaultCellFromType((EModelGridCellType)(int)CellType);
	if (ModelGridCellData_StandardRST::IsSubType(NewCell.CellType))
	{
		ModelGridCellData_StandardRST Transform;
		Transform.Params.Fields = NewCell.CellData;
		Transform.Params.AxisDirection = (int)CellTransform.Orientation.AxisDirection;
		Transform.Params.AxisRotation = (int)CellTransform.Orientation.AxisRotation;
		Transform.Params.DimensionMode = (CellTransform.DimensionType == EModelGridBPDimensionType::Thirds) ? 1 : 0;
		int MaxTranslate = (Transform.Params.DimensionMode == 0) ? (int)ModelGridCellData_StandardRST::MaxTranslate : (int)ModelGridCellData_StandardRST::MaxTranslate_Thirds;
		Transform.Params.TranslateX = (uint8_t)GS::Clamp(CellTransform.Translation.X, 0, MaxTranslate);
		Transform.Params.TranslateY = (uint8_t)GS::Clamp(CellTransform.Translation.Y, 0, MaxTranslate);
		Transform.Params.TranslateZ = (uint8_t)GS::Clamp(CellTransform.Translation.Z, 0, MaxTranslate);
		Transform.Params.DimensionX = (uint8_t)GS::Clamp(CellTransform.Dimensions.X, 0, (int)ModelGridCellData_StandardRST::MaxDimension);
		Transform.Params.DimensionY = (uint8_t)GS::Clamp(CellTransform.Dimensions.Y, 0, (int)ModelGridCellData_StandardRST::MaxDimension);
		Transform.Params.DimensionZ = (uint8_t)GS::Clamp(CellTransform.Dimensions.Z, 0, (int)ModelGridCellData_StandardRST::MaxDimension);
		NewCell.CellData = Transform.Params.Fields;
	}

	NewCell.SetToSolidColor(GS::Color3b(CellColor));

	TargetGridInOut->EditGrid([&](ModelGrid& Grid) {
		ModelGridEditor Editor(Grid);
		Editor.UpdateCell(CellIndex, NewCell);
	});
	return TargetGridInOut;
}


namespace GS {

static void CopyGridCellsInRange(ModelGrid& Grid, GS::AxisBox3i IndexRange, GS::Vector3i Translation, bool bErasePrevious, bool bReplaceAtTarget)
{
	ModelGridEditor Editor(Grid);

	// todo: this could be smarter...can copy-in-place if we know positive/negative directions

	int N = (int)IndexRange.VolumeCount();
	TArray<ModelGridCell> Cells;
	Cells.SetNum(N);
	int Counter = 0;
	GS::EnumerateCellsInRangeInclusive(IndexRange.Min, IndexRange.Max, [&](Vector3i CellIndex) {
		bool bIsInGrid = false;
		Cells[Counter++] = Grid.GetCellInfo(CellIndex, bIsInGrid);
		if (bErasePrevious)
			Editor.EraseCell(CellIndex);
	});

	Counter = 0;
	GS::EnumerateCellsInRangeInclusive(IndexRange.Min, IndexRange.Max, [&](Vector3i CellIndex) {
		const ModelGridCell& SetCell = Cells[Counter++];
		Vector3i SetCellIndex = CellIndex + Translation;
		bool bIsInGrid = false;
		EModelGridCellType CellType = Grid.GetCellType(SetCellIndex, bIsInGrid);
		if (bReplaceAtTarget || CellType == EModelGridCellType::Empty)
			Editor.UpdateCell(SetCellIndex, SetCell);
	});

}

}


UGSModelGrid* UGSScriptLibrary_ModelGridEdits::TranslateGridCellsInRangeV1(UGSModelGrid* TargetGridInOut, const FGSIntBox3& IndexRange, const FIntVector& Translation)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("TranslateGridCellsInRangeV1"));
	if (Translation.IsZero()) return TargetGridInOut;

	// todo: this could be smarter...can copy-in-place if we know positive/negative directions

	TargetGridInOut->EditGrid([&](ModelGrid& Grid) {
		GS::AxisBox3i IntRange(IndexRange.Min, IndexRange.Max);
		GS::CopyGridCellsInRange(Grid, IntRange, Translation, true, true);
	});
	return TargetGridInOut;
}

UGSModelGrid* UGSScriptLibrary_ModelGridEdits::CopyGridCellsInRangeV1(UGSModelGrid* TargetGridInOut, const FGSIntBox3& IndexRange, const FIntVector& Translation, bool bOnlyFillEmpty)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("CopyGridCellsInRangeV1"));
	if (Translation.IsZero()) return TargetGridInOut;

	TargetGridInOut->EditGrid([&](ModelGrid& Grid) {
		GS::AxisBox3i IntRange(IndexRange.Min, IndexRange.Max);
		GS::CopyGridCellsInRange(Grid, IntRange, Translation, false, !bOnlyFillEmpty);
	});
	return TargetGridInOut;
}


#undef LOCTEXT_NAMESPACE