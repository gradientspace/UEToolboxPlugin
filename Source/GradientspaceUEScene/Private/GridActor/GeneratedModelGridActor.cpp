// Copyright Gradientspace Corp. All Rights Reserved.

#include "GridActor/GeneratedModelGridActor.h"
#include "GSGenerationSubsystem.h"
#include "UObject/Package.h"
#include "Engine/Level.h"
#include "GradientspaceUELogging.h"

AGSGeneratedModelGridActor::~AGSGeneratedModelGridActor()
{
	UnregisterWithGenerationManager();
}


void AGSGeneratedModelGridActor::MarkForRebuild()
{
	bRegenerationPending = true;
}


void AGSGeneratedModelGridActor::RebuildImmediately(bool bEvenIfPaused)
{
	bRegenerationPending = true;
	ExecuteRegenerateIfPending(bEvenIfPaused);
}


void AGSGeneratedModelGridActor::ExecuteRegenerateIfPending(bool bOverridePause)
{
	if (bRegenerationPending == false)
		return;
	if (bPauseRegeneration && bOverridePause == false)
		return;

	// make sure we are in a level and still an active actor
	if (IsUnreachable())
		return;
	if (GetLevel() == nullptr)
		return;

	UGSModelGridComponent* Component = GetGridComponent();
	if (Component == nullptr)
		return;

	// dangerous to generate into Asset grid unless user has explicitly said it is OK
	UGSModelGrid* GridToEdit = nullptr;
	if ( Component->HasGridAsset() && Component->bIgnoreAsset == false ) {
		if (bAllowAssetGridModification == false) {
			UE_LOG(LogGradientspace, Warning, TEXT("[GSGeneratedModelGridActor] tried to generate grid into assigned Asset %s but bAllowAssetGridModification == false. Falling back to Component grid."),
				*Component->GetGridAsset()->GetName());
			GridToEdit = Component->GetComponentGrid();
		}
		else
		{
			GridToEdit = Component->GetGrid();
		}
	}
	else {
		GridToEdit = Component->GetComponentGrid();
	}
	if (GridToEdit == nullptr)
		return;

	// run the edit
	GridToEdit->BeginGridEdits();
	{
		if (bClearGridBeforeRegenerate)
			GridToEdit->ResetGrid();

		FEditorScriptExecutionGuard Guard;
		OnRegenerateModelGrid(GridToEdit);

		bRegenerationPending = false;
	}
	GridToEdit->EndGridEdits();
}





void AGSGeneratedModelGridActor::RegisterWithGenerationManager()
{
	if (IsValid(this) == false)
		return;
	if (HasAnyFlags(RF_ClassDefaultObject))
		return;
	if (GetPackage()->GetPIEInstanceID() != INDEX_NONE)
		return;
	if (bIsRegisteredWithGenerationManager)
		return;
	if (UGSGenerationSubsystem::RegisterGeneratedActor(this))
		bIsRegisteredWithGenerationManager = true;
}
void AGSGeneratedModelGridActor::UnregisterWithGenerationManager()
{
	if (!bIsRegisteredWithGenerationManager)
		return;
	UGSGenerationSubsystem::UnregisterGeneratedActor(this);
	bIsRegisteredWithGenerationManager = false;
}


void AGSGeneratedModelGridActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	bRegenerationPending = true;
}

void AGSGeneratedModelGridActor::PostLoad()
{
	Super::PostLoad();
	RegisterWithGenerationManager();
}

void AGSGeneratedModelGridActor::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	RegisterWithGenerationManager();
}

void AGSGeneratedModelGridActor::PostActorCreated()
{
	Super::PostActorCreated();
	RegisterWithGenerationManager();
}

void AGSGeneratedModelGridActor::Destroyed()
{
	UnregisterWithGenerationManager();
	Super::Destroyed();
}

void AGSGeneratedModelGridActor::PreRegisterAllComponents()
{
	Super::PreRegisterAllComponents();

	// apparently this will handle changing Actor visibility...
	if (const ULevel* Level = GetLevel()) {
		if (Level->bIsAssociatingLevel)
			RegisterWithGenerationManager();
	}
}

void AGSGeneratedModelGridActor::PostUnregisterAllComponents()
{
	// apparently this will handle changing Actor visibility...
	if (const ULevel* Level = GetLevel()) {
		if (Level->bIsDisassociatingLevel)
			UnregisterWithGenerationManager();
	}

	Super::PostUnregisterAllComponents();
}


#if WITH_EDITOR

void AGSGeneratedModelGridActor::PostEditUndo()
{
	Super::PostEditUndo();

	// attempt to detect if this actor went out of existence...
	if (IsActorBeingDestroyed() || IsValid(this) == false || GetLevel() == nullptr )
		UnregisterWithGenerationManager();
	else
		RegisterWithGenerationManager();
}

#endif

