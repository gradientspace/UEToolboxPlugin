// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshActor/GSMultiFrameMeshComponent.h"
#include "MeshActor/MultiFrameMeshSceneProxy.h"

#include "VectorUtil.h"
#include "BoxTypes.h"

#include "Materials/Material.h"
#include "MaterialDomain.h"

using namespace UE::Geometry;
using namespace GS;


UGSMultiFrameMeshComponent::UGSMultiFrameMeshComponent()
{
	BaseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
}

void UGSMultiFrameMeshComponent::SetMeshFrames(TArray<GS::DenseMesh>&& MeshFramesIn)
{
	MeshFrames = MoveTemp(MeshFramesIn);

	FAxisAlignedBox3d TmpBounds = FAxisAlignedBox3d::Empty();
	for (const auto& Mesh : MeshFrames)
	{
		int NumVertices = Mesh.GetVertexCount();
		for (int vid = 0; vid < NumVertices; ++vid)
		{
			TmpBounds.Contain((FVector3d)Mesh.GetPosition(vid));
		}
	}

	LocalBounds = (FBox)TmpBounds;
	MarkRenderStateDirty();
}



FPrimitiveSceneProxy* UGSMultiFrameMeshComponent::CreateSceneProxy()
{
	FMultiFrameMeshSceneProxy* NewSceneProxy = nullptr;

	if (MeshFrames.Num() > 0 && MeshFrames[0].GetTriangleCount() > 0)
	{
		NewSceneProxy = new FMultiFrameMeshSceneProxy(this);
		NewSceneProxy->Initialize(MeshFrames, BaseMaterial);
	}

	return NewSceneProxy;
}


int UGSMultiFrameMeshComponent::GetNumFrames() const
{
	return MeshFrames.Num();
}

void UGSMultiFrameMeshComponent::SetCurrentFrameIndex(int NewIndex)
{
	int SetNewIndex = FMath::Clamp(NewIndex, 0, MeshFrames.Num() - 1);
	if (SetNewIndex != CurrentFrameIndex)
	{
		CurrentFrameIndex = SetNewIndex;
		if (OverrideFrameIndex == -1 )
			UpdateFrameInSceneProxy();
	}
}

void UGSMultiFrameMeshComponent::SetOverrideFrameIndex(int NewIndex)
{
	int SetOverrideIndex = -1;
	if (NewIndex >= 0)
		SetOverrideIndex = NewIndex;
	else
		SetOverrideIndex = -1;
	if (OverrideFrameIndex != SetOverrideIndex)
	{
		OverrideFrameIndex = SetOverrideIndex;
		UpdateFrameInSceneProxy();
	}
}

void UGSMultiFrameMeshComponent::ClearOverrideFrameIndex()
{
	if (OverrideFrameIndex != -1)
	{
		OverrideFrameIndex = -1;
		UpdateFrameInSceneProxy();
	}
}

int UGSMultiFrameMeshComponent::GetVisibleFrameIndex() const
{
	if ( OverrideFrameIndex >= 0 )
		return  OverrideFrameIndex % MeshFrames.Num();
	else
		return FMath::Clamp(CurrentFrameIndex, 0, MeshFrames.Num() - 1);
}

void UGSMultiFrameMeshComponent::UpdateFrameInSceneProxy()
{
	// this may not be safe...maybe need to upload command to use ENQUEUE_RENDER_COMMAND ?
	if (SceneProxy != nullptr)
	{
		((FMultiFrameMeshSceneProxy*)SceneProxy)->UpdateVisibleFrameIndex(GetVisibleFrameIndex());
	}
}


FBoxSphereBounds UGSMultiFrameMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox LocalBoundingBox = (FBox)LocalBounds;
	FBoxSphereBounds Ret(LocalBoundingBox.TransformBy(LocalToWorld));
	Ret.BoxExtent *= BoundsScale;
	Ret.SphereRadius *= BoundsScale;
	return Ret;
}


#if WITH_EDITOR
void UGSMultiFrameMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();
	if (PropName == GET_MEMBER_NAME_CHECKED(UGSMultiFrameMeshComponent, OverrideFrameIndex))
	{
		UpdateFrameInSceneProxy();
	}
}
#endif



int32 UGSMultiFrameMeshComponent::GetNumMaterials() const
{
	return 1;
}
UMaterialInterface* UGSMultiFrameMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex == 0) return BaseMaterial;
	else return nullptr;
}
FMaterialRelevance UGSMultiFrameMeshComponent::GetMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const
{
	FMaterialRelevance Result = UMeshComponent::GetMaterialRelevance(InFeatureLevel);
	return Result;
}
void UGSMultiFrameMeshComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* Material)
{
	if (ElementIndex == 1 && BaseMaterial != Material)
	{
		BaseMaterial = Material;
		MarkRenderStateDirty();
	}
}
void UGSMultiFrameMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (BaseMaterial != nullptr)
		OutMaterials.Add(BaseMaterial);
}
void UGSMultiFrameMeshComponent::SetBaseMaterial(UMaterialInterface* Material)
{
	BaseMaterial = Material;
	MarkRenderStateDirty();
}
