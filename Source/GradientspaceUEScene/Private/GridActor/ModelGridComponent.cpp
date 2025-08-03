// Copyright Gradientspace Corp. All Rights Reserved.

#include "GridActor/ModelGridComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/CollisionProfile.h"
#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridCollision.h"


UGSModelGridComponent::UGSModelGridComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	LocalModelGrid = CreateDefaultSubobject<UGSModelGrid>(TEXT("ModelGrid"));

	LocalModelGridMaterials = CreateDefaultSubobject<UGSGridMaterialSet>(TEXT("GridMaterials"));

	OnLocalModelGridUpdated_Handle = LocalModelGrid->OnModelGridReplaced().AddUObject(this, &UGSModelGridComponent::OnLocalModelGridUpdated);
}


void UGSModelGridComponent::PostLoad()
{
	Super::PostLoad();

	// this should never happen but maybe it might
	if (ensure(LocalModelGrid != nullptr) == false)
	{
		LocalModelGrid = NewObject<UGSModelGrid>(this, TEXT("ModelGrid"));
	}
	if (LocalModelGrid)
	{
		OnLocalModelGridUpdated_Handle = LocalModelGrid->OnModelGridReplaced().AddUObject(this, &UGSModelGridComponent::OnLocalModelGridUpdated);
	}

	UpdateGridAssetListener();

	if (ensure(LocalModelGridMaterials != nullptr) == false)
	{
		LocalModelGridMaterials = NewObject<UGSGridMaterialSet>(this, TEXT("GridMaterials"));
	}

	// UGSModelGrid::PostLoad publishes a grid-modified event on load, but it will have loaded before we got
	// a chance to register listeners in the UpdateGridAssetListener() call. So we will publish a grid-modified
	// event again (otherwise initial mesh/etc will not be generated).
	// Note that non-asset path does not require this because it will not be loaded until after it's owning 
	// Component has been loaded
	if (GetGridAssetIfEnabled() != nullptr)
	{
		NotifyCurrentActiveGridModified();
	}
}


void UGSModelGridComponent::ForceUpdateConnections()
{
	UpdateGridAssetListener();
}
void UGSModelGridComponent::PostEditImport()
{
	ForceUpdateConnections();
}

#if WITH_EDITOR
void UGSModelGridComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();
	if (PropName == GET_MEMBER_NAME_CHECKED(UGSModelGridComponent, GridObjectAsset))
	{
		if (UpdateGridAssetListener())
			NotifyCurrentActiveGridModified();
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UGSModelGridComponent, bIgnoreAsset))
	{
		if (UpdateGridAssetListener())
			NotifyCurrentActiveGridModified();
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UGSModelGridComponent, GridCollisionMode))
	{
		InvalidateGridCollider();
		UpdateGridCollider();
	}
}

void UGSModelGridComponent::PostEditUndo()
{
	Super::PostEditUndo();

	if (UpdateGridAssetListener())
		NotifyCurrentActiveGridModified();
}
#endif


void UGSModelGridComponent::SetGridAsset(UGSModelGridAsset* NewGridAsset)
{
	if (GridObjectAsset == NewGridAsset) return;
	GridObjectAsset = NewGridAsset;

	if (UpdateGridAssetListener())
		NotifyCurrentActiveGridModified();
}


UGSModelGrid* UGSModelGridComponent::GetGrid() const
{
	// TODO some kinda config options?
	if (UGSModelGridAsset* UseAsset = GetGridAssetIfEnabled())
	{
		return UseAsset->ModelGrid;
	}
	return LocalModelGrid;
}

UGSModelGrid* UGSModelGridComponent::GetComponentGrid() const
{
	return LocalModelGrid;
}


UGSGridMaterialSet* UGSModelGridComponent::GetGridMaterials() const
{
	if (UGSModelGridAsset* UseAsset = GetGridAssetIfEnabled())
	{
		return UseAsset->InternalMaterialSet;
	}
	return LocalModelGridMaterials;
}

