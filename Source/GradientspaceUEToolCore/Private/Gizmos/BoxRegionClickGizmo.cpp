// Copyright Gradientspace Corp. All Rights Reserved.
#include "Gizmos/BoxRegionClickGizmo.h"

#include "InteractiveGizmoManager.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "BaseGizmos/GizmoMath.h"
#include "ToolDataVisualizer.h"
#include "ToolSceneQueriesUtil.h"

#include "SegmentTypes.h"
#include "BoxTypes.h"
#include "Intersection/IntrRay3AxisAlignedBox3.h"

#include "PreviewMesh.h"
#include "ToolSetupUtil.h"
#include "Generators/GridBoxMeshGenerator.h"
#include "DynamicMesh/MeshIndexUtil.h"
#include "DynamicMeshEditor.h"
#include "Polygroups/PolygroupsGenerator.h"


using namespace UE::Geometry;

const FString UBoxRegionClickGizmo::GizmoName(TEXT("BoxRegionClickGizmo"));


UInteractiveGizmo* UBoxRegionClickGizmoBuilder::BuildGizmo(const FToolBuilderState& SceneState) const
{
	return NewObject<UBoxRegionClickGizmo>(SceneState.GizmoManager);
}

void UBoxRegionClickGizmo::Setup()
{
	UInteractiveGizmo::Setup();

	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
	//HoverBehavior->Modifiers.RegisterModifier(ShiftModifierID, FInputDeviceState::IsShiftKeyDown);
	//HoverBehavior->Modifiers.RegisterModifier(CtrlModifierID, FInputDeviceState::IsCtrlKeyDown);
	HoverBehavior->Initialize(this);
	HoverBehavior->SetDefaultPriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY);
	AddInputBehavior(HoverBehavior);

	USingleClickInputBehavior* ClickBehavior = NewObject<USingleClickInputBehavior>();
	ClickBehavior->Initialize(this);
	//ClickDragBehavior->Modifiers.RegisterModifier(ShiftModifierID, FInputDeviceState::IsShiftKeyDown);
	//ClickDragBehavior->Modifiers.RegisterModifier(CtrlModifierID, FInputDeviceState::IsCtrlKeyDown);
	ClickBehavior->SetDefaultPriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY);
	AddInputBehavior(ClickBehavior);
}

void UBoxRegionClickGizmo::Shutdown()
{
	if (PreviewMesh)
	{
		PreviewMesh->Disconnect();
	}
}

void UBoxRegionClickGizmo::Initialize(UWorld* TargetWorld)
{
	PreviewMesh = NewObject<UPreviewMesh>(this);
	PreviewMesh->CreateInWorld(TargetWorld, FTransform::Identity);
	//UDynamicMeshComponent* DMComponent = Cast<UDynamicMeshComponent>(PreviewMesh->GetRootComponent());
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(PreviewMesh, nullptr);
	PreviewMesh->SetVisible(false);
	PreviewMesh->bBuildSpatialDataStructure = true;
	PreviewMesh->EnableSecondaryTriangleBuffers([&](const FDynamicMesh3* Mesh, int TriangleID)
	{
		return Mesh->GetTriangleGroup(TriangleID) != CurrentHitGroup;
	});
	PreviewMesh->FastNotifySecondaryTrianglesChanged();
	PreviewMesh->SetSecondaryBuffersVisibility(false);

	PreviewMesh->SetMaterial(
		ToolSetupUtil::GetSelectionMaterial(FLinearColor::Yellow, nullptr) );
	PreviewMesh->SetShadowsEnabled(false);
}



void UBoxRegionClickGizmo::InitializeParameters(
	const UE::Geometry::FFrame3d& WorldFrame,
	const UE::Geometry::FAxisAlignedBox3d& LocalBox)
{
	FrameWorld = WorldFrame;
	FrameBox = LocalBox;

	if (CurrentMeshBox != FrameBox)
	{
		CurrentMeshBox = FrameBox;
		FGridBoxMeshGenerator BoxGen;
		BoxGen.Box = FOrientedBox3d(CurrentMeshBox);
		BoxGen.Box.Extents += 0.01 * CurrentMeshBox.MaxDim() * FVector::One();
		BoxGen.EdgeVertices = FIndex3i(6,6,6);
		BoxGen.bPolygroupPerQuad = true;
		BoxGen.Generate();
		PreviewMesh->EditMesh([&](FDynamicMesh3& EditMesh)
		{
			EditMesh.Copy(&BoxGen);
			FDynamicMeshEditor Editor(&EditMesh);

			TArray<int32> InteriorVerts;
			FVector3d Extents = CurrentMeshBox.Extents();
			for (int k = 0; k < 3; ++k)
			{
				TArray<int32> Vertices;
				for (int vid : EditMesh.VertexIndicesItr())
				{
					FVector3d V = EditMesh.GetVertex(vid) - CurrentMeshBox.Center();
					if (V[k] > 0.8 * Extents[k] || V[k] < -0.8 * Extents[k])
					{
						Vertices.Add(vid);
					}
					if (V[k] < 0.99 * Extents[k] && V[k] > -0.99 * Extents[k])
					{
						InteriorVerts.Add(vid);
					}
				}
				TSet<int32> Triangles;
				UE::Geometry::VertexToTriangleOneRing(&EditMesh, Vertices, Triangles);
				Editor.DisconnectTriangles(Triangles.Array(), false);
			}

			EditMesh.DiscardTriangleGroups();
			EditMesh.EnableTriangleGroups();
			FPolygroupsGenerator GroupsGen(&EditMesh);
			GroupsGen.FindPolygroupsFromConnectedTris();
			GroupsGen.CopyPolygroupsToMesh();
			check(EditMesh.MaxGroupID() == 6 + 8 + 12 + 1);
		});
	}

	PreviewMesh->SetTransform(FrameWorld.ToFTransform());
}

