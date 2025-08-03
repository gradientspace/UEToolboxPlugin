// Copyright Gradientspace Corp. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/HotSpotTool.h"
#include "InteractiveToolManager.h"
#include "ToolTargetManager.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"
#include "ModelingToolTargetUtil.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Polygroups/PolygroupUtil.h"

#include "MeshOpPreviewHelpers.h" // UMeshOpPreviewWithBackgroundCompute

#include "Selections/MeshFaceSelection.h"
#include "Selections/GeometrySelectionUtil.h"
#include "DynamicSubmesh3.h"
#include "Operations/MeshRegionOperator.h"

#include "Selections/MeshConnectedComponents.h"
#include "Parameterization/DynamicMeshUVEditor.h"
#include "MeshBoundaryLoops.h"

#include "Engine/StaticMesh.h"

#include "Operators/HotSpotUVOp.h"
#include "MeshDescriptionToDynamicMesh.h"

//#include UE_INLINE_GENERATED_CPP_BY_NAME(HotSpotTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UHotSpotTool"



/*
 * ToolBuilder
 */

USingleTargetWithSelectionTool* UHotSpotToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHotSpotTool>(SceneState.ToolManager);
}

bool UHotSpotToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return USingleTargetWithSelectionToolBuilder::CanBuildTool(SceneState) &&
		SceneState.TargetManager->CountSelectedAndTargetableWithPredicate(SceneState, GetTargetRequirements(),
			[](UActorComponent& Component) { return ToolBuilderUtil::ComponentTypeCouldHaveUVs(Component); }) > 0;
}



/*
 * Tool
 */



static void ExtractHotSpots(FDynamicMesh3& SourceMesh, FHotSpotSet& HotSpots)
{
	HotSpots.Patches.Reset();

	FDynamicMeshUVEditor UVEditor(&SourceMesh, 0, true);
	FDynamicMeshUVOverlay* UseOverlay = UVEditor.GetOverlay();
	FMeshConnectedComponents UVComponents(&SourceMesh);
	UVComponents.FindConnectedTriangles([&](int32 Triangle0, int32 Triangle1) {
		return UseOverlay->AreTrianglesConnected(Triangle0, Triangle1);
	});

	HotSpots.Patches.Reserve(UVComponents.Num());
	for (FMeshConnectedComponents::FComponent& UVRegion : UVComponents)
	{
		FHotSpotSet::FHotSpotPatch& Patch = HotSpots.Patches.AddDefaulted_GetRef();
		Patch.UVBounds = FAxisAlignedBox2d::Empty();

		FDynamicSubmesh3 Submesh(&SourceMesh, UVRegion.Indices);
		FDynamicMesh3& UVMesh = Submesh.GetSubmesh();
		FDynamicMeshUVOverlay* UVMeshUVs = UVMesh.Attributes()->GetUVLayer(0);
		TArray<FVector2d> VertexUVs;
		VertexUVs.SetNum(UVMesh.MaxVertexID());
		for (int32 tid : UVMesh.TriangleIndicesItr())
		{
			FIndex3i Triangle = UVMesh.GetTriangle(tid);
			FIndex3i UVTriangle = UVMeshUVs->GetTriangle(tid);
			for (int32 j = 0; j < 3; ++j)
			{
				VertexUVs[Triangle[j]] = (FVector2d)UVMeshUVs->GetElement(UVTriangle[j]);
				Patch.UVBounds.Contain(VertexUVs[Triangle[j]]);
			}
		}

		FMeshBoundaryLoops BoundaryLoops(&UVMesh, true);
		FEdgeLoop Loop = BoundaryLoops[BoundaryLoops.GetLongestLoopIndex()];
		
		FVector2d UVCenter = Patch.UVBounds.Center();
		for (int32 vid : Loop.Vertices)
		{
			Patch.ShapeBoundary.AppendVertex( VertexUVs[vid] - UVCenter );
		}
		Patch.ShapeBounds = Patch.ShapeBoundary.Bounds();
	}

}



