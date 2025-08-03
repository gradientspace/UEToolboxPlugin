// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/IntVector.h"
#include "GridActor/UGSModelGrid.h"
#include "GradientspaceScriptTypes.h"
#include "ModelGridScriptTypes.h"
#include "ModelGridIteratorFunctions.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
DECLARE_DYNAMIC_DELEGATE_TwoParams(FProcessGridCellDelegate, UGSModelGrid*, Grid, FIntVector, CellIndex);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

UCLASS(meta = (ScriptName = "GSS_ModelGridIterators"), MinimalAPI)
class UGSScriptLibrary_ModelGridIterators : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:


	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta=(ScriptMethod, DisplayName = "ForEachCellInRange", Keywords = "gss, grid, iterate, iterator, foreach"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid* ForEachCellInRangeV1(
		UGSModelGrid* TargetGridInOut,
		FGSIntBox3 CellIndexRange,
		const FProcessGridCellDelegate& ProcessCellFunc);


};