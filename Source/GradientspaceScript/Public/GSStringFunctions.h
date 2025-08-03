// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Containers/Array.h"
#include "GSStringFunctions.generated.h"

class UDynamicMesh;
class AActor;

UCLASS(meta = (ScriptName = "GSS_StringFunctions"), MinimalAPI)
class UGSScriptLibrary_StringFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Make a string containing the sequential list of values in FloatArray
	 *
	 * @param FloatArray array of values
	 * @param Prefix optional prefix string that will be prepended to sorted values
	 * @param bIncludeIndices if true, index of each item will be included (eg 0=FloatArray[0] 1=FloatArray[1] ... )
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Float Array to String", Keywords = "gss, tostring, array"), Category = "Gradientspace|Utilities|Array")
	static FString FloatArrayToString( const TArray<float>& FloatArray, FString Prefix = TEXT(""), bool bIncludeIndices=false);

	/**
	 * Make a string containing the sequential list of values in DoubleArray
	 *
	 * @param FloatArray array of values
	 * @param Prefix optional prefix string that will be prepended to sorted values
	 * @param bIncludeIndices if true, index of each item will be included (eg 0=DoubleArray[0] 1=DoubleArray[1] ... )
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Float Array to String", Keywords = "gss, tostring, array"), Category = "Gradientspace|Utilities|Array")
	static FString DoubleArrayToString(const TArray<double>& DoubleArray, FString Prefix = TEXT(""), bool bIncludeIndices = false);

	/**
	 * Make a string containing the sequential list of values in IntArray
	 *
	 * @param IntArray array of values
	 * @param Prefix optional prefix string that will be prepended to sorted values
	 * @param bIncludeIndices if true, index of each item will be included (eg 0=IntArray[0] 1=IntArray[1] ... )
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Int Array to String", Keywords = "gss, tostring, array"), Category = "Gradientspace|Utilities|Array")
	static FString IntArrayToString( const TArray<int>& IntArray, FString Prefix = TEXT(""), bool bIncludeIndices=false);

	/**
	 * Make a string containing the sequential list of values in StringArray
	 *
	 * @param StringArray array of values
	 * @param Prefix optional prefix string that will be prepended to sorted values
	 * @param bIncludeIndices if true, index of each item will be included (eg 0=StringArray[0] 1=StringArray[1] ... )
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "String Array to String", Keywords = "gss, tostring, array"), Category = "Gradientspace|Utilities|Array")
	static FString StringArrayToString(const TArray<FString>& StringArray, FString Prefix = TEXT(""), bool bIncludeIndices = false);

	/**
	 * Make a string containing the sequential list of values in NameArray
	 *
	 * @param NameArray array of values
	 * @param Prefix optional prefix string that will be prepended to sorted values
	 * @param bIncludeIndices if true, index of each item will be included (eg 0=NameArray[0] 1=NameArray[1] ... )
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Name Array to String", Keywords = "gss, tostring, array"), Category = "Gradientspace|Utilities|Array")
	static FString NameArrayToString(const TArray<FName>& NameArray, FString Prefix = TEXT(""), bool bIncludeIndices = false);
};