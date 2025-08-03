// Copyright Gradientspace Corp. All Rights Reserved.
#include "GridActor/UGSModelGridAssetUtils.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Misc/ITransaction.h"


using namespace GS;

UGSModelGridAsset* GS::CreateModelGridAssetFromGrid(
	const FString& NewAssetPath,
	ModelGrid&& NewGrid)
{
	// This is a horrible hack to workaround issues where doing an 'undo' across calls to this function will invalidate the asset internals.
	// Currently necessary because Asset creation is not transacted
	ITransaction* UndoState = GUndo;
	GUndo = nullptr;
	ON_SCOPE_EXIT{ GUndo = UndoState; };

	FString NewObjectName = FPackageName::GetLongPackageAssetName(NewAssetPath);
	UPackage* NewAssetPackage = CreatePackage(*NewAssetPath);
	if (ensure(NewAssetPackage != nullptr) == false)
		return nullptr;

	EObjectFlags UseFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
	UGSModelGridAsset* NewAsset = NewObject<UGSModelGridAsset>(NewAssetPackage, FName(*NewObjectName), UseFlags);
	if (!ensure(NewAsset))
		return nullptr;

	NewAsset->ModelGrid = NewObject<UGSModelGrid>(NewAsset);

	NewAsset->ModelGrid->InitializeNewGrid(false);
	NewAsset->ModelGrid->SetEnableTransactions(true);
	NewAsset->ModelGrid->EditGrid([&](ModelGrid& EditGrid)
	{
		EditGrid = MoveTemp(NewGrid);
	});

	[[maybe_unused]] bool MarkedDirty = NewAsset->MarkPackageDirty();
#if WITH_EDITOR
	NewAsset->PostEditChange();
#endif

	return NewAsset;
}

