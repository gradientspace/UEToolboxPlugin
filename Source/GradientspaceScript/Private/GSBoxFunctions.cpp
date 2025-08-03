// Copyright Gradientspace Corp. All Rights Reserved.
#include "GSBoxFunctions.h"
#include "GradientspaceScriptUtil.h"
#include "ModelGridScriptUtil.h"
#include "Grid/GSGridUtil.h"

using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UGSScriptLibrary_BoxFunctions"

FGSIntBox3 UGSScriptLibrary_BoxFunctions::ExpandBox(const FGSIntBox3& Box, int ExpandContractX, int ExpandContractY, int ExpandContractZ)
{
	FIntVector Delta(ExpandContractX, ExpandContractY, ExpandContractZ);
	FGSIntBox3 NewBox { Box.Min-Delta, Box.Max+Delta };
	return NewBox;
}


void UGSScriptLibrary_BoxFunctions::ForEachIndex3InRangeV1(
	const FGSIntBox3& IndexRange,
	const FEnumerateIndex3Delegate& IndexFunc)
{
	CHECK_DELEGATE_BOUND_OR_RETURN(IndexFunc, TEXT("IndexFunc"), TEXT("ForEachIndex3InRangeV1"));

	EnumerateCellsInRangeInclusive(IndexRange.Min, IndexRange.Max, [&](GS::Vector3i Index) {
		IndexFunc.ExecuteIfBound(IndexRange, Index);
	});
}


#undef LOCTEXT_NAMESPACE