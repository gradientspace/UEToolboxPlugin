// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshActor/DefaultMeshRenderBuffers.h"
#include "Misc/EngineVersionComparison.h"

using namespace GS;


FMeshRenderBuffers::FMeshRenderBuffers(ERHIFeatureLevel::Type FeatureLevelType)
	: VertexFactory(FeatureLevelType, "FMeshRenderBuffers")
{
	//StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
	//StaticMeshVertexBuffer.SetUseHighPrecisionTangentBasis(true);
}


FMeshRenderBuffers::~FMeshRenderBuffers()
{
	check(IsInRenderingThread());
	if (TriangleCount > 0)
	{
		PositionVertexBuffer.ReleaseResource();
		StaticMeshVertexBuffer.ReleaseResource();
		ColorVertexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
		if (IndexBuffer.IsInitialized())
		{
			IndexBuffer.ReleaseResource();
		}
	}
}


void FMeshRenderBuffers::Upload(FRHICommandListImmediate& RHICmdList)
{
	// todo look at how this is being done, compared to other classes in 5.4+ where
	// some architectural changes have been made

	check(IsInRenderingThread());
	//FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

	if (TriangleCount == 0)
	{
		return;
	}

	InitOrUpdateResource(RHICmdList, &this->PositionVertexBuffer);
	InitOrUpdateResource(RHICmdList, &this->StaticMeshVertexBuffer);
	InitOrUpdateResource(RHICmdList, &this->ColorVertexBuffer);

	FLocalVertexFactory::FDataType Data;
	this->PositionVertexBuffer.BindPositionVertexBuffer(&this->VertexFactory, Data);
	this->StaticMeshVertexBuffer.BindTangentVertexBuffer(&this->VertexFactory, Data);
	this->StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&this->VertexFactory, Data);
	this->ColorVertexBuffer.BindColorVertexBuffer(&this->VertexFactory, Data);
#if UE_VERSION_OLDER_THAN(5,4,0)
	this->VertexFactory.SetData(Data);
#else
	this->VertexFactory.SetData(RHICmdList, Data);
#endif

	InitOrUpdateResource(RHICmdList, &this->VertexFactory);

	// aaahh why doing this again when it was done above??
	PositionVertexBuffer.InitResource(RHICmdList);
	StaticMeshVertexBuffer.InitResource(RHICmdList);
	ColorVertexBuffer.InitResource(RHICmdList);
	VertexFactory.InitResource(RHICmdList);

	if (IndexBuffer.Indices.Num() > 0)
	{
		IndexBuffer.InitResource(RHICmdList);
	}
}


void FMeshRenderBuffers::InitOrUpdateResource(FRHICommandListImmediate& RHICmdList, FRenderResource* Resource)
{
	check(IsInRenderingThread());
	if (!Resource->IsInitialized())
	{
		Resource->InitResource(RHICmdList);
	}
	else
	{
		Resource->UpdateRHI(RHICmdList);
	}
}

void FMeshRenderBuffers::EnqueueDeleteOnRenderThread(FMeshRenderBuffers* RenderBuffers)
{
	if (RenderBuffers->TriangleCount > 0)
	{
		ENQUEUE_RENDER_COMMAND(FMeshRenderBuffers_DestroyBuffers)(
			[RenderBuffers](FRHICommandListImmediate& RHICmdList)
		{
			delete RenderBuffers;
		});
	}
}