class FHotSpotUVOpFactory : public UE::Geometry::IDynamicMeshOperatorFactory
{
public:
	UHotSpotTool* SourceTool;
	FHotSpotSet HotSpots;

	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<FDynamicMeshOperator> MakeNewOperator() override
	{
		TUniquePtr<FHotSpotUVOp> NewOperator = MakeUnique<FHotSpotUVOp>();
		NewOperator->OriginalMeshShared = SourceTool->EditRegionSharedMesh;
		if (SourceTool->TriangleSelection.Num() > 0)
		{
			NewOperator->TriangleSelection = SourceTool->TriangleSelection;
		}

		NewOperator->WorldTransform = SourceTool->WorldTransform;

		NewOperator->HotSpots = HotSpots;

		NewOperator->Debug_ShiftSelection = SourceTool->Settings->SelectionShift;
		NewOperator->ScaleFactor = SourceTool->Settings->ScaleFactor;

		//NewOperator->InputGroups = InputGroups;
		//NewOperator->UVLayer = GetSelectedUVChannel();

		NewOperator->SetResultTransform(SourceTool->WorldTransform);

		return NewOperator;
	}


	virtual void UpdateHotSpots(UStaticMesh* SourceMesh)
	{
		HotSpots.Patches.Reset();

		if (!SourceMesh) { return; }

		FMeshDescription* MeshDescription = SourceMesh->GetMeshDescription(0);
		FDynamicMesh3 NewMesh;
		FMeshDescriptionToDynamicMesh Converter;
		Converter.Convert(MeshDescription, NewMesh, false);

		ExtractHotSpots(NewMesh, HotSpots);
	}
};


