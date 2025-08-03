// Copyright Gradientspace Corp. All Rights Reserved.
#include "GridActor/ModelGridActor.h"

#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"

#include "Engine/World.h"
#include "Engine/CollisionProfile.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"

#include "TimerManager.h"
#include "GSJobSubsystem.h"

#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridMeshCache.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "Utility/GSUEModelGridUtil.h"

#define LOCTEXT_NAMESPACE "AGSModelGridActor"

using namespace UE::Geometry;
using namespace GS;


AGSModelGridActor::AGSModelGridActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GridComponent = CreateDefaultSubobject<UGSModelGridComponent>(TEXT("GridComponent"));
	SetRootComponent(GridComponent);

	OnModelGridChanged_Handle = GridComponent->OnModelGridChanged().AddUObject(this, &AGSModelGridActor::OnModelGridChanged);
}


void AGSModelGridActor::PostLoad()
{
	Super::PostLoad();

	// this should never happen but maybe it might
	if (ensure(GridComponent != nullptr) == false)
	{
		GridComponent = NewObject<UGSModelGridComponent>(this, TEXT("GridComponent"));
	}

	if (GridComponent && OnModelGridChanged_Handle.IsValid() == false)
	{
		OnModelGridChanged_Handle = GridComponent->OnModelGridChanged().AddUObject(this, &AGSModelGridActor::OnModelGridChanged);
	}
}


void AGSModelGridActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void AGSModelGridActor::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	// After duplicating a GridComponent linked to an Asset, the Duplicate needs
	// to start listening to the Asset's delegates for changes. Component does not get PostDuplicate() itself,
	// but it does get PostEditImport(), which will handle T3D cases (editor duplicate/copy-paste/etc).
	// But it might not handle other cases, and it's (relatively) harmless
	if (GridComponent != nullptr)
		GridComponent->ForceUpdateConnections();

	// enqueue mesh update on next tick
	UGSJobSubsystem::EnqueueGameThreadTickJob(
		this, this, [this]() { UpdatePreviewMeshState(); });
}


#if WITH_EDITOR
void AGSModelGridActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();
	if (PropName == GET_MEMBER_NAME_CHECKED(AGSModelGridActor, bEnablePreviewMesh))
	{
		// enqueue mesh update on next tick
		UGSJobSubsystem::EnqueueGameThreadTickJob(
			this, this, [this]() { UpdatePreviewMeshState(); });
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(AGSModelGridActor, PreviewMeshCollisionMode))
	{
		if (PreviewMeshComponent) {
			UpdatePreviewMeshCollisionState();
			PreviewMeshComponent->UpdateCollision(true);
		}
	}
}

void AGSModelGridActor::PreEditUndo()
{
	Super::PreEditUndo();

	// if the PMC becomes non-null in PostEditUndo we need to fix something...
	// (also appears that the GridComponent Owner might be nullptr in this case?)
	bPreviewMeshWasNullInLastPreEditUndo = (PreviewMeshComponent == nullptr);
}

void AGSModelGridActor::PostEditUndo()
{
	Super::PostEditUndo();

	// on undo this actor will have been removed and we do not want to launch an update in that case...
	if (!IsValid(this)) {
		return;
	}

	// There is some kind of weird thing happening where if we duplicate a modelgrid using alt+drag in the viewport,
	// then undo/redo, during the first redo the PreviewMeshComponent will be null (expected), but the second time it
	// will not be null. And then the third time we will get a crash in the selection system. If we destroy the 
	// Component explicitly here that will not happen. But we do not want to destroy in *every* undo/redo transaction.
	// Naturally there is no direct way to check if (1) is this a redo and (2) is it an "actor-was-just-recreated" redo.
	// So we rely on this hack instead, passing info between PreEditUndo() and PostEditUndo()
	if (PreviewMeshComponent != nullptr && bPreviewMeshWasNullInLastPreEditUndo)
	{
		PreviewMeshComponent->UnregisterComponent();
		PreviewMeshComponent->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative, false));
		PreviewMeshComponent->DestroyComponent(false);
		PreviewMeshComponent = nullptr;
	}
	
	// don't start this update until after the transaction has completed
	UGSJobSubsystem::EnqueueGameThreadTickJob(
		this, this, [this]() { UpdatePreviewMeshState(); });
}
#endif


