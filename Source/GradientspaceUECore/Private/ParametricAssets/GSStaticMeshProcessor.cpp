#include "ParametricAssets/GSStaticMeshProcessor.h"

#include "UDynamicMesh.h"

#include "GeometryScript/MeshAssetFunctions.h"


void UGSStaticMeshAssetGeneratorStack::RunStandardMeshGeneration(UDynamicMesh* TargetMesh)
{
	if (IsValid(MeshGenerator))
		MeshGenerator->OnProcessDynamicMesh(TargetMesh);
	for (TObjectPtr<UGSDynamicMeshProcessor> Modifier : ModifierStack) {
		if (IsValid(Modifier))
			Modifier->OnProcessDynamicMesh(TargetMesh);
	}
}


void UGSStaticMeshAssetGeneratorStack::OnRunMeshGeneration_Implementation(UDynamicMesh * TargetMesh) 
{ 
	RunStandardMeshGeneration(TargetMesh);
}


void UGSStaticMeshAssetGeneratorStack::OnCopyResultToStaticMesh_Implementation(UDynamicMesh* GeneratedMesh, UStaticMesh* StaticMesh)
{
	EGeometryScriptOutcomePins Outcome;
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(GeneratedMesh, StaticMesh,
		FGeometryScriptCopyMeshToAssetOptions(), FGeometryScriptMeshWriteLOD(),
		Outcome, nullptr);
}



void UGSStaticMeshAssetGeneratorStack::OnProcessStaticMeshAsset_Implementation(UStaticMesh* StaticMesh)
{ 
	// should cache this somehow? makes a lot of garbage otherwise.
	UDynamicMesh* TmpMesh = NewObject<UDynamicMesh>();
	TmpMesh->AddToRoot();

	OnRunMeshGeneration(TmpMesh);
	OnCopyResultToStaticMesh(TmpMesh, StaticMesh);

	TmpMesh->Reset();
	TmpMesh->RemoveFromRoot();
	TmpMesh->MarkAsGarbage();
}


void UGSStaticMeshAssetGeneratorStack::OnCollectChildParameterObjects_Implementation(TArray<UObject*>& ChildObjectsInOut)
{
	if (IsValid(MeshGenerator))
		ChildObjectsInOut.Add(MeshGenerator);
	for (TObjectPtr<UGSDynamicMeshProcessor> Modifier : ModifierStack) {
		if (IsValid(Modifier))
			ChildObjectsInOut.Add(Modifier);
	}
}