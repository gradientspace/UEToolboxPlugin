// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/IntVector.h"
#include "GridActor/UGSModelGrid.h"
#include "GradientspaceScriptTypes.h"
#include "ModelGridScriptTypes.h"
#include "ModelGridQueryFunctions.generated.h"

UENUM(BlueprintType)
enum class EGSInGridOutcome : uint8
{
	NotInGrid = 0,
	InGrid = 1
};

UENUM(BlueprintType)
enum class EGSGridCellState : uint8
{
	NotInGrid = 0,
	Empty = 1,
	NonEmpty = 2
};



UCLASS(meta = (ScriptName = "GSS_ModelGridQueries"), MinimalAPI)
class UGSScriptLibrary_ModelGridQueries : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, Keywords = "gss, dimensions"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	GetGridCellDimensions(UGSModelGrid* TargetGridInOut, FVector& CellDimensions);

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, Keywords = "gss, cell"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	GetGridOccupiedCellRange(UGSModelGrid* TargetGridInOut, FGSIntBox3& CellIndexRange, FBox& LocalBounds, int PadExtents = 0);

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, Keywords = "gss, cell, bounds"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	GetGridCellBounds(UGSModelGrid* TargetGridInOut, FIntVector CellIndex, FBox& LocalBounds);


	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, Keywords = "gss, dimensions", ExpandEnumAsExecs = "State"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	GetGridCellType(UGSModelGrid* TargetGridInOut, FIntVector CellIndex, EGSGridCellState& State, EModelGridBPCellType& Type);

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, DisplayName="GetGridCellShape", KeyWords = "gss, get", ExpandEnumAsExecs = "IsInGrid"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	GetGridCellShapeV1(UGSModelGrid* TargetGridInOut, FIntVector CellIndex, 
		EModelGridBPCellType& CellType, FModelGridCellTransformV1& Transform, FModelGridBPCellMaterialV1& CellMaterial,
		EGSInGridOutcome& IsInGrid);


	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, KeyWords = "gss, find, cell", ExpandEnumAsExecs = "IsInGrid"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	GetGridCellIndexAtPoint(UGSModelGrid* TargetGridInOut, FVector LocalPosition, FIntVector& CellIndex, EGSInGridOutcome& IsInGrid);


};