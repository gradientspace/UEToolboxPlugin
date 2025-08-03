// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/ModelGridUtilityTool.h"

#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"
#include "ToolSetupUtil.h"
#include "ModelingObjectsCreationAPI.h"
#include "EditorModelingObjectsCreationAPI.h"

#include "GridActor/ModelGridActor.h"
#include "GridActor/UGSModelGrid.h"
#include "GridActor/UGSModelGridAsset.h"
#include "ModelGrid/ModelGrid.h"

#include "Misc/PackageName.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "UModelGridUtilityTool"

using namespace GS;


bool UModelGridUtilityToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	if (SceneState.SelectedActors.Num() == 1)
	{
		return (Cast<AGSModelGridActor>(SceneState.SelectedActors[0]) != nullptr);
	}
	return false;
}

UInteractiveTool* UModelGridUtilityToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UModelGridUtilityTool* Tool = NewObject<UModelGridUtilityTool>(SceneState.ToolManager);
	Tool->SetTargetWorld(SceneState.World);

	if (SceneState.SelectedActors.Num() == 1)
	{
		AGSModelGridActor* Actor = Cast<AGSModelGridActor>(SceneState.SelectedActors[0]);
		if (Actor)
		{
			Tool->SetExistingActor(Actor);
		}
	}

	return Tool;
}




void UModelGridUtilityTool::SetTargetWorld(UWorld* World)
{
	TargetWorld = World;
}

UWorld* UModelGridUtilityTool::GetTargetWorld()
{
	return TargetWorld.Get();
}

void UModelGridUtilityTool::SetExistingActor(AGSModelGridActor* Actor)
{
	ExistingActor = Actor;
}



void UModelGridUtilityTool::Setup()
{
	if (ExistingActor != nullptr)
	{
		TargetInfo = NewObject<UModelGridUtilityTool_TargetInfo>(this);
		AddToolPropertySource(TargetInfo);

		AssetFunctionsProps = NewObject<UModelGridUtilityTool_AssetFunctions>(this);
		AssetFunctionsProps->Tool = this;
		AddToolPropertySource(AssetFunctionsProps);
	}

	UpdateTargetInfo();
}


void UModelGridUtilityTool_AssetFunctions::PostAction(EModelGridUtilityToolActions Action)
{
	if (Tool.IsValid())	{
		Tool->RequestAction(Action);
	}
}
void UModelGridUtilityTool::RequestAction(EModelGridUtilityToolActions Action)
{
	if (!bHavePendingAction)
	{
		PendingAction = Action;
		bHavePendingAction = true;
	}
}

void UModelGridUtilityTool::OnTick(float DeltaTime)
{
	static int TickCounter = 0;

	if (bHavePendingAction)
	{
		ApplyAction(PendingAction);
		bHavePendingAction = false;
		PendingAction = EModelGridUtilityToolActions::NoAction;
	}

	UpdateTargetInfo();
}




namespace Local
{

FString MakeInfoString(UGSModelGrid* Grid)
{
	FString Result;
	Grid->ProcessGrid([&](const ModelGrid& Grid)
	{
		AxisBox3i ModifiedRange = Grid.GetModifiedRegionBounds(0);
		Result = FString::Printf(TEXT("%dx%dx%d"),
			ModifiedRange.CountX(), ModifiedRange.CountY(), ModifiedRange.CountZ());
	});
	return Result;
}

}

void UModelGridUtilityTool::UpdateTargetInfo()
{
	if (!TargetInfo) return;

	if (ExistingActor.IsValid() == false) return;
	AGSModelGridActor* GridActor = ExistingActor.Get();
	UGSModelGridComponent* Component = GridActor->GetGridComponent();

	TargetInfo->ComponentGrid = Local::MakeInfoString(Component->GetComponentGrid());

	TargetInfo->bTargetHasAssetAssigned = Component->HasGridAsset();
	if (TargetInfo->bTargetHasAssetAssigned)
	{
		UGSModelGridAsset* GridAsset = Component->GetGridAsset();
		TargetInfo->AssetGrid = Local::MakeInfoString(GridAsset->ModelGrid);
		TargetInfo->IsAssetOverridden = Component->bIgnoreAsset ? TEXT("YES") : TEXT("NO");
		TargetInfo->AssignedAsset = GridAsset;
	}
	else
	{
		TargetInfo->AssetGrid = TEXT("(NONE)");
	}
}




