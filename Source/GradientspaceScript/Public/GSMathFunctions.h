// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GradientspaceScriptTypes.h"
#include "GSMathFunctions.generated.h"

class UDynamicMesh;
class AActor;

UCLASS(meta = (ScriptName = "GSS_MathFunctions"), MinimalAPI)
class UGSScriptLibrary_MathFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Calculate two points along the ray defined by Position and Direction, eg for use with raycast Trace functions.
	 * @param Position origin position of ray
	 * @param Direction direction of ray. May be passed un-normalized.
	 * @param StartDist distance along ray from Position to desired StartPoint
	 * @param EndDist distance along ray from Position to desired EndPoint
	 */
	UFUNCTION(BlueprintPure, Category = "Gradientspace|Math", meta=(Position="0,0,0", Direction="1,0,0", StartDist="0", EndDist="1000000", Keywords = "gss, trace, linetrace"))
	static GRADIENTSPACESCRIPT_API void CalcLineTraceStartEnd(
		FVector Position, FVector Direction,
		double StartDist, double EndDist,
		FVector& StartPoint, FVector& EndPoint );

};