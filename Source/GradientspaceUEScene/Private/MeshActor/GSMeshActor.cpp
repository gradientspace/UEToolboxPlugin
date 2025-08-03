// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshActor/GSMeshActor.h"

#include "Engine/CollisionProfile.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"

#define LOCTEXT_NAMESPACE "AGSMeshActor"

AGSMeshActor::AGSMeshActor()
{
	MeshComponent = CreateDefaultSubobject<UGSMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	//MeshComponent->CollisionType = ECollisionTraceFlag::CTF_UseDefault;

	//MeshComponent->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));

	SetRootComponent(MeshComponent);
}


AGSMultiFrameMeshActor::AGSMultiFrameMeshActor()
{
	MeshComponent = CreateDefaultSubobject<UGSMultiFrameMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	//MeshComponent->CollisionType = ECollisionTraceFlag::CTF_UseDefault;
	//MeshComponent->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));

	SetRootComponent(MeshComponent);
}



#undef LOCTEXT_NAMESPACE