void UGSModelGridComponent::OnLocalModelGridUpdated(UGSModelGrid* ModelGrid)
{
	check(ModelGrid == LocalModelGrid);
	if (GetGridAssetIfEnabled() == nullptr)
	{
		NotifyModelGridModification(ModelGrid);
	}
}

void UGSModelGridComponent::OnModelGridAssetUpdated(UGSModelGrid* ModelGrid)
{
	check(ModelGrid == GridObjectAsset->ModelGrid);
	if (GetGridAssetIfEnabled()) 
	{
		NotifyModelGridModification(ModelGrid);
	}
}

void UGSModelGridComponent::NotifyModelGridModification(UGSModelGrid* ModelGrid)
{
	// force-wait for collider update, for now
	InvalidateGridCollider();
	UpdateGridCollider();

	ModelGridChangedEvent.Broadcast(this, ModelGrid);
}


bool UGSModelGridComponent::UpdateGridAssetListener()
{
	UGSModelGridAsset* CurGridAsset = GetGridAssetIfEnabled();

	UGSModelGridAsset* PrevGridAsset = LastTargetAsset.IsValid() ? LastTargetAsset.Get() : nullptr;
	if (PrevGridAsset == CurGridAsset) return false;

	if (PrevGridAsset != nullptr)
	{
		if (OnModelGridAssetUpdated_Handle.IsValid())
		{
			if (PrevGridAsset->ModelGrid)
				PrevGridAsset->ModelGrid->OnModelGridReplaced().Remove(OnModelGridAssetUpdated_Handle);
			OnModelGridAssetUpdated_Handle.Reset();
		}
	}

	if (CurGridAsset != nullptr)
	{
		if (GridObjectAsset->ModelGrid)
			OnModelGridAssetUpdated_Handle = GridObjectAsset->ModelGrid->OnModelGridReplaced().AddUObject(this, &UGSModelGridComponent::OnModelGridAssetUpdated);
	}

	LastTargetAsset = CurGridAsset;
	return true;
}

void UGSModelGridComponent::NotifyCurrentActiveGridModified()
{
	if (GetGridAssetIfEnabled())
	{
		OnModelGridAssetUpdated(GridObjectAsset->ModelGrid);
	}
	else
	{
		OnLocalModelGridUpdated(LocalModelGrid);
	}

	InvalidateGridCollider();
	UpdateGridCollider();
}


void UGSModelGridComponent::InvalidateGridCollider()
{
	GridCollider.Reset();
}

void UGSModelGridComponent::UpdateGridCollider()
{
	if (GridCollisionMode == EModelGridComponentCollisionMode::GridCollider)
	{
		GridCollider = MakePimpl<GS::ModelGridCollider>();
		GetGrid()->ProcessGrid([&](const GS::ModelGrid& Grid) {
			GridCollider->Initialize(Grid);
			GS::AxisBox3i ModifiedCellBounds = Grid.GetModifiedRegionBounds(0);
			GS::AxisBox3d UpdateBox = Grid.GetCellLocalBounds(ModifiedCellBounds.Min);
			UpdateBox.Contain(Grid.GetCellLocalBounds(ModifiedCellBounds.Max));
			GridCollider->UpdateInBounds(Grid, UpdateBox);
		});
	}
}

void UGSModelGridComponent::SetGridCollisionMode(EModelGridComponentCollisionMode NewMode)
{
	if (GridCollisionMode != NewMode) {
		GridCollisionMode = NewMode;
		InvalidateGridCollider();
		UpdateGridCollider();
	}
}

void UGSModelGridComponent::ProcessGridCollider(TFunctionRef<void(const GS::ModelGridCollider&)> ProcessFunc) const
{
	if (GridCollisionMode == EModelGridComponentCollisionMode::GridCollider && GridCollider.IsValid())
	{
		ProcessFunc(*GridCollider);
	}
}