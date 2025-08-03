// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"


#include "GSDynamicMeshIteratorFunctions.generated.h"

class UDynamicMesh;


DECLARE_DYNAMIC_DELEGATE_TwoParams(FProcessDynamicMeshIndexDelegate, UDynamicMesh*, DynamicMesh, int, ElementIndex);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FProcessDynamicMeshVertexDelegate, UDynamicMesh*, DynamicMesh, int, VertexID, FVector, Position);


UCLASS(meta = (ScriptName = "GSS_DynamicMeshIterators"), MinimalAPI)
class UGSScriptLibrary_DynamicMeshIterators : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Iterate through the vertex indices of TargetMesh and call the ProcessVertexFunc delegate for each.
	 * The intention is that the delegate will do things like reposition mesh vertices, not
	 * do things like edit the mesh (however the function will handle that case gracefully, and only
	 * iterate up to the initial MaxVertexID)
	 */
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|DynamicMesh", meta = (ScriptMethod, DisplayName = "ForEach Mesh Vertex", Keywords = "gss, foreach, mesh"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Mesh") UDynamicMesh* ForEachMeshVertexV1(
		UDynamicMesh* TargetMesh,
		const FProcessDynamicMeshVertexDelegate& ProcessVertexFunc);


	/**
	 * Iterate through the indices of Selection on TargetMesh and call the ProcessIndexFunc delegate for each.
	 * The selection indices are copied to an internal array before processing, so it is safe to change
	 * the mesh inside the delegate, however you must then handle (eg) potentially invalid indices in your delegate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|DynamicMesh", meta = (ScriptMethod, DisplayName = "ForEach MeshSelection Index", Keywords = "gss, foreach, mesh"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Mesh") UDynamicMesh* ForEachMeshSelectionIndexV1(
		UDynamicMesh* TargetMesh,
		FGeometryScriptMeshSelection Selection,
		const FProcessDynamicMeshIndexDelegate& ProcessIndexFunc);

};
