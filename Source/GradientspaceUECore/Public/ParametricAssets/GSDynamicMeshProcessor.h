#pragma once

#include "ParametricAssets/GSObjectProcessor.h"

#include "GSDynamicMeshProcessor.generated.h"


class UDynamicMesh;

UCLASS(Blueprintable, MinimalAPI, EditInlineNew)
class UGSDynamicMeshProcessor : public UGSObjectProcessor
{
	GENERATED_BODY()
public:
	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DynamicMesh Processor")
	//FString ShortName;

public:

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DynamicMesh Processor")
	GRADIENTSPACEUECORE_API void OnProcessDynamicMesh(UDynamicMesh* DynamicMesh);
	virtual void OnProcessDynamicMesh_Implementation(UDynamicMesh* DynamicMesh) { }
};
