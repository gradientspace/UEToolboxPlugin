// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/IntVector.h"
#include "GridActor/UGSModelGrid.h"
#include "GradientspaceScriptTypes.h"
#include "ModelGridScriptTypes.h"
#include "ModelGridNeighbourFunctions.generated.h"

UCLASS(meta = (ScriptName = "GSS_ModelGridNeighbours"), MinimalAPI)
class UGSScriptLibrary_ModelGridNeighbours : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintPure, Category = "Gradientspace|ModelGrid", meta = (Keywords = "gss, neighbours, nbrs"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "OffsetXYZ") FIntVector
	GetFaceNbrOffset(EModelGridBPAxisDirection AxisDirection = EModelGridBPAxisDirection::PlusZ);

	UFUNCTION(BlueprintPure, Category = "Gradientspace|ModelGrid", meta = (Keywords = "gss, neighbours, nbrs"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "CellIndex") FIntVector
	GetFaceNbrCellIndex(FIntVector CellIndex, EModelGridBPAxisDirection AxisDirection = EModelGridBPAxisDirection::PlusZ);

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, DisplayName = "GetGridCellFaceNbrs", Keywords = "gss, neighbours, nbrs"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	GetGridCellFaceNbrsV1(UGSModelGrid* TargetGridInOut, FIntVector CellIndex, FMGCellFaceNbrsV1& Neighbours);


	UFUNCTION(BlueprintPure, Category = "Gradientspace|ModelGrid", meta = (Keywords = "gss, neighbours, nbrs"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "OffsetXYZ")FIntVector
	GetPlaneNbrOffset(EMGPlaneAxes PlaneAxes = EMGPlaneAxes::PlaneXY, EMGPlaneDirections PlaneDirection = EMGPlaneDirections::PlusU);

	UFUNCTION(BlueprintPure, Category = "Gradientspace|ModelGrid", meta = (Keywords = "gss, neighbours, nbrs"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "CellIndex") FIntVector
	GetPlaneNbrCellIndex(FIntVector CellIndex, EMGPlaneAxes PlaneAxes = EMGPlaneAxes::PlaneXY, EMGPlaneDirections PlaneDirection = EMGPlaneDirections::PlusU);

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, DisplayName = "GetGridCellPlaneNbrs", PlaneAxes="PlaneXY", Keywords = "gss, neighbours, nbrs"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	GetGridCellPlaneNbrsV1(UGSModelGrid* TargetGridInOut, FIntVector CellIndex, EMGPlaneAxes PlaneAxes, FMGCellPlaneNbrsV1& Neighbours);


	UFUNCTION(BlueprintPure, Category = "Gradientspace|ModelGrid", meta = (Keywords="gss"))
	static GRADIENTSPACESCRIPT_API int CountPlaneNbrsOfType(const FMGCellPlaneNbrsV1& Neighbours, EModelGridBPCellType CellType, bool bInvert);

};