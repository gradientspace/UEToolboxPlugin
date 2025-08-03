// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/IntVector.h"
#include "GradientspaceScriptTypes.h"
#include "GSBoxFunctions.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
DECLARE_DYNAMIC_DELEGATE_TwoParams(FEnumerateIndex3Delegate, const FGSIntBox3&, Box, FIntVector, IndexXYZ);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

UCLASS(meta = (ScriptName = "GSS_BoxFunctions"), MinimalAPI)
class UGSScriptLibrary_BoxFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Expand or contract the input IntBox3 along each coordinate vector 
	 */
	UFUNCTION(BlueprintPure, Category = "Gradientspace|Box", meta=(Keywords="gss, box"))
	static GRADIENTSPACESCRIPT_API FGSIntBox3
	ExpandBox(const FGSIntBox3& Box, int ExpandContractX = 1, int ExpandContractY = 1, int ExpandContractZ = 1);

	/**
	 * Call the IndexFunc delegate for each integer (X,Y,Z) coordinate value that is contained in the IndexRange IntBox3.
	 * The enumeration is inclusive (<=), ie the Max values are included in the enumeration, so the box Min=(0,0,0) Max=(0,0,0)
	 * is considered to include the point (0,0,0)
	 */
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|Box", meta=(DisplayName = "ForEach Index3 In Range", Keywords = "gss, box, foreach"))
	static GRADIENTSPACESCRIPT_API void ForEachIndex3InRangeV1(
		const FGSIntBox3& IndexRange,
		const FEnumerateIndex3Delegate& IndexFunc );



};