void UHotSpotTool::Setup()
{
	UInteractiveTool::Setup();

	UToolTarget* ActiveTarget = Super::GetTarget();
	InputMesh = UE::ToolTarget::GetDynamicMeshCopy(ActiveTarget);

	WorldTransform = UE::ToolTarget::GetLocalToWorldTransform(ActiveTarget);
	FComponentMaterialSet MaterialSet = UE::ToolTarget::GetMaterialSet(ActiveTarget);

	// initialize our properties

	Settings = NewObject<UHotSpotToolProperties>(this);
	Settings->RestoreProperties(this);
	Settings->WatchProperty(Settings->bForceUpdate, [&](bool) { EditCompute->InvalidateResult(); });
	Settings->WatchProperty(Settings->SelectionShift, [&](int) { EditCompute->InvalidateResult(); });
	Settings->WatchProperty(Settings->ScaleFactor, [&](int) { EditCompute->InvalidateResult(); });
	AddToolPropertySource(Settings);

	UVChannelProperties = NewObject<UMeshUVChannelProperties>(this);
	UVChannelProperties->RestoreProperties(this);
	UVChannelProperties->Initialize(&InputMesh, false);
	UVChannelProperties->ValidateSelection(true);
	UVChannelProperties->WatchProperty(UVChannelProperties->UVChannel, [this](const FString& NewValue)
	{
		MaterialSettings->UpdateUVChannels(UVChannelProperties->UVChannelNamesList.IndexOfByKey(UVChannelProperties->UVChannel),
			                                UVChannelProperties->UVChannelNamesList);
	});
	AddToolPropertySource(UVChannelProperties);

	PolygroupLayerProperties = NewObject<UPolygroupLayersProperties>(this);
	PolygroupLayerProperties->RestoreProperties(this, TEXT("HotSpotTool"));
	PolygroupLayerProperties->InitializeGroupLayers(&InputMesh);
	PolygroupLayerProperties->WatchProperty(PolygroupLayerProperties->ActiveGroupLayer, [&](FName) { OnSelectedGroupLayerChanged(); });
	AddToolPropertySource(PolygroupLayerProperties);
	UpdateActiveGroupLayer();

	MaterialSettings = NewObject<UExistingMeshMaterialProperties>(this);
	MaterialSettings->MaterialMode = ESetMeshMaterialMode::Checkerboard;
	MaterialSettings->RestoreProperties(this, TEXT("HotSpotTool"));
	AddToolPropertySource(MaterialSettings);

	UE::ToolTarget::HideSourceObject(ActiveTarget);

	// force update
	MaterialSettings->UpdateMaterials();


	// get current selection if there is one
	FMeshFaceSelection TriSelection(&InputMesh);
	const FGeometrySelection& Selection = GetGeometrySelection();
	UE::Geometry::EnumerateSelectionTriangles(Selection, InputMesh,
		[&](int32 tid) { TriSelection.Select(tid); }, nullptr  );
	TriangleSelection = TriSelection.AsArray();

	TriSelection.ExpandToOneRingNeighbours(2);
	ModifiedROI = TriSelection.AsSet();


	// if selection is not empty, create display for source area
	if (TriangleSelection.Num() > 0)
	{
		SourcePreview = NewObject<UPreviewMesh>();
		SourcePreview->CreateInWorld(GetTargetWorld(), WorldTransform);
		ToolSetupUtil::ApplyRenderingConfigurationToPreview(SourcePreview, ActiveTarget);
		SourcePreview->SetMaterials(MaterialSet.Materials);
		SourcePreview->EnableSecondaryTriangleBuffers( 
			[this](const FDynamicMesh3* Mesh, int32 TriangleID) { return TriangleSelection.Contains(TriangleID);}  );
		SourcePreview->SetSecondaryBuffersVisibility(false);
		SourcePreview->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
		SourcePreview->UpdatePreview(&InputMesh);

		// initialize a region operator for the modified area
		RegionOperator = MakePimpl<FMeshRegionOperator>(&InputMesh, ModifiedROI.Array());
		EditRegionMesh = RegionOperator->Region.GetSubmesh();
		EditRegionSharedMesh = MakeShared<FSharedConstDynamicMesh3>(&EditRegionMesh);
	}
	else
	{
		EditRegionMesh = InputMesh;
		EditRegionSharedMesh = MakeShared<FSharedConstDynamicMesh3>(&EditRegionMesh);
	}


	this->OperatorFactory = MakePimpl<FHotSpotUVOpFactory>();
	this->OperatorFactory->SourceTool = this;


	// Create the preview compute for the extrusion operation
	EditCompute = NewObject<UMeshOpPreviewWithBackgroundCompute>(this);
	EditCompute->Setup(GetTargetWorld(), this->OperatorFactory.Get());
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(EditCompute->PreviewMesh, ActiveTarget);
	EditCompute->ConfigureMaterials(MaterialSet.Materials, nullptr, nullptr);
	//Preview->ConfigureMaterials(MaterialSet.Materials, ToolSetupUtil::GetDefaultWorkingMaterial(GetToolManager()));
	EditCompute->PreviewMesh->SetTransform((FTransform)WorldTransform);

	// is this the right tangents behavior?
	EditCompute->PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
	EditCompute->PreviewMesh->UpdatePreview(&EditRegionMesh);
	EditCompute->SetVisibility(true);


	// hide input Component
	UE::ToolTarget::HideSourceObject(ActiveTarget);


	Settings->WatchProperty(Settings->Source, [this](UStaticMesh* NewValue) { OnHotSpotSourceUpdated();	});
	OnHotSpotSourceUpdated();


	//SetToolDisplayName(LOCTEXT("ToolNameLocal", "UV Unwrap"));
	//GetToolManager()->DisplayMessage(
	//	LOCTEXT("OnStartTool_Regions", "Generate UVs for PolyGroups or existing UV islands of the mesh using various strategies."),
	//	EToolMessageLevel::UserNotification);
}



void UHotSpotTool::OnHotSpotSourceUpdated()
{
	OperatorFactory->UpdateHotSpots(Settings->Source);

	if (Settings->Source != nullptr)
	{
		TArray<FStaticMaterial> Materials = Settings->Source->GetStaticMaterials();
		ActiveHotSpotMaterial = Materials[0].MaterialInterface;
		EditCompute->ConfigureMaterials(ActiveHotSpotMaterial, nullptr);
	}

	EditCompute->InvalidateResult();
}

void UHotSpotTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	//bool bForceMaterialUpdate = false;
	//if (PropertySet == Settings || PropertySet == UVChannelProperties)
	//{
	//	// One of the UV generation properties must have changed.  Dirty the result to force a recompute
	//	Preview->InvalidateResult();
	//	bForceMaterialUpdate = true;
	//}

	//if (PropertySet == MaterialSettings || bForceMaterialUpdate)
	//{
	//	MaterialSettings->UpdateMaterials();
	//	Preview->OverrideMaterial = MaterialSettings->GetActiveOverrideMaterial();
	//}
}


