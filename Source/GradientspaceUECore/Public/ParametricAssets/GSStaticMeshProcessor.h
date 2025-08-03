#pragma once

#include "ParametricAssets/GSAssetProcessor.h"
#include "ParametricAssets/GSObjectProcessor.h"
#include "ParametricAssets/GSDynamicMeshProcessor.h"

#include "GSStaticMeshProcessor.generated.h"

class UStaticMesh;


UCLASS(Blueprintable, MinimalAPI, EditInlineNew)
class UGSStaticMeshProcessor : public UGSObjectProcessor
{
	GENERATED_BODY()
public:
	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="StaticMesh Processor")
	//FString ShortName;

public:

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "StaticMesh Processor")
	GRADIENTSPACEUECORE_API void OnProcessStaticMesh(UStaticMesh* StaticMesh);
	virtual void OnProcessStaticMesh_Implementation(UStaticMesh* StaticMesh) { }
};



UCLASS(Blueprintable, MinimalAPI, EditInlineNew)
class UGSStaticMeshAssetProcessor : public UGSAssetProcessorUserData
{
	GENERATED_BODY()
public:
	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StaticMesh Processor")
	//FString ShortName;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "StaticMesh Processor")
	GRADIENTSPACEUECORE_API void OnProcessStaticMeshAsset(UStaticMesh* StaticMesh);
	virtual void OnProcessStaticMeshAsset_Implementation(UStaticMesh* StaticMesh) { }
};




UCLASS(Blueprintable, MinimalAPI, EditInlineNew, meta = (DisplayName = "Static Mesh Generator Stack"))
class UGSStaticMeshAssetGeneratorStack : public UGSStaticMeshAssetProcessor
{
	GENERATED_BODY()
public:

	/**
	 * Initial mesh generator. Can be assumed to receive an initially-empty DynamicMesh,
	 * to append geometry to.
	 */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Operation Stack")
	TObjectPtr<UGSDynamicMeshProcessor> MeshGenerator;

	/**
	 * Modifier stack. These Processors are applied in-sequence to the DynamicMesh
	 * created by the initial MeshGenerator
	 */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Operation Stack")
	TArray<TObjectPtr<UGSDynamicMeshProcessor>> ModifierStack;


	/**
	 * The standard mesh generation sequenece is
	 *   1) run MeshGenerator
	 *   2) apply each Processor in ModifierStack
	 * The default OnRunMeshGeneration will call this function, which allows C++ subclasses
	 * to (eg) add additional steps without having to change the Event-level API
	 */
	UFUNCTION(BlueprintCallable, Category = "StaticMesh Generator")
	virtual void RunStandardMeshGeneration(UDynamicMesh* TargetMesh);

	/**
	 * Generate a new DynamicMesh to be used as the result mesh. 
	 * By default this calls RunStandardMeshGeneration(), which runs the 
	 * initial MeshGenerator and then each Processor in the ModifierStack
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Asset Generation")
	GRADIENTSPACEUECORE_API void OnRunMeshGeneration(UDynamicMesh* TargetMesh);
	virtual void OnRunMeshGeneration_Implementation(UDynamicMesh* TargetMesh);

	/**
	 * Copy te given DynamicMesh to the StaticMesh Asset. This is the final step,
	 * executed after the mesh is generated. The default behavior is simply to
	 * copy the generated mesh to LOD0 with default settings. 
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Asset Generation")
	GRADIENTSPACEUECORE_API void OnCopyResultToStaticMesh(UDynamicMesh* GeneratedMesh, UStaticMesh* StaticMesh);
	virtual void OnCopyResultToStaticMesh_Implementation(UDynamicMesh* GeneratedMesh, UStaticMesh* StaticMesh);


	/**
	 * C++ implementation of OnProcessStaticMeshAsset - this will:
	 *   1) allocate a new UDynamicMesh
	 *   2) run OnRunMeshGeneration(DynamicMesh)
	 *   3) run OnCopyResultToStaticMesh(DynamicMesh, StaticMesh)
	 *   4) free UDynamicMesh
	 */
	virtual void OnProcessStaticMeshAsset_Implementation(UStaticMesh* StaticMesh) override;


	/**
	 * returns child Generator and ModiferStack Processors
	 */
	virtual void OnCollectChildParameterObjects_Implementation(TArray<UObject*>& ChildObjectsInOut) override;
};

