// Copyright Gradientspace Corp. All Rights Reserved.
#include "ModelGridNeighbourFunctions.h"
#include "GradientspaceScriptUtil.h"
#include "ModelGridScriptUtil.h"

#include "ModelGrid/ModelGridTypes.h"
#include "ModelGrid/ModelGridCell.h"
#include "ModelGrid/ModelGrid.h"


using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UGSScriptLibrary_ModelGridNeighbours"


FIntVector UGSScriptLibrary_ModelGridNeighbours::GetFaceNbrOffset(EModelGridBPAxisDirection AxisDirection)
{
	switch (AxisDirection) {
	case EModelGridBPAxisDirection::PlusZ:	return FIntVector(0, 0, 1);
	case EModelGridBPAxisDirection::PlusY:	return FIntVector(0, 1, 0);
	case EModelGridBPAxisDirection::PlusX:	return FIntVector(1, 0, 0);
	case EModelGridBPAxisDirection::MinusX:	return FIntVector(-1, 0, 0);
	case EModelGridBPAxisDirection::MinusY:	return FIntVector(0, -1, 0);
	case EModelGridBPAxisDirection::MinusZ:	return FIntVector(0, 0, -1);
	}
	return FIntVector(0, 0, 0);
}


FIntVector UGSScriptLibrary_ModelGridNeighbours::GetFaceNbrCellIndex(FIntVector CellIndex, EModelGridBPAxisDirection AxisDirection)
{
	return CellIndex + GetFaceNbrOffset(AxisDirection);
}

UGSModelGrid* UGSScriptLibrary_ModelGridNeighbours::GetGridCellFaceNbrsV1(UGSModelGrid* TargetGridInOut, FIntVector CellIndex, FMGCellFaceNbrsV1& Neighbours)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("GetGridCellFaceNbrs"));

	auto do_neighbour = [&](const ModelGrid& Grid, FIntVector Offset, EModelGridBPCellType& SetCellType) {
		bool bIsInGrid = false;
		GS::EModelGridCellType ModelGridCellType = Grid.GetCellType(CellIndex + Offset, bIsInGrid);
		SetCellType = GS::ConvertToBPCellType(ModelGridCellType);
	};

	TargetGridInOut->ProcessGrid([&](const ModelGrid& Grid) {
		do_neighbour(Grid, FIntVector(0,0,1), Neighbours.PlusZ);
		do_neighbour(Grid, FIntVector(0,0,-1), Neighbours.MinusZ);
		do_neighbour(Grid, FIntVector(1, 0, 0), Neighbours.PlusX);
		do_neighbour(Grid, FIntVector(-1, 0, 0), Neighbours.MinusX);
		do_neighbour(Grid, FIntVector(0, 1, 0), Neighbours.PlusY);
		do_neighbour(Grid, FIntVector(0, -1, 0), Neighbours.MinusY);
	});

	return TargetGridInOut;
}






FIntVector UGSScriptLibrary_ModelGridNeighbours::GetPlaneNbrOffset(EMGPlaneAxes PlaneAxes, EMGPlaneDirections PlaneDirection)
{
	int i0 = 0, i1 = 1;
	if (PlaneAxes == EMGPlaneAxes::PlaneYZ) { i0 = 1; i1 = 2; }
	else if (PlaneAxes == EMGPlaneAxes::PlaneXZ) { i0 = 0; i1 = 2; }

	const FIntVector2 Ordering[8] = {
		{-1,0}, {1,0}, {0,-1}, {0,1},
		{-1,-1}, {1,-1}, {1,1}, {-1,1} };

	FIntVector2 PlaneOffset = Ordering[(int)PlaneDirection];

	FIntVector Result(0, 0, 0);
	Result[i0] = PlaneOffset.X;
	Result[i1] = PlaneOffset.Y;
	return Result;
}

FIntVector UGSScriptLibrary_ModelGridNeighbours::GetPlaneNbrCellIndex(FIntVector CellIndex, EMGPlaneAxes PlaneAxes, EMGPlaneDirections PlaneDirection)
{
	return CellIndex + GetPlaneNbrOffset(PlaneAxes, PlaneDirection);
}


UGSModelGrid* UGSScriptLibrary_ModelGridNeighbours::GetGridCellPlaneNbrsV1(UGSModelGrid* TargetGridInOut, FIntVector CellIndex, EMGPlaneAxes PlaneAxes, FMGCellPlaneNbrsV1& Neighbours)
{
	CHECK_GRID_VALID_OR_RETURN(TargetGridInOut, TEXT("GetGridCellPlaneNbrsV1"));

	Neighbours.CellIndex = CellIndex;
	Neighbours.PlaneAxes = PlaneAxes;

	int i0 = 0, i1 = 1;
	if (PlaneAxes == EMGPlaneAxes::PlaneYZ) { i0 = 1; i1 = 2; }
	else if (PlaneAxes == EMGPlaneAxes::PlaneXZ) { i0 = 0; i1 = 2; }

	const FIntVector2 Ordering[8] = {
		{-1,0}, {1,0}, {0,-1}, {0,1},
		{-1,-1}, {1,-1}, {1,1}, {-1,1} };

	EModelGridBPCellType* SetNeighbours[8] = { &Neighbours.MinusU, &Neighbours.PlusU, &Neighbours.MinusV, &Neighbours.PlusV,
		&Neighbours.MinusUMinusV, &Neighbours.PlusUMinusV, &Neighbours.PlusUPlusV, &Neighbours.MinusUPlusV };

	TargetGridInOut->ProcessGrid([&](const ModelGrid& Grid) {
		for (int j = 0; j < 8; ++j) {
			FIntVector Point(0, 0, 0);
			Point[i0] = Ordering[j].X;
			Point[i1] = Ordering[j].Y;

			bool bIsInGrid = false;
			GS::EModelGridCellType ModelGridCellType = Grid.GetCellType(CellIndex + Point, bIsInGrid);
			*(SetNeighbours[j]) = GS::ConvertToBPCellType(ModelGridCellType);
		}
	});

	return TargetGridInOut;
}


int UGSScriptLibrary_ModelGridNeighbours::CountPlaneNbrsOfType(const FMGCellPlaneNbrsV1& Neighbours, EModelGridBPCellType CellType, bool bInvert)
{
	int Count = 0;
	EModelGridBPCellType NeighboursArr[8] = { Neighbours.MinusU, Neighbours.PlusU, Neighbours.MinusV, Neighbours.PlusV,
		Neighbours.MinusUMinusV, Neighbours.PlusUMinusV, Neighbours.PlusUPlusV, Neighbours.MinusUPlusV };
	for (int j = 0; j < 8; ++j) {
		bool bEqual = (NeighboursArr[j] == CellType);
		if (bInvert) bEqual = !bEqual;
		if (bEqual) Count++;
	}
	return Count;
}



#undef LOCTEXT_NAMESPACE

