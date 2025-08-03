// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "MeshActor/DenseMeshSceneProxy.h"
#include "SceneInterface.h"

namespace GS
{

#if 0   // broken in 5.4


//
// TODO: currently only works w/ GPUScene. for non-GPUScene, requires custom vertex factory
// that has the instance transforms/etc, that's what FInstancedStaticMeshDataType / FInstancedStaticMeshVertexFactory is
// about in InstancedStaticMesh.h
//
class FGSInstancedMeshSceneProxy : public FDenseMeshSceneProxy
{
public:
	FGSInstancedMeshSceneProxy(UGSMeshComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel)
		: FDenseMeshSceneProxy(InComponent)
	{

		// derived from FInstancedStaticMeshSceneProxy::SetupProxy()
		//bAnySegmentUsesWorldPositionOffset = false;
		bHasPerInstanceRandom = false;
		bHasPerInstanceCustomData = false;
		// see FInstancedStaticMeshSceneProxy::SetupProxy() for how to determine if any material has per-instance random or per-instance custom data

		// necessary for instancing?
		bool bIsUsingGPUScene = UseGPUScene(GetScene().GetShaderPlatform(), GetScene().GetFeatureLevel());
		if (!bIsUsingGPUScene)
		{
			check(false);
			return;
		}

		// should come from Component, just a test for now...
		int NumRenderInstances = 2;

		bSupportsInstanceDataBuffer = true;
		InstanceSceneData.SetNumZeroed(NumRenderInstances);

		bHasPerInstanceDynamicData = false; // InComponent->PerInstancePrevTransform.Num() > 0 && InComponent->PerInstancePrevTransform.Num() == InComponent->GetInstanceCount();
		InstanceDynamicData.SetNumZeroed(bHasPerInstanceDynamicData ? NumRenderInstances : 0);

		InstanceRandomID.SetNumZeroed(bHasPerInstanceRandom ? NumRenderInstances : 0);	// skip if not using in materials

		// if component supported custom-data floats we would initialize this here
		bHasPerInstanceCustomData = false;
		InstanceCustomData.SetNumZeroed(0);

		FVector3d TranslatedSpaceOffset = FVector3d::Zero();	// this can be used to help w/ LWC-scale issues

		for (int32 InstanceIndex = 0; InstanceIndex < NumRenderInstances; ++InstanceIndex)
		{
			// ISMC supports remapping indices here in case some instances are skipped/etc (NumRenderInstances also needs to take it into account)
			int32 RenderInstanceIndex = InstanceIndex;

			// this is being initialized from here down
			FInstanceSceneData& NewSceneData = InstanceSceneData[RenderInstanceIndex];

			FTransform InstanceTransform;
			InstanceTransform.SetTranslation( FVector( 100*(InstanceIndex+1), 0, 0) );		// should come from Component
			InstanceTransform.AddToTranslation(TranslatedSpaceOffset);
			NewSceneData.LocalToPrimitive = InstanceTransform.ToMatrixWithScale();

			// InstanceCustomData / etc could be initialized here
		}
	}



	SIZE_T GetTypeHash() const override {
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}
};


#endif

} // end namespace GS