void UModelGridUtilityTool::ApplyAction(EModelGridUtilityToolActions ActionType)
{
	switch (ActionType)
	{
	case EModelGridUtilityToolActions::CopyToNewAsset:
		SaveAsAsset(false); break;
	case EModelGridUtilityToolActions::SetToNewAsset:
		SaveAsAsset(true); break;
	case EModelGridUtilityToolActions::CopyAssetToComponent:
		CopyAssetToComponent(); break;
	case EModelGridUtilityToolActions::RemoveAsset:
		RemoveAsset(); break;
	case EModelGridUtilityToolActions::ClearComponent:
		ClearComponent(); break;
	case EModelGridUtilityToolActions::CopyFromSelected:
		CopyFromSelected(); break;
	}

	UpdateTargetInfo();
}



void UModelGridUtilityTool::SaveAsAsset(bool bUpdateTargetActor)
{
	if (ExistingActor.IsValid() == false) return;
	AGSModelGridActor* GridActor = ExistingActor.Get();

	UGSModelGrid* SourceGrid = GridActor->GetGrid();
	if (!SourceGrid)
		return;

	UModelingObjectsCreationAPI* UseAPI = GetToolManager()->GetContextObjectStore()->FindContext<UModelingObjectsCreationAPI>();
	UEditorModelingObjectsCreationAPI* EditorAPI = Cast<UEditorModelingObjectsCreationAPI>(UseAPI);

	FString NewAssetPath;
	ECreateModelingObjectResult PathResult = EditorAPI->GetNewAssetPath(NewAssetPath, "GridObject", nullptr, GetTargetWorld());
	if (PathResult != ECreateModelingObjectResult::Ok)
		return;

	ITransaction* UndoState = GUndo;
	GUndo = nullptr; // Pretend we're not in a transaction
	ON_SCOPE_EXIT{ GUndo = UndoState; }; // Revert

	FString NewObjectName = FPackageName::GetLongPackageAssetName(NewAssetPath);
	UPackage* NewAssetPackage = CreatePackage(*NewAssetPath);
	if (ensure(NewAssetPackage != nullptr) == false)
		return;

	EObjectFlags UseFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
	UGSModelGridAsset* NewAsset = NewObject<UGSModelGridAsset>(NewAssetPackage, FName(*NewObjectName), UseFlags);
	if (!ensure(NewAsset))
		return;

	NewAsset->ModelGrid = NewObject<UGSModelGrid>(NewAsset);

	NewAsset->ModelGrid->InitializeNewGrid(false);
	NewAsset->ModelGrid->SetEnableTransactions(true);
	SourceGrid->ProcessGrid([&](const ModelGrid& SourceGrid)
	{
		NewAsset->ModelGrid->EditGrid([&](ModelGrid& EditGrid)
		{
			EditGrid = SourceGrid;
		});
	});

	[[maybe_unused]] bool MarkedDirty = NewAsset->MarkPackageDirty();
	NewAsset->PostEditChange();

	// set new asset in Target
	if (bUpdateTargetActor)
	{
		UGSModelGridComponent* Component = GridActor->GetGridComponent();
		GetToolManager()->BeginUndoTransaction(LOCTEXT("SaveAsAsset", "Update Assigned Asset"));

		Component->Modify();
		Component->SetGridAsset(NewAsset);
		Component->PostEditChange();

		GetToolManager()->EndUndoTransaction();
	}
}



void UModelGridUtilityTool::CopyAssetToComponent()
{
	if (ExistingActor.IsValid() == false) return;
	AGSModelGridActor* GridActor = ExistingActor.Get();

	UGSModelGridComponent* Component = GridActor->GetGridComponent();
	if (Component->HasGridAsset() == false)
	{
		GetToolManager()->DisplayMessage( LOCTEXT("NoAssetMessage", "Target Actor has no ModelGrid Asset assigned"), EToolMessageLevel::UserError);
		return;
	}
	UGSModelGridAsset* GridAsset = Component->GetGridAsset();
	UGSModelGrid* ComponentGrid = Component->GetComponentGrid();

	GetToolManager()->BeginUndoTransaction(LOCTEXT("CopyAssetToComponent", "Copy Asset Grid To Component"));

	ComponentGrid->SetEnableTransactions(true);
	ComponentGrid->Modify();
	Component->Modify();

	GridAsset->ModelGrid->ProcessGrid([&](const ModelGrid& SourceGrid)
	{
		Component->GetComponentGrid()->EditGrid([&](ModelGrid& TargetGrid)
		{
			TargetGrid = SourceGrid;
		});
	});

	ComponentGrid->PostEditChange();

	GetToolManager()->EndUndoTransaction();
}



