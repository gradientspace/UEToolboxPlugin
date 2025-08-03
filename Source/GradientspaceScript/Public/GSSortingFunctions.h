// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Containers/Array.h"
#include "GSSortingFunctions.generated.h"

class UDynamicMesh;
class AActor;

UCLASS(meta = (ScriptName = "GSS_SortFunctions"), MinimalAPI)
class UGSScriptLibrary_SortFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**  
	 * Sort the input array and return it as a new array, and also return the list of corresponding indices
	 * of the sorted values in the input array (which can be used to index into arrays that might be associated
	 * with the input array)
	 * 
	 * @param FloatArray input array of values
	 * @param SortedArray output sorted version of FloatArray
	 * @param SortingOrder output sort-order indices, ie SortedArray[k] = FloatArray[SortingOrder[k]]
	 * @param bDecreasing if true, values will be sorted in decreasing order instead of increasing
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Sort Float Array", Keywords = "gss, sort, array"), Category = "Gradientspace|Utilities|Array")
	static void SortFloatArray(
		const TArray<float>& FloatArray, TArray<float>& SortedArray, TArray<int>& SortingOrder, bool bDecreasing = false);

	/**
	 * Sort the input array and return it as a new array, and also return the list of corresponding indices
	 * of the sorted values in the input array (which can be used to index into arrays that might be associated
	 * with the input array)
	 *
	 * @param DoubleArray input array of values
	 * @param SortedArray output sorted version of FloatArray
	 * @param SortingOrder output sort-order indices, ie SortedArray[k] = FloatArray[SortingOrder[k]]
	 * @param bDecreasing if true, values will be sorted in decreasing order instead of increasing
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Sort Float Array", Keywords = "gss, sort, array"), Category = "Gradientspace|Utilities|Array")
	static void SortDoubleArray(
		const TArray<double>& DoubleArray, TArray<double>& SortedArray, TArray<int>& SortingOrder, bool bDecreasing = false);

	/**
	 * Sort the input array and return it as a new array, and also return the list of corresponding indices
	 * of the sorted values in the input array (which can be used to index into arrays that might be associated
	 * with the input array)
	 *
	 * @param IntArray input array of values
	 * @param SortedArray output sorted version of FloatArray
	 * @param SortingOrder output sort-order indices, ie SortedArray[k] = FloatArray[SortingOrder[k]]
	 * @param bDecreasing if true, values will be sorted in decreasing order instead of increasing
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Sort Int Array", Keywords = "gss, sort, array"), Category = "Gradientspace|Utilities|Array")
	static void SortIntArray(
		const TArray<int>& IntArray, TArray<int>& SortedArray, TArray<int>& SortingOrder, bool bDecreasing = false);

	/**
	 * Sort the input array and return it as a new array, and also return the list of corresponding indices
	 * of the sorted values in the input array (which can be used to index into arrays that might be associated
	 * with the input array)
	 *
	 * @param StringArray input array of values
	 * @param SortedArray output sorted version of FloatArray
	 * @param SortingOrder output sort-order indices, ie SortedArray[k] = FloatArray[SortingOrder[k]]
	 * @param bDecreasing if true, values will be sorted in decreasing order instead of increasing
	 * @param bIgnoreCase if true, case will be ignored when comparing strings
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Sort String Array", Keywords = "gss, sort, array"), Category = "Gradientspace|Utilities|Array")
	static void SortStringArray(
		const TArray<FString>& StringArray, TArray<FString>& SortedArray, TArray<int>& SortingOrder, bool bDecreasing = false, bool bIgnoreCase = true);


	/**
	 * Sort the input array and return it as a new array, and also return the list of corresponding indices
	 * of the sorted values in the input array (which can be used to index into arrays that might be associated
	 * with the input array)
	 *
	 * @param NameArray input array of values
	 * @param SortedArray output sorted version of FloatArray
	 * @param SortingOrder output sort-order indices, ie SortedArray[k] = FloatArray[SortingOrder[k]]
	 * @param bDecreasing if true, values will be sorted in decreasing order instead of increasing
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Sort Name Array", Keywords = "gss, sort, array"), Category = "Gradientspace|Utilities|Array")
	static void SortFNameArray(
		const TArray<FName>& NameArray, TArray<FName>& SortedArray, TArray<int>& SortingOrder, bool bDecreasing = false);

};