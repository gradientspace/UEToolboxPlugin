// Copyright Gradientspace Corp. All Rights Reserved.

#include "MeshActor/GSMeshComponent.h"
#include "MeshActor/DenseMeshSceneProxy.h"
#include "MeshActor/InstancedDenseMeshSceneProxy.h"

#include "VectorUtil.h"
#include "BoxTypes.h"

using namespace UE::Geometry;
using namespace GS;


UGSMeshComponent::UGSMeshComponent()
{
	Mesh = MakeUnique<DenseMesh>();
}


void UGSMeshComponent::ProcessMesh(TFunctionRef<void(const DenseMesh& Mesh)> ProcessFunc) const
{
	if (Mesh.IsValid())
	{
		ProcessFunc(*Mesh);
	}
}

void UGSMeshComponent::EditMesh(TFunctionRef<void(DenseMesh& Mesh)> UpdateFunc)
{
	if (Mesh.IsValid())
	{
		UpdateFunc(*Mesh);

		FAxisAlignedBox3d TmpBounds = FAxisAlignedBox3d::Empty();
		int NumVertices = Mesh->GetVertexCount();
		for (int vid = 0; vid < NumVertices; ++vid )
		{
			TmpBounds.Contain( (FVector3d)Mesh->GetPosition(vid) );
		}

		LocalBounds = (FBox)TmpBounds;

		MarkRenderStateDirty();
	}
}



void UGSMeshComponent::SetUseDynamicDrawPath(bool bEnable)
{
	if (bUseDynamicDrawPath != bEnable)
	{
		bUseDynamicDrawPath = bEnable;
		MarkRenderStateDirty();
	}
}



FPrimitiveSceneProxy* UGSMeshComponent::CreateSceneProxy()
{
	FDenseMeshSceneProxy* NewSceneProxy = nullptr;

	ProcessMesh([&](const DenseMesh& Mesh) 
	{
		if (Mesh.GetTriangleCount() > 0)
		{
			NewSceneProxy = new FDenseMeshSceneProxy(this);
			//NewSceneProxy = new FGSInstancedMeshSceneProxy(this, GetWorld()->GetFeatureLevel());

			//NewSceneProxy->InitializeFromMesh_Fastest(Mesh);
			NewSceneProxy->InitializeFromMesh_LocalOptimize(Mesh);
		}
	});

	return NewSceneProxy;
}


FBoxSphereBounds UGSMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox LocalBoundingBox = (FBox)LocalBounds;
	FBoxSphereBounds Ret(LocalBoundingBox.TransformBy(LocalToWorld));
	Ret.BoxExtent *= BoundsScale;
	Ret.SphereRadius *= BoundsScale;
	return Ret;
}