void UBoxRegionClickGizmo::SetEnabled(bool bEnabledIn)
{
	bEnabled = bEnabledIn;
}



void UBoxRegionClickGizmo::Render(IToolsContextRenderAPI* RenderAPI)
{
	CurCameraState = RenderAPI->GetCameraState();
	if (!bEnabled) return;

	FToolDataVisualizer Draw;
	Draw.BeginFrame(RenderAPI);

	Draw.PushTransform(FrameWorld.ToFTransform());

	if (bHovering && bDrawBoxWireframe)
	{
		FAxisAlignedBox3d Tmp = FrameBox;
		Tmp.Expand(1.0);
		Draw.DrawWireBox((FBox)FrameBox, FLinearColor::Yellow, 3.0f, true);
	}

	//FVector3d A, B;
	//GetLineWorld(A, B);
	//if (bHovering || bInDragInteraction)
	//{
	//	Draw.DrawLine(A, B, FLinearColor::Yellow, 6.0, false);
	//}
	//else
	//{
	//	Draw.DrawLine(A, B, FLinearColor::Red, 3.0, false);
	//}

	Draw.PopTransform();
	Draw.EndFrame();
}

void UBoxRegionClickGizmo::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	if (!bEnabled) return;
}

void UBoxRegionClickGizmo::Tick(float DeltaTime)
{
	PreviewMesh->SetVisible(bEnabled);
}



bool UBoxRegionClickGizmo::HitTest(FRay3d WorldRay, double& RayT, FVector3d& RayPos, int& HitGroupID) const
{
	if (!bEnabled) return false;

	FRay3d LocalRay = FrameWorld.ToFrame(WorldRay);

	double HitRayT = 0;
	if ( FIntrRay3AxisAlignedBox3d::FindIntersection(LocalRay, FrameBox, HitRayT) )
	{
		FHitResult MeshHit;
		if (PreviewMesh->FindRayIntersection(WorldRay, MeshHit))
		{
			HitGroupID = PreviewMesh->GetMesh()->GetTriangleGroup(MeshHit.FaceIndex);
		}

		FVector3d LocalHitPos = LocalRay.PointAt(HitRayT);
		RayPos = FrameWorld.FromFramePoint(LocalHitPos);
		RayT = WorldRay.GetParameter(RayPos);

		return true;
	}

	return false;
}



FInputRayHit UBoxRegionClickGizmo::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	FVector3d RayPos; double RayT; int HitGroupID;
	if (HitTest(ClickPos.WorldRay, RayT, RayPos, HitGroupID))
	{
		return FInputRayHit(RayT);
	}
	return FInputRayHit();
}

void UBoxRegionClickGizmo::OnClicked(const FInputDeviceRay& ClickPos)
{
	FVector3d RayPos; double RayT; int HitGroupID;
	if (HitTest(ClickPos.WorldRay, RayT, RayPos, HitGroupID))
	{
		EBoxRegionType RegionType = EBoxRegionType::None;
		FVector3d RegionNormals[3] = { FVector3d::Zero(), FVector3d::Zero(), FVector3d::Zero() };
		FVector3i NormalCounts(0, 0, 0);

		PreviewMesh->ProcessMesh([&](const FDynamicMesh3& Mesh)
		{
			for (int tid : Mesh.TriangleIndicesItr())
			{
				int gid = Mesh.GetTriangleGroup(tid);
				if (gid != HitGroupID) continue;
				FVector3d FaceNormal = Mesh.GetTriNormal(tid);
				for (int j = 0; j < 3; ++j)
				{
					FVector3d Axis(0, 0, 0); Axis[j] = 1.0;
					if (FMathd::Abs(FaceNormal.Dot(Axis)) > 0.99)
					{
						NormalCounts[j]++;
						RegionNormals[j] = FaceNormal;
					}
				}
			}
			int NumDifferentAxes = (NormalCounts[0]>0?1:0) + (NormalCounts[1]>0?1:0) + (NormalCounts[2]>0?1:0);
			RegionType = static_cast<EBoxRegionType>(NumDifferentAxes);
		});

		if (RegionType != EBoxRegionType::None)
		{
			FVector3d SendNormals[3] = { FVector3d::Zero(), FVector3d::Zero(), FVector3d::Zero() };
			int idx = 0;
			for (int j = 0; j < 3; ++j)
			{
				if (NormalCounts[j] > 0) SendNormals[idx++] = RegionNormals[j];
			}
			OnBoxRegionClick.Broadcast(this, static_cast<int>(RegionType), SendNormals);
		}
	}
}



FInputRayHit UBoxRegionClickGizmo::BeginHoverSequenceHitTest(const FInputDeviceRay& DevicePos)
{
	FVector3d RayPos; double RayT; int HitGroupID;
	if (HitTest(DevicePos.WorldRay, RayT, RayPos, HitGroupID))
	{
		return FInputRayHit(RayT);
	}
	return FInputRayHit();
}

void UBoxRegionClickGizmo::OnBeginHover(const FInputDeviceRay& DevicePos)
{
	bHovering = true;
}

bool UBoxRegionClickGizmo::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	FVector3d RayPos; double RayT; int HitGroupID;
	if (HitTest(DevicePos.WorldRay, RayT, RayPos, HitGroupID))
	{
		CurrentHitGroup = HitGroupID;
		PreviewMesh->FastNotifySecondaryTrianglesChanged();
		return true;
	}
	return false;
}

void UBoxRegionClickGizmo::OnEndHover()
{
	bHovering = false;
	CurrentHitGroup = -1;
	PreviewMesh->FastNotifySecondaryTrianglesChanged();
}
