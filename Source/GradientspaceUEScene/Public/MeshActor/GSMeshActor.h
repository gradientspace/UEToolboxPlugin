// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MeshActor/GSMeshComponent.h"
#include "MeshActor/GSMultiFrameMeshComponent.h"

#include "GSMeshActor.generated.h"

UCLASS(ConversionRoot, ComponentWrapperClass, ClassGroup = Gradientspace, meta = (ChildCanTick), NotPlaceable, Hidden, Experimental)
class GRADIENTSPACEUESCENE_API AGSMeshActor : public AActor
{
	GENERATED_BODY()
public:
	AGSMeshActor();

protected:
	UPROPERTY(Category = GSMeshActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics", AllowPrivateAccess = "true"))
	TObjectPtr<class UGSMeshComponent> MeshComponent;

public:
	UFUNCTION(BlueprintCallable, Category = GSMeshActor)
	UGSMeshComponent* GetMeshComponent() const { return MeshComponent; }

};



UCLASS(ConversionRoot, ComponentWrapperClass, ClassGroup = Gradientspace, meta = (ChildCanTick), NotPlaceable, Hidden, Experimental)
class GRADIENTSPACEUESCENE_API AGSMultiFrameMeshActor : public AActor
{
	GENERATED_BODY()
public:
	AGSMultiFrameMeshActor();

protected:
	UPROPERTY(Category = GSMeshActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics", AllowPrivateAccess = "true"))
	TObjectPtr<class UGSMultiFrameMeshComponent> MeshComponent;

public:
	UFUNCTION(BlueprintCallable, Category = GSMeshActor)
	UGSMultiFrameMeshComponent* GetMeshComponent() const { return MeshComponent; }

};
