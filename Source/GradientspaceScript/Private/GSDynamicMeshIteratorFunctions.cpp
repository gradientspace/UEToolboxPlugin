// Copyright Gradientspace Corp. All Rights Reserved.
#include "GSDynamicMeshIteratorFunctions.h"
#include "GradientspaceScriptUtil.h"
#include "UDynamicMesh.h"

using namespace UE::Geometry;
//using namespace GS;

#define LOCTEXT_NAMESPACE "UGSScriptLibrary_DynamicMeshIterators"


UDynamicMesh* UGSScriptLibrary_DynamicMeshIterators::ForEachMeshVertexV1(
	UDynamicMesh* TargetMesh,
	const FProcessDynamicMeshVertexDelegate& ProcessVertexFunc)
{
	CHECK_MESH_VALID_OR_RETURN(TargetMesh, TEXT("ForEachMeshVertexV1"));
	CHECK_DELEGATE_BOUND_OR_RETURN_OBJECT(ProcessVertexFunc, TargetMesh, TEXT("ProcessVertexFunc"), TEXT("ForEachMeshVertexV1"));

	const FDynamicMesh3& ConstMeshRef = TargetMesh->GetMeshRef();
	int MaxVertexID = ConstMeshRef.MaxVertexID();

	for (int vid = 0; vid < MaxVertexID; ++vid) {
		if (ConstMeshRef.IsVertex(vid)) {
			FVector3d VertexPosition = ConstMeshRef.GetVertex(vid);
			ProcessVertexFunc.Execute(TargetMesh, vid, VertexPosition);
		}
	}

	return TargetMesh;
}


UDynamicMesh* UGSScriptLibrary_DynamicMeshIterators::ForEachMeshSelectionIndexV1(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshSelection Selection,
	const FProcessDynamicMeshIndexDelegate& ProcessIndexFunc)
{
	CHECK_MESH_VALID_OR_RETURN(TargetMesh, TEXT("ForEachMeshSelectionIndexV1"));
	CHECK_DELEGATE_BOUND_OR_RETURN_OBJECT(ProcessIndexFunc, TargetMesh, TEXT("ProcessIndexFunc"), TEXT("ForEachMeshSelectionIndexV1"));

	TArray<int32> SelectionIndices;
	const FDynamicMesh3& ConstMeshRef = TargetMesh->GetMeshRef();
	Selection.ConvertToMeshIndexArray(ConstMeshRef, SelectionIndices);
	int Num = SelectionIndices.Num();

	// todo support validation??

	for (int k = 0; k < Num; ++k) {
		int Index = SelectionIndices[k];
		ProcessIndexFunc.Execute(TargetMesh, Index);
	}

	return TargetMesh;
}



#undef LOCTEXT_NAMESPACE