// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/IntVector.h"
#include "UDynamicMesh.h"
#include "GridActor/UGSModelGrid.h"
#include "ModelGridScriptTypes.h"
#include "GradientspaceScriptTypes.h"
#include "ModelGridEditFunctions.generated.h"



UCLASS(meta = (ScriptName = "GSS_ModelGridEdits"), MinimalAPI)
class UGSScriptLibrary_ModelGridEdits : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Disable change notifications from being posted by the TargetGrid, until the next (job-subsystem) Tick.
	 * This avoids (eg) multiple meshing jobs from being launched, but also might result in a one-frame delay.
	 * UGSModelGrid::BeginGridEdits() / EndGridEdits() can be used for more precise control.
	 */
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, Keywords = "gss"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	SetDeferUpdatesUntilNextTick(UGSModelGrid* TargetGridInOut);


	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, KeyWords = "gss, clear, erase"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	RemoveAllCellsFromGrid(UGSModelGrid* TargetGridInOut);

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, Keywords="gss"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	SetGridCellDimensions(UGSModelGrid* TargetGridInOut, FVector Dimensions = FVector(50,50,50));

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, KeyWords = "gss, erase, clear"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	EraseGridCell(UGSModelGrid* TargetGridInOut, FIntVector CellIndex);

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, KeyWords = "gss, erase, clear"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	EraseGridCellsInRange(UGSModelGrid* TargetGridInOut, const FGSIntBox3& IndexRange);

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, KeyWords = "gss, set, fill"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	FillGridCell(UGSModelGrid* TargetGridInOut, FIntVector CellIndex,
		const FColor& CellColor = FColor::White);

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, DisplayName = "FillGridCellsInRange", KeyWords = "gss, set, fill"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	FillGridCellsInRangeV1(UGSModelGrid* TargetGridInOut, const FGSIntBox3& IndexRange,
		const FColor& CellColor = FColor::White, bool bOnlyFillEmpty = false);

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, DisplayName = "SetGridCellShape", KeyWords="gss, set, fill"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	SetGridCellShapeV1(UGSModelGrid* TargetGridInOut, FIntVector CellIndex,
		EModelGridBPCellType CellType = EModelGridBPCellType::Filled,
		const FModelGridCellTransformV1& CellTransform = FModelGridCellTransformV1(),
		const FColor& CellColor = FColor::White);


	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, DisplayName = "TranslateGridCellsInRange", KeyWords = "gss, translate"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	TranslateGridCellsInRangeV1(UGSModelGrid* TargetGridInOut, const FGSIntBox3& IndexRange, const FIntVector& Translation);


	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, DisplayName = "CopyGridCellsInRange", KeyWords = "gss, copy"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	CopyGridCellsInRangeV1(UGSModelGrid* TargetGridInOut, const FGSIntBox3& IndexRange, const FIntVector& Translation, bool bOnlyFillEmpty = false);
};