void UModelGridUtilityTool::RemoveAsset()
{
	if (ExistingActor.IsValid() == false) return;
	AGSModelGridActor* GridActor = ExistingActor.Get();

	UGSModelGridComponent* Component = GridActor->GetGridComponent();
	if (Component->HasGridAsset() == false)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("NoAssetMessage", "Target Actor has ModelGrid Asset assigned"), EToolMessageLevel::UserError);
		return;
	}

	GetToolManager()->BeginUndoTransaction(LOCTEXT("RemoveAsset", "Clear Assigned Asset"));

	Component->Modify();
	Component->SetGridAsset(nullptr);
	Component->PostEditChange();

	GetToolManager()->EndUndoTransaction();
}



void UModelGridUtilityTool::ClearComponent()
{
	if (ExistingActor.IsValid() == false) return;
	AGSModelGridActor* GridActor = ExistingActor.Get();

	UGSModelGridComponent* Component = GridActor->GetGridComponent();
	UGSModelGrid* ComponentGrid = Component->GetComponentGrid();

	GetToolManager()->BeginUndoTransaction(LOCTEXT("ClearComponent", "Clear Component ModelGrid"));

	ComponentGrid->SetEnableTransactions(true);
	ComponentGrid->Modify();
	Component->Modify();

	Component->GetComponentGrid()->EditGrid([&](ModelGrid& TargetGrid)
	{
		ModelGrid NewGrid;
		NewGrid.Initialize(TargetGrid.GetCellDimensions());
		TargetGrid = NewGrid;
	});

	ComponentGrid->PostEditChange();

	GetToolManager()->EndUndoTransaction();
}


void UModelGridUtilityTool::CopyFromSelected()
{
	if (ExistingActor.IsValid() == false) return;
	AGSModelGridActor* GridActor = ExistingActor.Get();
	UGSModelGridComponent* Component = GridActor->GetGridComponent();

	// todo: could support component selection...
	AGSModelGridActor* SourceGridActor = nullptr;
	USelection* SelectedActors = GEditor->GetSelectedActors();
	for (int i = 0; i < SelectedActors->Num() && SourceGridActor == nullptr; ++i) {
		if (AGSModelGridActor* Actor = Cast<AGSModelGridActor>(SelectedActors->GetSelectedObject(i)))
			SourceGridActor = Actor;
	}
	if (SourceGridActor == nullptr) {
		GetToolManager()->DisplayMessage(LOCTEXT("NoSourceGrid", "No Source ModelGridActor is selected"), EToolMessageLevel::UserError);
		return;
	}
	if (SourceGridActor == GridActor) {
		GetToolManager()->DisplayMessage(LOCTEXT("SameSourceGrid", "A different ModelGridActor must be selected in the Outliner"), EToolMessageLevel::UserError);
		return;
	}

	UGSModelGrid* SourceGrid = SourceGridActor->GetGrid();
	UGSModelGrid* TargetGrid = GridActor->GetGrid();
	if (SourceGrid == nullptr || TargetGrid == nullptr)
		return;

	GetToolManager()->BeginUndoTransaction(LOCTEXT("CopyFromSelected", "Transfer Grid"));

	TargetGrid->SetEnableTransactions(true);
	TargetGrid->Modify();
	Component->Modify();

	SourceGrid->ProcessGrid([&](const ModelGrid& SourceGrid)
	{
		TargetGrid->EditGrid([&](ModelGrid& TargetGrid)
		{
			TargetGrid = SourceGrid;
		});
	});

	TargetGrid->PostEditChange();

	GetToolManager()->EndUndoTransaction();
}

#undef LOCTEXT_NAMESPACE 
