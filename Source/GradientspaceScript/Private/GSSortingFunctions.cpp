// Copyright Gradientspace Corp. All Rights Reserved.
#include "GSSortingFunctions.h"
#include "GradientspaceScriptUtil.h"
#include "Math/GSMath.h"

#define LOCTEXT_NAMESPACE "UGSScriptLibrary_SortFunctions"


template<typename ValueType>
void SortValues(const TArray<ValueType>& InputArray, TArray<ValueType>& SortedArray, TArray<int>& SortingOrder, bool bDecreasing)
{
	int N = InputArray.Num();
	if (N == 0) return;

	struct FValue
	{
		ValueType Value;
		int Index;
		bool operator<(const FValue& Other) const { return Value < Other.Value; }
	};
	TArray<FValue> SortList;
	SortList.Reserve(N);
	for (int k = 0; k < N; ++k)
	{
		SortList.Add(FValue{ InputArray[k], k });
	}
	SortList.StableSort();
	if (bDecreasing)
	{
		Algo::Reverse(SortList);
	}
	SortedArray.Reserve(N);
	SortingOrder.Reserve(N);
	for (const FValue& V : SortList)
	{
		SortedArray.Add(V.Value);
		SortingOrder.Add(V.Index);
	}
}



template<typename ValueType>
void SortValuesCustom(const TArray<ValueType>& InputArray, TArray<ValueType>& SortedArray, TArray<int>& SortingOrder, bool bDecreasing,
	TFunctionRef<bool(const ValueType& A, const ValueType& B)> ComparisonF)
{
	int N = InputArray.Num();
	if (N == 0) return;

	struct FValue
	{
		ValueType Value;
		int Index;
	};
	auto LocalCompareF = [&](const FValue& A, const FValue& B) { return ComparisonF(A.Value, B.Value); };

	TArray<FValue> SortList;
	SortList.Reserve(N);
	for (int k = 0; k < N; ++k)
	{
		SortList.Add(FValue{ InputArray[k], k });
	}
	SortList.StableSort(LocalCompareF);
	if (bDecreasing)
	{
		Algo::Reverse(SortList);
	}
	SortedArray.Reserve(N);
	SortingOrder.Reserve(N);
	for (const FValue& V : SortList)
	{
		SortedArray.Add(V.Value);
		SortingOrder.Add(V.Index);
	}
}



void UGSScriptLibrary_SortFunctions::SortFloatArray(const TArray<float>& FloatArray, TArray<float>& SortedArray, TArray<int>& SortingOrder, bool bDecreasing)
{
	SortValues<float>(FloatArray, SortedArray, SortingOrder, bDecreasing);
}

void UGSScriptLibrary_SortFunctions::SortDoubleArray(const TArray<double>& DoubleArray, TArray<double>& SortedArray, TArray<int>& SortingOrder, bool bDecreasing)
{
	SortValues<double>(DoubleArray, SortedArray, SortingOrder, bDecreasing);
}

void UGSScriptLibrary_SortFunctions::SortIntArray(const TArray<int>& IntArray, TArray<int>& SortedArray, TArray<int>& SortingOrder, bool bDecreasing)
{
	SortValues<int>(IntArray, SortedArray, SortingOrder, bDecreasing);
}

void UGSScriptLibrary_SortFunctions::SortStringArray(const TArray<FString>& StringArray, TArray<FString>& SortedArray, TArray<int>& SortingOrder, bool bDecreasing, bool bIgnoreCase)
{
	ESearchCase::Type UseCase = (bIgnoreCase) ? ESearchCase::Type::IgnoreCase : ESearchCase::Type::CaseSensitive;
	SortValuesCustom<FString>(StringArray, SortedArray, SortingOrder, bDecreasing,
		[UseCase](const FString& A, const FString& B) { return A.Compare(B, UseCase) < 0; }
	);
}

void UGSScriptLibrary_SortFunctions::SortFNameArray(const TArray<FName>& NameArray, TArray<FName>& SortedArray, TArray<int>& SortingOrder, bool bDecreasing)
{
	SortValuesCustom<FName>(NameArray, SortedArray, SortingOrder, bDecreasing,
		[](const FName& A, const FName& B) {  return A.LexicalLess(B);  }
	);
}

#undef LOCTEXT_NAMESPACE