void UHotSpotTool::OnShutdown(EToolShutdownType ShutdownType)
{
	Settings->SaveProperties(this);
	UVChannelProperties->SaveProperties(this);
	PolygroupLayerProperties->SaveProperties(this, TEXT("HotSpotTool"));
	MaterialSettings->SaveProperties(this, TEXT("HotSpotTool"));


	if (SourcePreview != nullptr)
	{
		SourcePreview->Disconnect();
		SourcePreview = nullptr;
	}

	if (EditCompute != nullptr)
	{
		FDynamicMeshOpResult ComputeResult = EditCompute->Shutdown();
		EditCompute->ClearOpFactory();
		EditCompute->OnOpCompleted.RemoveAll(this);
		EditCompute = nullptr;

		UToolTarget* ActiveTarget = GetTarget();
		UE::ToolTarget::ShowSourceObject(ActiveTarget);

		if (ShutdownType == EToolShutdownType::Accept)
		{
			GetToolManager()->BeginUndoTransaction(LOCTEXT("HotSpot", "HotSpot UV"));

			if (TriangleSelection.Num() == 0)
			{
				FDynamicMesh3* NewDynamicMesh = ComputeResult.Mesh.Get();
				if (ensure(NewDynamicMesh))
				{
					UE::ToolTarget::CommitDynamicMeshUVUpdate(Target, NewDynamicMesh);
				}
			}
			else
			{
				// todo
				check(false);
			}

			if (ActiveHotSpotMaterial != nullptr)
			{
				FComponentMaterialSet NewMaterials;
				NewMaterials.Materials.Add(ActiveHotSpotMaterial);
				UE::ToolTarget::CommitMaterialSetUpdate(Target, NewMaterials);
			}

			GetToolManager()->EndUndoTransaction();
		}
	}


	if (EditRegionSharedMesh.IsValid())
	{
		EditRegionSharedMesh->ReleaseSharedObject();
	}
	InputMesh.Clear();
	OperatorFactory.Reset();

}

void UHotSpotTool::Render(IToolsContextRenderAPI* RenderAPI)
{
}


void UHotSpotTool::OnTick(float DeltaTime)
{
	EditCompute->Tick(DeltaTime);
}

bool UHotSpotTool::CanAccept() const
{
	return Super::CanAccept() && EditCompute->HaveValidResult();
}


int32 UHotSpotTool::GetSelectedUVChannel() const
{
	return UVChannelProperties ? UVChannelProperties->GetSelectedChannelIndex(true) : 0;
}

void UHotSpotTool::OnSelectedGroupLayerChanged()
{
	UpdateActiveGroupLayer();
	EditCompute->InvalidateResult();
}


void UHotSpotTool::UpdateActiveGroupLayer()
{
	//if (PolygroupLayerProperties->HasSelectedPolygroup() == false)
	//{
	//	ActiveGroupSet = MakeShared<UE::Geometry::FPolygroupSet, ESPMode::ThreadSafe>(InputMesh.Get());
	//}
	//else
	//{
	//	FName SelectedName = PolygroupLayerProperties->ActiveGroupLayer;
	//	FDynamicMeshPolygroupAttribute* FoundAttrib = UE::Geometry::FindPolygroupLayerByName(*InputMesh, SelectedName);
	//	ensureMsgf(FoundAttrib, TEXT("Selected Attribute Not Found! Falling back to Default group layer."));
	//	ActiveGroupSet = MakeShared<UE::Geometry::FPolygroupSet, ESPMode::ThreadSafe>(InputMesh.Get(), FoundAttrib);
	//}
	//if (HotSpotOpFactory)
	//{
	//	OperatorFactory->InputGroups = ActiveGroupSet;
	//}
}


void UHotSpotTool::OnPreviewMeshUpdated()
{
	if (MaterialSettings)
	{
		MaterialSettings->UpdateMaterials();
	}

}



#undef LOCTEXT_NAMESPACE