void AGSModelGridActor::UpdatePreviewMeshState()
{
//#if WITH_EDITOR
//	// this function should not run inside a transaction
//	ensure(!GIsTransacting);
//	ensure(GUndo == nullptr);
//#endif

	if (bEnablePreviewMesh && PreviewMeshComponent == nullptr)
	{
		OnModelGridChanged(GetGridComponent(), GetGrid());	// will launch compute job and then create PreviewMeshComponent
	}
	else if (PreviewMeshComponent != nullptr && bEnablePreviewMesh == false)
	{
		PreviewMeshComponent->UnregisterComponent();
		PreviewMeshComponent->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative, false));
		//PreviewMeshComponent->DetachFromParent(false, false);
		PreviewMeshComponent->DestroyComponent(false);
		PreviewMeshComponent = nullptr;
		// is there something we can do to force update of details panel component tree? this doesn't do it...
		//this->PostEditChange();
	}
}

void AGSModelGridActor::UpdateGridPreviewMesh(UE::Geometry::FDynamicMesh3&& PreviewMesh, TArray<UMaterialInterface*>&& Materials)
{
//#if WITH_EDITOR
//	// this function should not run inside a transaction
//	ensure(!GIsTransacting);
//	ensure(GUndo == nullptr);
//#endif

	// If the world is null, then the call to PreviewMeshComponent->RegisterComponent() will hit an ensure.
	// This seems to happen if placing a modelgrid in an unsaved new worldpartition world...not clear why.
	// Possibly in that case we should create the Component but not register it...but I don't understand the situation, yet.
	if (this->GetWorld() == nullptr)
		return;

	if (bEnablePreviewMesh == false) 
	{
		if (PreviewMeshComponent != nullptr)
			UpdatePreviewMeshState();
		return;
	}

	bool bWasCreatedThisFrame = false;
	if (PreviewMeshComponent == nullptr)
	{
		PreviewMeshComponent = NewObject<UDynamicMeshComponent>(this, FName("TransientDynamicMesh") );

		// have to explicitly set these flags because the uproperty markup will not be transferred to this new object
		// Note that this slows down PIE quite a bit...possibly should only use TextExportTransient?
		PreviewMeshComponent->ClearFlags(RF_Transactional);
		PreviewMeshComponent->SetFlags(RF_Transient);
		PreviewMeshComponent->SetFlags(RF_TextExportTransient);
		PreviewMeshComponent->SetFlags(RF_DuplicateTransient);

		PreviewMeshComponent->SetMobility(EComponentMobility::Movable);
		PreviewMeshComponent->SetGenerateOverlapEvents(false);

		PreviewMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		UpdatePreviewMeshCollisionState();

		PreviewMeshComponent->SetTangentsType(EDynamicMeshComponentTangentsMode::AutoCalculated);
		//PreviewMeshComponent->SetColorOverrideMode(EDynamicMeshComponentColorOverrideMode::VertexColors);

		PreviewMeshComponent->SetupAttachment(GetRootComponent());
		PreviewMeshComponent->RegisterComponent();
		bWasCreatedThisFrame = true;
	}

	if (Materials.Num() > 0)
	{
		PreviewMeshComponent->ConfigureMaterialSet(Materials);
	}
	else
	{
		UMaterial* GridMaterial = LoadObject<UMaterial>(nullptr, TEXT("/GradientspaceUEToolbox/Materials/M_GridEditMaterial"));
		if (GridMaterial == nullptr)
		{
			GridMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		PreviewMeshComponent->SetMaterial(0, GridMaterial);
	}

	PreviewMeshComponent->EditMesh([&](FDynamicMesh3& EditMesh)
	{
		EditMesh = MoveTemp(PreviewMesh);
	}, EDynamicMeshComponentRenderUpdateMode::FullUpdate);

	PreviewMeshComponent->UpdateCollision(true);
}



void AGSModelGridActor::SetPreviewMeshCollisionMode(EModelGridMeshCollisionMode NewMode)
{
	if (PreviewMeshCollisionMode != NewMode) {
		PreviewMeshCollisionMode = NewMode;
		if (PreviewMeshComponent) {
			UpdatePreviewMeshCollisionState();
			PreviewMeshComponent->UpdateCollision(true);
		}
	}
}

void AGSModelGridActor::UpdatePreviewMeshCollisionState()
{
	if (!PreviewMeshComponent) return;

	if (this->PreviewMeshCollisionMode == EModelGridMeshCollisionMode::ComplexAsSimple)
	{
		PreviewMeshComponent->CollisionType = ECollisionTraceFlag::CTF_UseComplexAsSimple;
		PreviewMeshComponent->bEnableComplexCollision = true;
	}
	else
	{
		PreviewMeshComponent->CollisionType = ECollisionTraceFlag::CTF_UseDefault;
		PreviewMeshComponent->bEnableComplexCollision = false;
	}
}


//void AGSModelGridActor::BuildModelGridMesh(
static void BuildModelGridMesh(
	UGSModelGrid* ModelGrid, 
	UGSGridMaterialSet* MaterialSet,
	FDynamicMesh3& TargetMesh,
	TArray<UMaterialInterface*>& MaterialsOut)
{
	using namespace GS;

	GS::SharedPtr<FReferenceSetMaterialMap> GridMaterialMap = MakeSharedPtr<FReferenceSetMaterialMap>();
	UGSGridMaterialSet::BuildMaterialMapForSet(MaterialSet, *GridMaterialMap);
	MaterialsOut = GridMaterialMap->MaterialList;

	// update mesh...
	FDynamicMesh3 FinalMesh;

	ModelGrid->ProcessGrid([&](const GS::ModelGrid& Grid)
	{
		GS::ExtractGridFullMesh(Grid, FinalMesh, /*materials=*/true, /*uvs=*/true, GridMaterialMap);
	});

	if (FinalMesh.TriangleCount() > 0)
	{
		FMergeCoincidentMeshEdges Welder(&FinalMesh);
		Welder.MergeVertexTolerance = 0.01;
		Welder.OnlyUniquePairs = false;
		Welder.bWeldAttrsOnMergedEdges = true;
		Welder.Apply();
		FinalMesh.CompactInPlace();
	}

	TargetMesh = MoveTemp(FinalMesh);
}



void AGSModelGridActor::OnModelGridChanged(UGSModelGridComponent* Component, UGSModelGrid* ModelGrid)
{
	UGSGridMaterialSet* MaterialSet = Component->GetGridMaterials();

	struct FJobData
	{
		FDynamicMesh3 FinalMesh;
		TArray<UMaterialInterface*> FinalMaterials;
		// todo some kind of timestamp so that if we launch two jobs, we take later result?
	};
	TSharedPtr<FJobData> RebuildMeshData = MakeShared<FJobData>();

	UGSJobSubsystem::EnqueueStandardJob(
		this, this, 
		[RebuildMeshData, MaterialSet, ModelGrid, this]() {
			if (!ensure(IsValid(this)))
				return false;

			BuildModelGridMesh(ModelGrid, MaterialSet, RebuildMeshData->FinalMesh, RebuildMeshData->FinalMaterials);
			return true;
		},
		[RebuildMeshData, this]()
		{
			if (ensure(IsValid(this)) )
				this->UpdateGridPreviewMesh(MoveTemp(RebuildMeshData->FinalMesh), MoveTemp(RebuildMeshData->FinalMaterials));
		},
		UGSJobSubsystem::FJobOptions::ThreadSafe() );

}



#undef LOCTEXT_NAMESPACE
