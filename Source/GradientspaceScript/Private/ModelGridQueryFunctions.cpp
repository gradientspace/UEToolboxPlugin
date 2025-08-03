// Copyright Gradientspace Corp. All Rights Reserved.
#include "ModelGridQueryFunctions.h"
#include "GradientspaceScriptUtil.h"
#include "ModelGridScriptUtil.h"

#include "ModelGrid/ModelGridTypes.h"
#include "ModelGrid/ModelGridCell.h"
#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridEditor.h"

using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UGSScriptLibrary_ModelGridQueries"

UGSModelGrid* UGSScriptLibrary_ModelGridQueries::GetGridCellDimensions(UGSModelGrid* TargetGridInOut, 
	FVector& CellDimensions)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("GetGridCellDimensions"));
	TargetGridInOut->ProcessGrid([&](const ModelGrid& Grid) {
		CellDimensions = Grid.GetCellDimensions();
	});
	return TargetGridInOut;
}

UGSModelGrid* UGSScriptLibrary_ModelGridQueries::GetGridCellBounds(UGSModelGrid* TargetGridInOut,
	FIntVector CellIndex, FBox& LocalBounds)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("GetGridCellBounds"));
	TargetGridInOut->ProcessGrid([&](const ModelGrid& Grid) {
		LocalBounds = (FBox)Grid.GetCellLocalBounds(CellIndex);
	});
	return TargetGridInOut;
}

UGSModelGrid* UGSScriptLibrary_ModelGridQueries::GetGridCellType(UGSModelGrid* TargetGridInOut,
	FIntVector CellIndex, EGSGridCellState& State, EModelGridBPCellType& Type)
{
	State = EGSGridCellState::NotInGrid;
	Type = EModelGridBPCellType::Empty;
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("GetGridCellType"));
	TargetGridInOut->ProcessGrid([&](const ModelGrid& Grid) {
		bool bIsInGrid = false;
		EModelGridCellType CellType = Grid.GetCellType(CellIndex, bIsInGrid);
		if (bIsInGrid) {
			State = (CellType == EModelGridCellType::Empty) ? EGSGridCellState::Empty : EGSGridCellState::NonEmpty;
			Type = ((int)CellType <= (int)EModelGridBPCellType::LastKnownType) ? (EModelGridBPCellType)(int)CellType : EModelGridBPCellType::Unknown;
		}
	});
	return TargetGridInOut;
}



UGSModelGrid* UGSScriptLibrary_ModelGridQueries::GetGridCellShapeV1(UGSModelGrid* TargetGridInOut, FIntVector CellIndex,
	EModelGridBPCellType& CellType, FModelGridCellTransformV1& BPCellTransform, FModelGridBPCellMaterialV1& BPCellMaterial,
	EGSInGridOutcome& IsInGrid)
{
	IsInGrid = EGSInGridOutcome::NotInGrid;
	CellType = EModelGridBPCellType::Empty;
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("GetGridCellShapeV1"));
	TargetGridInOut->ProcessGrid([&](const ModelGrid& Grid) {
		bool bIsInGrid = false;
		ModelGridCell Cell = Grid.GetCellInfo((GS::Vector3i)CellIndex, bIsInGrid);
		if (!bIsInGrid) return;
		IsInGrid = EGSInGridOutcome::InGrid;
		CellType = GS::ConvertToBPCellType(Cell.CellType);

		if (ModelGridCellData_StandardRST::IsSubType(Cell.CellType))
		{
			ModelGridCellData_StandardRST Transform;
			Transform.Params.Fields = Cell.CellData;

			BPCellTransform.Orientation.AxisDirection = (EModelGridBPAxisDirection)Transform.Params.AxisDirection;
			BPCellTransform.Orientation.AxisRotation = (EModelGridBPAxisRotation)Transform.Params.AxisRotation;
			BPCellTransform.DimensionType = (EModelGridBPDimensionType)Transform.Params.DimensionMode;
			BPCellTransform.Dimensions.X = Transform.Params.DimensionX;
			BPCellTransform.Dimensions.Y = Transform.Params.DimensionY;
			BPCellTransform.Dimensions.Z = Transform.Params.DimensionZ;
			BPCellTransform.Translation.X = Transform.Params.TranslateX;
			BPCellTransform.Translation.Y = Transform.Params.TranslateY;
			BPCellTransform.Translation.Z = Transform.Params.TranslateZ;
		}


		BPCellMaterial.MaterialType = (EModelGridBPCellMaterialType)(int)Cell.MaterialType;
		if (Cell.MaterialType == GS::EGridCellMaterialType::SolidColor) {
			BPCellMaterial.SolidColor = (FColor)Cell.CellMaterial.AsColor4b();
		}
		else if (Cell.MaterialType == GS::EGridCellMaterialType::SolidRGBIndex) {
			BPCellMaterial.SolidColor = (FColor)Cell.CellMaterial.AsColor3b();
			BPCellMaterial.Index = Cell.CellMaterial.GetIndex8();
		} else if (Cell.MaterialType == GS::EGridCellMaterialType::FaceColors) {
			GS_LOG_SCRIPT_ERROR(TEXT("GetGridCellV1"), TEXT("FaceColors Material Type is not yet supported"));
		}
	});
	return TargetGridInOut;
}



UGSModelGrid* UGSScriptLibrary_ModelGridQueries::GetGridOccupiedCellRange(UGSModelGrid* TargetGridInOut, FGSIntBox3& CellIndexRange, FBox& LocalBounds, int PadExtents)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("GetGridOccupiedCellRange"));
	TargetGridInOut->ProcessGrid([&](const ModelGrid& Grid) {
		GS::AxisBox3i IndexRange = Grid.GetOccupiedRegionBounds(PadExtents);
		CellIndexRange.Min = (FIntVector)IndexRange.Min; CellIndexRange.Max = (FIntVector)IndexRange.Max;
		LocalBounds.Min = Grid.GetCellLocalBounds(IndexRange.Min).Min;
		LocalBounds.Max = Grid.GetCellLocalBounds(IndexRange.Max).Max;
	});
	return TargetGridInOut;
}


UGSModelGrid* UGSScriptLibrary_ModelGridQueries::GetGridCellIndexAtPoint(UGSModelGrid* TargetGridInOut,
	FVector LocalPosition, FIntVector& CellIndex, EGSInGridOutcome& IsInGrid)
{
	IsInGrid = EGSInGridOutcome::NotInGrid;
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("GetGridCellIndexAtPoint"));

	TargetGridInOut->ProcessGrid([&](const ModelGrid& Grid) {
		bool bIsInGrid = false;
		CellIndex = Grid.GetCellAtPosition((GS::Vector3d)LocalPosition, bIsInGrid);
		IsInGrid = (bIsInGrid) ? EGSInGridOutcome::InGrid : EGSInGridOutcome::NotInGrid;
	});
	return TargetGridInOut;
}



#undef LOCTEXT_NAMESPACE