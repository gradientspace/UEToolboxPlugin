// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GradientspaceScriptTypes.h"
#include "GSTransformFunctions.generated.h"

class UDynamicMesh;
class AActor;

UCLASS(meta = (ScriptName = "GSS_TransformFunctions"), MinimalAPI)
class UGSScriptLibrary_TransformFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Transform the vertices and normals of TargetMesh from the local space of the TargetActor to world coordinates,
	 * by applying the ActorTransform
	 */
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|DynamicMesh", meta = (ScriptMethod, Keywords = "gss, transform, map"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Mesh") UDynamicMesh* MapMeshFromActorSpaceToWorld(
		UDynamicMesh* TargetMesh,
		AActor* TargetActor);

	/**
	 * Transform the vertices and normals of TargetMesh from World coordinates into the local space of the TargetActor,
	 * by applying the inverse of the ActorTransfsorm
	 */
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|DynamicMesh", meta = (ScriptMethod, Keywords = "gss, transform, map"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Mesh") UDynamicMesh* MapMeshFromWorldSpaceToActor(
		UDynamicMesh* TargetMesh,
		AActor* TargetActor);

	/**
	 * Transform the vertices and normals of TargetMesh from the local space of FromActor to the local space of ToActor,
	 * by applying FromActor.ActorTransform and then Inverse(ToActor.ActorTransform)
	 */
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|DynamicMesh", meta = (ScriptMethod, Keywords = "gss, transform, map"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Mesh") UDynamicMesh* MapMeshFromActorToActor(
		UDynamicMesh* TargetMesh,
		AActor* FromActor,
		AActor* ToActor,
		bool bInvert = false);

	/**
	 * Transform the vertices and normals of TargetMesh from the local space of one Transform to the local space of another,
	 * by applying MeshToWorld and then Inverse(OtherToWorld)
	 */
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|DynamicMesh", meta = (ScriptMethod, Keywords = "gss, transform, map"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Mesh") UDynamicMesh* MapMeshToOtherLocalSpace(
		UDynamicMesh* TargetMesh,
		FTransform MeshToWorld,
		FTransform OtherToWorld,
		bool bInvert = false);









	//
	// These functions are deprecated
	// Note: BlueprintInternalUseOnly tag will hide the function from search results
	//


	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = "Gradientspace|DynamicMesh", meta = (DisplayName="MapMeshToWorldFromActorSpace_Deprecated", ScriptMethod, Keywords = "gss, transform, map"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Mesh") UDynamicMesh* MapMeshToWorldFromActorSpace(
		UDynamicMesh* TargetMesh,
		AActor* TargetActor);

};