// Copyright Gradientspace Corp. All Rights Reserved.
#include "GSStringFunctions.h"
#include "GradientspaceScriptUtil.h"


#define LOCTEXT_NAMESPACE "UGSScriptLibrary_StringFunctions"

template<typename ElementType>
FString ArrayToString(const TArray<ElementType>& Array, const FString& Prefix, bool bIncludeIndices,
	TFunctionRef<void(FStringBuilderBase& Bldr, int Index)> AppendItemFunc )
{
	FStringBuilderBase StringBuilder;
	if (!Prefix.IsEmpty())
		StringBuilder.Append(Prefix);
	int N = Array.Num();
	for (int i = 0; i < N - 1; ++i) {
		if (bIncludeIndices)
			StringBuilder.Appendf(TEXT("%d="), i);
		AppendItemFunc(StringBuilder, i);
		StringBuilder.AppendChar(' ');
	}
	if (bIncludeIndices)
		StringBuilder.Appendf(TEXT("%d="), N - 1);
	AppendItemFunc(StringBuilder, N-1);

	return FString(StringBuilder.ToString());
}


FString UGSScriptLibrary_StringFunctions::FloatArrayToString(const TArray<float>& FloatArray, FString Prefix, bool bIncludeIndices)
{
	return ArrayToString(FloatArray, Prefix, bIncludeIndices,
		[&](FStringBuilderBase& Bldr, int Index) { Bldr.Appendf(TEXT("%f"), FloatArray[Index]); });
}

FString UGSScriptLibrary_StringFunctions::DoubleArrayToString(const TArray<double>& DoubleArray, FString Prefix, bool bIncludeIndices)
{
	return ArrayToString(DoubleArray, Prefix, bIncludeIndices,
		[&](FStringBuilderBase& Bldr, int Index) { Bldr.Appendf(TEXT("%f"), DoubleArray[Index]); });
}

FString UGSScriptLibrary_StringFunctions::IntArrayToString(const TArray<int>& IntArray, FString Prefix, bool bIncludeIndices)
{
	return ArrayToString(IntArray, Prefix, bIncludeIndices,
		[&](FStringBuilderBase& Bldr, int Index) { Bldr.Appendf(TEXT("%d"), IntArray[Index]); });
}

FString UGSScriptLibrary_StringFunctions::StringArrayToString(const TArray<FString>& StringArray, FString Prefix, bool bIncludeIndices)
{
	return ArrayToString(StringArray, Prefix, bIncludeIndices,
		[&](FStringBuilderBase& Bldr, int Index) { Bldr.Append(StringArray[Index]); });
}

FString UGSScriptLibrary_StringFunctions::NameArrayToString(const TArray<FName>& NameArray, FString Prefix, bool bIncludeIndices)
{
	return ArrayToString(NameArray, Prefix, bIncludeIndices,
		[&](FStringBuilderBase& Bldr, int Index) { Bldr.Append(NameArray[Index].ToString()); });
}

#undef LOCTEXT_NAMESPACE