// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Misc/EngineVersionComparison.h"
#include "Containers/Array.h"

namespace GSUE
{

template<typename ElementType>
ElementType TArrayPop(TArray<ElementType>& Array)
{
#if UE_VERSION_OLDER_THAN(5,4,0)
	return Array.Pop(false);
#else
	return Array.Pop(EAllowShrinking::No);
#endif
}

template<typename ElementType>
void TArraySetNum(TArray<ElementType>& Array, int NewNum, bool bAllowShrinking)
{
#if UE_VERSION_OLDER_THAN(5,4,0)
	return Array.SetNum(NewNum, bAllowShrinking);
#else
	return Array.SetNum(NewNum, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
#endif
}

template<typename ElementType>
void TArrayRemoveAt(TArray<ElementType>& Array, int Index, int Count, bool bAllowShrinking)
{
#if UE_VERSION_OLDER_THAN(5,4,0)
	return Array.RemoveAt(Index, Count, bAllowShrinking);
#else
	return Array.RemoveAt(Index, Count, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
#endif
}


}
