// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/CreateModelGridTool_Editor.h"

#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"
#include "ModelingObjectsCreationAPI.h"
#include "EditorModelingObjectsCreationAPI.h"
#include "Selection/ToolSelectionUtil.h"

#include "GridActor/ModelGridActor.h"
#include "GridActor/UGSModelGrid.h"
#include "GridActor/UGSModelGridAsset.h"
#include "GridActor/UGSModelGridAssetUtils.h"

#include "Engine/World.h"
#include "Editor/EditorEngine.h"

#define LOCTEXT_NAMESPACE "UCreateModelGridToolEditor"

using namespace GS;
using namespace UE::Geometry;


UCreateModelGridTool* UCreateModelGridToolEditorBuilder::CreateNewToolOfType(const FToolBuilderState& SceneState) const
{
	return NewObject<UCreateModelGridToolEditor>(SceneState.ToolManager);
}

void UCreateModelGridToolEditor::OnCreateNewModelGrid(GS::ModelGrid&& SourceGrid)
{
	FString UseName = Settings->Name;
	if (UseName.Len() == 0)
		UseName = TEXT("ModelGrid");

	// create new asset if requested
	UGSModelGridAsset* NewGridAsset = nullptr;
	if (Settings->bCreateAsset)
	{
		UModelingObjectsCreationAPI* ModelingCreationAPI = GetToolManager()->GetContextObjectStore()->FindContext<UModelingObjectsCreationAPI>();
		UEditorModelingObjectsCreationAPI* EditorAPI = Cast<UEditorModelingObjectsCreationAPI>(ModelingCreationAPI);
		FString NewAssetPath;
		ECreateModelingObjectResult PathResult = EditorAPI->GetNewAssetPath(NewAssetPath, UseName, nullptr, GetTargetWorld());
		if (PathResult == ECreateModelingObjectResult::Ok)
		{
			NewGridAsset = GS::CreateModelGridAssetFromGrid(
				NewAssetPath,
				MoveTemp(SourceGrid));
		}
		if (!ensure(NewGridAsset))
			UE_LOG(LogTemp, Warning, TEXT("UCreateModelGridTool::Shutdown: failed to create new Grid Asset"));
	}

	GetToolManager()->BeginUndoTransaction(LOCTEXT("ShutdownEdit", "Create ModelGrid"));

	FActorSpawnParameters SpawnInfo;
	AGSModelGridActor* NewActor = GetTargetWorld()->SpawnActor<AGSModelGridActor>(SpawnInfo);
	if (NewActor)
	{
		if (NewGridAsset)
		{
			NewActor->GetGridComponent()->SetGridAsset(NewGridAsset);
		}
		else
		{
			NewActor->GetGrid()->EditGrid([&](ModelGrid& EditGrid)
			{
				EditGrid = SourceGrid;
			});
		}

		NewActor->SetActorTransform(GridFrame.ToFTransform());
		FActorLabelUtilities::SetActorLabelUnique(NewActor, UseName);

		NewActor->PostEditChange();

		ToolSelectionUtil::SetNewActorSelection(GetToolManager(), NewActor);
	}

	GetToolManager()->EndUndoTransaction();
}

#undef LOCTEXT_NAMESPACE