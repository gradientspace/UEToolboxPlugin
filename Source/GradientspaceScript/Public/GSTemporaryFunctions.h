// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTemporaryFunctions.generated.h"


UCLASS(meta = (ScriptName = "GSS_TemporaryFunctions"), MinimalAPI)
class UGSScriptLibrary_TemporaryFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Request a temporary UDynamicMesh object that can be used for a single frame, ie in the context
	 * of a single Blueprint evaluation. This is an alternative to using or creating a DynamicMeshPool,
	 * essentially it is a global shared pool. 
	 * 
	 * The caller /must/ ensure that the returned mesh object is not referenced across multiple BP evaluations,
	 * ie it cannot be stored as a variable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|Utility", meta=(DisplayName="Request One-Frame Temp DynamicMesh", Keywords="gss, mesh, oneframe, temp"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Temp Mesh") UDynamicMesh*
	RequestOneFrameTemporaryDynamicMesh();

	/**
	 * Return a UDynamicMesh allocated via RequestOneFrameTemporaryDynamicMesh to the global pool. 
	 * Although not strictly necessary, this can avoid creating a bunch of spurious uobjects if (for example)
	 * you are looping over many meshes and processing each one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|Utility", meta = (DisplayName = "Return One-Frame Temp DynamicMesh", Keywords = "gss, mesh, oneframe, temp"))
	static GRADIENTSPACESCRIPT_API void
	ReturnTemporaryDynamicMesh(UDynamicMesh* Mesh);

};