// Copyright Gradientspace Corp. All Rights Reserved.
#include "GSTransformFunctions.h"
#include "GradientspaceScriptUtil.h"
#include "ModelGridScriptUtil.h"
#include "DynamicMesh/MeshTransforms.h"
#include "UDynamicMesh.h"
#include "GameFramework/Actor.h"

using namespace UE::Geometry;
using namespace GS;

#define LOCTEXT_NAMESPACE "UGSScriptLibrary_TransformFunctions"

namespace GS {
static void ApplyTransformToMesh(UDynamicMesh* TargetMesh, const FTransform& Transform, bool bInvert)
{
	const bool bHandleNegativeScale = true;
	FTransformSRT3d TransformSRT = (FTransformSRT3d)Transform;
	if (bInvert)
		TransformSRT = TransformSRT.InverseUnsafe();

	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) {
		MeshTransforms::ApplyTransform(EditMesh, TransformSRT, bHandleNegativeScale);
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
}
static void ApplyTransformSeqToMesh(UDynamicMesh* TargetMesh, const FTransformSRT3d& Transform1, const FTransformSRT3d& Transform2)
{
	const bool bHandleNegativeScale = true;

	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) {

		// TODO: could try to combine these in most cases...have to do negative-scale handling ourselves though

		MeshTransforms::ApplyTransform(EditMesh, Transform1, bHandleNegativeScale);
		MeshTransforms::ApplyTransform(EditMesh, Transform2, bHandleNegativeScale);
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
}
}


UDynamicMesh* UGSScriptLibrary_TransformFunctions::MapMeshFromActorSpaceToWorld(
	UDynamicMesh* TargetMesh,
	AActor* TargetActor)
{
	CHECK_MESH_VALID_OR_RETURN(TargetMesh, TEXT("MapMeshFromActorSpaceToWorld"));
	if (TargetActor != nullptr)
		ApplyTransformToMesh(TargetMesh, TargetActor->GetTransform(), false);
	return TargetMesh;
}

UDynamicMesh* UGSScriptLibrary_TransformFunctions::MapMeshFromWorldSpaceToActor(
	UDynamicMesh* TargetMesh,
	AActor* TargetActor)
{
	CHECK_MESH_VALID_OR_RETURN(TargetMesh, TEXT("MapMeshFromWorldSpaceToActor"));
	if (TargetActor != nullptr)
		ApplyTransformToMesh(TargetMesh, TargetActor->GetTransform(), true);
	return TargetMesh;
}

UDynamicMesh* UGSScriptLibrary_TransformFunctions::MapMeshToWorldFromActorSpace(
	UDynamicMesh* TargetMesh,
	AActor* TargetActor)
{
	CHECK_MESH_VALID_OR_RETURN(TargetMesh, TEXT("MapMeshToWorldFromActorSpace"));
	if (TargetActor != nullptr)
		ApplyTransformToMesh(TargetMesh, TargetActor->GetTransform(), true);
	return TargetMesh;
}




UDynamicMesh* UGSScriptLibrary_TransformFunctions::MapMeshFromActorToActor(
	UDynamicMesh* TargetMesh,
	AActor* FromActor,
	AActor* ToActor,
	bool bInvert)
{
	CHECK_MESH_VALID_OR_RETURN(TargetMesh, TEXT("MapMeshFromActorToActor"));
	CHECK_OBJ_VALID_OR_RETURN_OTHER(FromActor, TargetMesh, TEXT("FromActor"), TEXT("MapMeshFromActorToActor"));
	CHECK_OBJ_VALID_OR_RETURN_OTHER(ToActor, TargetMesh, TEXT("ToActor"), TEXT("MapMeshFromActorToActor"));

	FTransformSRT3d ToWorldA = (FTransformSRT3d)FromActor->GetTransform();
	FTransformSRT3d ToWorldB = (FTransformSRT3d)ToActor->GetTransform();
	if (bInvert == false)
		ApplyTransformSeqToMesh(TargetMesh, ToWorldA, ToWorldB.InverseUnsafe());
	else
		ApplyTransformSeqToMesh(TargetMesh, ToWorldB, ToWorldA.InverseUnsafe());
	return TargetMesh;
}



UDynamicMesh* UGSScriptLibrary_TransformFunctions::MapMeshToOtherLocalSpace(
	UDynamicMesh* TargetMesh,
	FTransform MeshToWorld,
	FTransform OtherToWorld,
	bool bInvert)
{
	CHECK_MESH_VALID_OR_RETURN(TargetMesh, TEXT("MapMeshToOtherLocalSpace"));
	FTransformSRT3d ToWorldA = (FTransformSRT3d)MeshToWorld;
	FTransformSRT3d ToWorldB = (FTransformSRT3d)OtherToWorld;
	if (bInvert == false)
		ApplyTransformSeqToMesh(TargetMesh, ToWorldA, ToWorldB.InverseUnsafe());
	else
		ApplyTransformSeqToMesh(TargetMesh, ToWorldB, ToWorldA.InverseUnsafe());
	return TargetMesh;
}





#undef LOCTEXT_NAMESPACE