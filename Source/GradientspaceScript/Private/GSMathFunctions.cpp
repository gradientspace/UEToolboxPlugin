// Copyright Gradientspace Corp. All Rights Reserved.
#include "GSMathFunctions.h"
#include "GradientspaceScriptUtil.h"
#include "ModelGridScriptUtil.h"
#include "UDynamicMesh.h"
#include "Math/GSMath.h"

using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UGSScriptLibrary_MathFunctions"


void UGSScriptLibrary_MathFunctions::CalcLineTraceStartEnd(
	FVector Position, FVector Direction,
	double StartDistance, double EndDistance,
	FVector& StartPoint, FVector& EndPoint)
{
	FVector NormDir = Normalized(Direction);
	if (NormDir.IsNearlyZero()) {
		UE_LOG(LogGradientspace, Warning, TEXT("CalcLineTraceStartEnd: ray direction (%f,%f,%f) is invalid"), Direction.X, Direction.Y, Direction.Z);
		StartPoint = EndPoint = Position;
		return;
	}

	StartDistance = FMath::Clamp(StartDistance, -(double)GS::Mathf::SafeMaxExtent(), (double)GS::Mathf::SafeMaxExtent());
	EndDistance = FMath::Clamp(EndDistance, -(double)GS::Mathf::SafeMaxExtent(), (double)GS::Mathf::SafeMaxExtent());
	StartPoint = Position + StartDistance * NormDir;
	EndPoint = Position + EndDistance * NormDir;
}



#undef LOCTEXT_NAMESPACE








