// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/ModelGridEditorToolInteraction.h"

#include "FrameTypes.h"
#include "VectorTypes.h"
#include "ToolDataVisualizer.h"
#include "Distance/DistLine3Ray3.h"

#include "Math/GSVector3.h"
#include "Math/GSVectorMath.h"
#include "Intersection/GSRayBoxIntersection.h"

using namespace UE::Geometry;
using namespace GS;




static FVector3d GetWorkPlaneNormal(int WorkPlaneAxisIndex, const GS::Frame3d& GridFrame, const GS::Vector3d& RayDirection, bool bLocal)
{
	FVector3d PlaneNormal(0, 0, 0);
	PlaneNormal[WorkPlaneAxisIndex] = 1;
	PlaneNormal = GridFrame.ToWorldVector(PlaneNormal);
	if (PlaneNormal.Dot(RayDirection) > 0)
		PlaneNormal = -PlaneNormal;
	if (bLocal)
		PlaneNormal = GridFrame.ToLocalVector(PlaneNormal);
	return PlaneNormal;
}


bool ModelGridEditorToolInteraction::FindWorkPlaneCellRayIntersection(const GS::Ray3d& WorldRay, GS::Vector3i InitialCell,
	GS::Vector3i& WorkPlaneCellOut, bool& bIsInGrid, bool& bIsEmpty, GS::Frame3d& WorkPlaneHitFrameOut) const
{
	Frame3d GridFrame = ToolAPI->GetGridFrame();
	int FixedWorkPlaneNormalAxisIndex = ToolAPI->GetFixedWorkPlaneMode();

	FAxisAlignedBox3d CurCellBounds = (FAxisAlignedBox3d)ToolAPI->GetLocalCellBounds(InitialCell);
	FVector3d CurCellCenter = GridFrame.ToWorldPoint(CurCellBounds.Center());
	FVector3d WorkPlaneNormal = GetWorkPlaneNormal(FixedWorkPlaneNormalAxisIndex, GridFrame, WorldRay.Direction, false);

	// leave work plane at center of cell, it feels more right...
	//CurCellCenter -= WorkPlaneNormal * CurCellBounds.Extents()[FixedWorkPlaneNormalAxisIndex];

	FFrame3d DrawPlane(CurCellCenter, WorkPlaneNormal);
	FVector PlaneHitPointWorld;
	if (DrawPlane.RayPlaneIntersection(WorldRay.Origin, WorldRay.Direction, 2, PlaneHitPointWorld))
	{
		PlaneHitPointWorld += 0.0001 * DrawPlane.Z();
		bIsEmpty = false, bIsInGrid = false;
		WorkPlaneCellOut = ToolAPI->GetCellAtPosition(PlaneHitPointWorld, true, bIsInGrid);
		ToolAPI->QueryCellAtIndex(WorkPlaneCellOut, bIsInGrid, bIsEmpty);
		WorkPlaneHitFrameOut = Frame3d(GridFrame.ToLocalPoint(PlaneHitPointWorld), GridFrame.ToLocalVector(WorkPlaneNormal));
		return true;
	}
	return false;
}



bool ModelGridEditorToolInteraction::FindWorkPlaneNormalAxisCellIndex(const GS::Ray3d& WorldRay, const GS::Frame3d& WorkPlaneLocal, 
	GS::Vector3i& AxisCellIndexOut, GS::Vector3d& AxisPosLocalOut) const
{
	GS::Frame3d GridFrame = ToolAPI->GetGridFrame();
	GS::Frame3d WorldFrame = GridFrame.ToWorldFrame(WorkPlaneLocal);

	// assume work plane is fixed. We want to find closest point along it
	FLine3d AxisLine(WorldFrame.Origin, WorldFrame.Z());
	FDistLine3Ray3d Distance(AxisLine, (FRay3d)WorldRay);
	Distance.Get();
	Vector3d LinePosWorld = Distance.LineClosestPoint;
	AxisPosLocalOut = GridFrame.ToLocalPoint(LinePosWorld);
	bool bIsInGrid = false;
	AxisCellIndexOut = ToolAPI->GetCellAtPosition(AxisPosLocalOut, false, bIsInGrid);
	return bIsInGrid;
}

bool ModelGridEditorToolInteraction::FindWorkPlaneRayHitCellIndex(const GS::Ray3d& WorldRay, const GS::Frame3d& WorkPlaneLocal,
	GS::Vector3i& PlaneCellIndexOut, GS::Vector3d& PlanePosLocalOut) const
{
	GS::Frame3d GridFrame = ToolAPI->GetGridFrame();
	GS::Frame3d WorldFrame = GridFrame.ToWorldFrame(WorkPlaneLocal);
	auto Result = WorldFrame.FindRayPlaneIntersection(WorldRay, 2);
	if (!Result)
		return false;
	PlanePosLocalOut = GridFrame.ToLocalPoint(Result);
	bool bIsInGrid = false;
	PlaneCellIndexOut = ToolAPI->GetCellAtPosition(PlanePosLocalOut, false, bIsInGrid);
	return bIsInGrid;
}


void ModelGridEditExistingCellsInteraction::UpdateCurrentHitQuery(const GS::Ray3d& WorldRay)
{
	bHavePreviewHit = bActiveHitIsValidExistingCell = false;

	auto UpdateActiveHit = [this, WorldRay]() {
		bHavePreviewHit = true;
		bool bIsInGrid = false, bIsEmpty = false;
		ToolAPI->QueryCellAtIndex(ActiveHitLocation.HitCellIndex, bIsInGrid, bIsEmpty);
		bActiveHitIsValidExistingCell = bIsInGrid && !bIsEmpty;

		ActiveHitLocalDrawPlane = Frame3d(ActiveHitLocation.LocalHitLocation, Vector3d::UnitZ());
		if (bActiveHitIsValidExistingCell) {
			ActiveHitLocalDrawPlane = Frame3d(ActiveHitLocation.LocalHitLocation,
				GS::ClosestUnitVector(ActiveHitLocation.LocalHitNormal));
		}

		// if we have a fixed work plane, we want to re-orient the ActiveHitLocalDrawPlane to be aligned with the workplane
		// note: something a bit out-of-sync here because new frame origin will not be along ray...and maybe not inside input cell...
		if (ToolAPI->GetFixedWorkPlaneMode() >= 0) {
			bIsInGrid = false; bIsEmpty = false; Vector3i TmpCell;
			FindWorkPlaneCellRayIntersection(WorldRay, ActiveHitLocation.HitCellIndex, TmpCell, bIsInGrid, bIsEmpty, ActiveHitLocalDrawPlane);
		}
	};

	// ray-mesh hit has priority
	if ( ToolAPI->FindModelGridSurfaceRayIntersection(WorldRay, ActiveHitLocation) ) {
		UpdateActiveHit();
		return;
	}

	// if we don't hit a surface, try hitting grid cells?  (might be weird...)
	if ( ToolAPI->FindOccupiedCellRayIntersection(WorldRay, ActiveHitLocation) ) {
		UpdateActiveHit();
		return;
	}

	FHitResult TmpHitResult;
	if (ToolAPI->FindGroundRayIntersectionCell(WorldRay, TmpHitResult, ActiveHitLocation)) {
		bHavePreviewHit = true;
		bActiveHitIsValidExistingCell = false;
		ActiveHitLocalDrawPlane = Frame3d(ActiveHitLocation.LocalHitLocation, Vector3d::UnitZ());

		// if we have a fixed work plane, we want to re-orient the ActiveHitLocalDrawPlane to be aligned with the workplane
		// note: something a bit out-of-sync here because new frame origin will not be along ray...and maybe not inside input cell...
		if (ToolAPI->GetFixedWorkPlaneMode() >= 0) {
			bool bIsInGrid = false, bIsEmpty = false; Vector3i TmpCell;
			bool bPlaneHit = FindWorkPlaneCellRayIntersection(WorldRay, ActiveHitLocation.HitCellIndex, TmpCell, bIsInGrid, bIsEmpty, ActiveHitLocalDrawPlane);
		}
	}
}



bool ModelGridEditExistingCellsInteraction::HoverUpdate(const GS::Ray3d& WorldRay, double& HoverDistance)
{
	UpdateCurrentHitQuery(WorldRay);
	ToolAPI->UpdateGridEditPreviewLocation(bHavePreviewHit, ActiveHitLocation.HitCellIndex, ActiveHitLocalDrawPlane);

	HoverDistance = GS::Ray3d::SafeMaxDist();
	if (bHavePreviewHit) {
		HoverDistance = Distance(WorldRay.Origin, ActiveHitLocation.WorldHitLocation);
		return true;
	}
	return false;
}

bool ModelGridEditExistingCellsInteraction::CanBeginClickDrag(const GS::Ray3d& WorldRay)
{
	return bHavePreviewHit;
}

void ModelGridEditExistingCellsInteraction::BeginClickDrag(const GS::Ray3d& WorldRay)
{
	ActiveInterationMode = ToolAPI->GetInteractionMode();
	CurrentDragInitialCell = ActiveHitLocation.HitCellIndex;

	if (!bActiveHitIsValidExistingCell)
		return;

	if (ActiveInterationMode != IModelGridEditToolStateAPI::EInteractionMode::SingleCell_Continuous) {
		ToolAPI->InitializeGridEditCursorLocation(ActiveHitLocation.HitCellIndex, ActiveHitLocation.LocalHitLocation, ActiveHitLocation.LocalHitNormal);
	}

	ToolAPI->UpdateGridEditCursorLocation(ActiveHitLocation.HitCellIndex, ActiveHitLocation.LocalHitLocation, ActiveHitLocation.LocalHitNormal);
}


void ModelGridEditExistingCellsInteraction::UpdateClickDrag(const GS::Ray3d& WorldRay)
{
	switch (ActiveInterationMode) {
	case IModelGridEditToolStateAPI::EInteractionMode::StartEndCells_NormalAxis:
		UpdateClickDrag_AxisNormal(WorldRay); break;
	case IModelGridEditToolStateAPI::EInteractionMode::StartEndCells_InPlane:
		UpdateClickDrag_InPlane(WorldRay); break;
	default:
		UpdateClickDrag_Continuous(WorldRay); break;
	}
}


void ModelGridEditExistingCellsInteraction::UpdateClickDrag_Continuous(const GS::Ray3d& WorldRay)
{
	UpdateCurrentHitQuery(WorldRay);
	Vector3i PreviewCellIndex = ActiveHitLocation.HitCellIndex;
	Vector3d PreviewLocalPosition = ActiveHitLocation.LocalHitLocation;
	Vector3d PreviewLocalNormal = ActiveHitLocation.LocalHitNormal;

	// if we are using a fixed work plane, replace hit query result w/ workplane hit
	// (perhaps this should be integrated into hit query??)
	// does not actually work properly!! the issue is that after a cell is erased, the occupied-cell query in UpdateCurrentHitQuery() will
	// still hit that cell, blocking the plane-hits. If we are erasing on a fixed plane, the queries should only hit cells
	// on that plane...
	int WorkPlaneAxis = ToolAPI->GetFixedWorkPlaneMode();
	if (WorkPlaneAxis >= 0 && GS::Abs(PreviewCellIndex[WorkPlaneAxis] - CurrentDragInitialCell[WorkPlaneAxis]) > 0) {
		bool bIsInGrid = false, bIsEmpty = false;
		bool bPlaneHit = FindWorkPlaneCellRayIntersection(WorldRay, CurrentDragInitialCell, PreviewCellIndex, bIsInGrid, bIsEmpty, ActiveHitLocalDrawPlane);
		bHavePreviewHit = bPlaneHit && bIsInGrid;
		if (bHavePreviewHit) {
			PreviewLocalPosition = ActiveHitLocalDrawPlane.Origin;
			PreviewLocalNormal = ActiveHitLocalDrawPlane.Rotation.AxisZ();
		}
		if (bHavePreviewHit && bIsEmpty == false)
			bActiveHitIsValidExistingCell = true;
	}

	ToolAPI->UpdateGridEditPreviewLocation(bHavePreviewHit, PreviewCellIndex, ActiveHitLocalDrawPlane);
	if (bActiveHitIsValidExistingCell)
		ToolAPI->UpdateGridEditCursorLocation(PreviewCellIndex, PreviewLocalPosition, PreviewLocalNormal);
}


void ModelGridEditExistingCellsInteraction::UpdateClickDrag_AxisNormal(const GS::Ray3d& WorldRay)
{
	Vector3i PlaneCellIndex; Vector3d LocalPosition;
	if (ModelGridEditorToolInteraction::FindWorkPlaneNormalAxisCellIndex(WorldRay, ActiveHitLocalDrawPlane, PlaneCellIndex, LocalPosition)) {
		ToolAPI->UpdateGridEditCursorLocation(PlaneCellIndex, LocalPosition, ActiveHitLocalDrawPlane.Z());
	}
}

void ModelGridEditExistingCellsInteraction::UpdateClickDrag_InPlane(const GS::Ray3d& WorldRay)
{
	Vector3i PlaneCellIndex; Vector3d LocalPosition;
	if (ModelGridEditorToolInteraction::FindWorkPlaneRayHitCellIndex(WorldRay, ActiveHitLocalDrawPlane, PlaneCellIndex, LocalPosition)) {
		ToolAPI->UpdateGridEditCursorLocation(PlaneCellIndex, LocalPosition, ActiveHitLocalDrawPlane.Z());
	}
}



void ModelGridEditExistingCellsInteraction::Render(FToolDataVisualizer& Visualizer, const FTransform& ToLocalTransform, EInteractionState CurrentState)
{
	if (CurrentState == EInteractionState::NoInteraction) return;

	Visualizer.PushTransform(ToLocalTransform);

	FLinearColor UseColor = (bActiveHitIsValidExistingCell) ? GetActiveCellPreviewColor() : FLinearColor::Gray;
	float UseLineThickness = (bActiveHitIsValidExistingCell) ? GetActiveCellLineThickness() : 0.5f;

	AxisBox3d CurCellBounds = ToolAPI->GetLocalCellBounds(ActiveHitLocation.HitCellIndex);
	Visualizer.DrawWireBox((FBox)CurCellBounds, UseColor, UseLineThickness, true);

	Visualizer.PopTransform();
}





bool ModelGridSelectCellInteraction::CanBeginClickDrag(const GS::Ray3d& WorldRay)
{
	return true;
}
void ModelGridSelectCellInteraction::BeginClickDrag(const GS::Ray3d& WorldRay)
{
	if (bActiveHitIsValidExistingCell)
		ToolAPI->TrySetActiveGridSelection(ActiveHitLocation.HitCellIndex);
	else
		ToolAPI->ClearActiveGridSelection();
}
void ModelGridSelectCellInteraction::UpdateClickDrag(const GS::Ray3d& WorldRay)
{
	// ignore, no drag functionality currently
}









void ModelGridAppendCellsInteraction::UpdateCurrentHitQuery(const GS::Ray3d& WorldRay)
{
	Ray3d LocalRay = ToolAPI->GetGridFrame().ToLocalRay(WorldRay);

	auto Modifiers = ToolAPI->GetDrawModifiers();

	TArray<ModelGridEditHitInfo, TInlineAllocator<3>> HitQueryResults;

	// try to hit an existing filled cell
	bool bHaveCellHit = false;
	ModelGridEditHitInfo FilledCellsHit;
	if (ToolAPI->FindOccupiedCellRayIntersection(WorldRay, FilledCellsHit)) {
		HitQueryResults.Add(FilledCellsHit);
		bHaveCellHit = true;
	}

	// try to hit existing world objects
	if (Modifiers.bDrawOnWorldObjects) {
		FHitResult SceneHitResult;
		ModelGridEditHitInfo SceneGridHit;
		if (ToolAPI->FindSceneRayIntersectionCell(WorldRay, SceneHitResult, SceneGridHit))
			HitQueryResults.Add(SceneGridHit);
	}

	// try to hit the ground plane. However we only draw on ground if we didn't hit an existing cell..
	if (Modifiers.bDrawOnGround && WorldRay.Direction.Dot(Vector3d::UnitZ()) < 0 ) {
		FHitResult GroundHitResult;
		ModelGridEditHitInfo GroundGridHit;
		if (ToolAPI->FindGroundRayIntersectionCell(WorldRay, GroundHitResult, GroundGridHit))
			HitQueryResults.Add(GroundGridHit);
	}

	// if we didn't hit anything, we have no starting point for drawing
	if (HitQueryResults.Num() == 0) {
		bHaveCurrentDrawCell = false;
		return;
	}

	// take the nearest of the above 3 hits  (note we don't actually need to sort here, just find-minimum...)
	HitQueryResults.Sort([WorldRay](const ModelGridEditHitInfo& A, const ModelGridEditHitInfo& B) {
		return WorldRay.GetParameter(A.WorldHitLocation) < WorldRay.GetParameter(B.WorldHitLocation);
	});
	ActiveHitQueryResult = HitQueryResults[0];

	// want to figure out the draw plane based on the cell under the cursor, not the adjacent cell
	// we will draw into. Maybe? should we even do this for non-cell hits?
	//ActiveHitLocalDrawPlane = Frame3d(ActiveHitQueryResult.LocalHitLocation, Vector3d::UnitZ());
	ActiveHitLocalDrawPlane = Frame3d(ActiveHitQueryResult.LocalHitLocation,
		GS::ClosestUnitVector(ActiveHitQueryResult.LocalHitNormal));

	// If we hit a filled grid cell, we want to draw on one of the neighbours, not the cell itself
	// So offset by the normal of the cell-face that we hit
	if (ActiveHitQueryResult.HitType == ModelGridEditHitInfo::EHitType::GridCell) {
		Vector3i NormalOffset = Vector3i::RoundToInt(ActiveHitQueryResult.LocalHitNormal);
		CurrentDrawCell = ActiveHitQueryResult.HitCellIndex + NormalOffset;
	} else {
		CurrentDrawCell = ActiveHitQueryResult.HitCellIndex;
	}
	bHaveCurrentDrawCell = true;

	// make sure we can draw to this cell
	bCanDrawToCurrentDrawCell = ToolAPI->CanApplyActiveDrawActionToCell(CurrentDrawCell);

	// if we have a fixed work plane, we want to re-orient the ActiveHitLocalDrawPlane to be aligned with the workplane
	// note: something a bit out-of-sync here because new frame origin will not be along ray...and maybe not inside input cell...
	if (ToolAPI->GetFixedWorkPlaneMode() >= 0) {
		bool bIsInGrid = false, bIsEmpty = false; Vector3i TmpCell;
		bool bPlaneHit = FindWorkPlaneCellRayIntersection(WorldRay, CurrentDrawCell, TmpCell, bIsInGrid, bIsEmpty, ActiveHitLocalDrawPlane);
	}
}



bool ModelGridAppendCellsInteraction::HoverUpdate(const GS::Ray3d& WorldRay, double& HoverDistance)
{
	UpdateCurrentHitQuery(WorldRay);
	ToolAPI->UpdateGridEditPreviewLocation(bHaveCurrentDrawCell, 
		(bHaveCurrentDrawCell) ? CurrentDrawCell : ActiveHitQueryResult.HitCellIndex, ActiveHitLocalDrawPlane);

	HoverDistance = GS::Ray3d::SafeMaxDist();
	if (bHaveCurrentDrawCell) {
		HoverDistance = Distance(WorldRay.Origin, ActiveHitQueryResult.WorldHitLocation);
		return true;
	}
	return false;
}

bool ModelGridAppendCellsInteraction::CanBeginClickDrag(const GS::Ray3d& WorldRay)
{
	// why not bHaveCurrentDrawCell && bCanDrawToCurrentDrawCell ??
	// (because we allow starting a click-drag on invalid cells in some cases? should investigate this...)
	return bHaveCurrentDrawCell;
}


void ModelGridAppendCellsInteraction::BeginClickDrag(const GS::Ray3d& WorldRay)
{
	ActiveInterationMode = ToolAPI->GetInteractionMode();
	CurrentDragInitialCell = CurrentDrawCell;
	bHaveLastDrawCell = false;

	if (!(bHaveCurrentDrawCell && bCanDrawToCurrentDrawCell))
		return;

	if (ActiveInterationMode != IModelGridEditToolStateAPI::EInteractionMode::SingleCell_Continuous) {
		ToolAPI->InitializeGridEditCursorLocation(CurrentDrawCell, ActiveHitLocalDrawPlane.Origin, ActiveHitLocalDrawPlane.Z() );
	}

	ToolAPI->UpdateGridEditCursorLocation(CurrentDrawCell, ActiveHitLocalDrawPlane.Origin, ActiveHitLocalDrawPlane.Z() );
	LastDrawCell = CurrentDrawCell;	
	bHaveLastDrawCell = true;
}



void ModelGridAppendCellsInteraction::UpdateClickDrag(const GS::Ray3d& WorldRay)
{
	switch (ActiveInterationMode) {
	case IModelGridEditToolStateAPI::EInteractionMode::StartEndCells_NormalAxis:
		UpdateClickDrag_AxisNormal(WorldRay); break;
	case IModelGridEditToolStateAPI::EInteractionMode::StartEndCells_InPlane:
		UpdateClickDrag_InPlane(WorldRay); break;
	default:
		UpdateClickDrag_Continuous(WorldRay); break;
	}
}

void ModelGridAppendCellsInteraction::UpdateClickDrag_Continuous(const GS::Ray3d& WorldRay)
{
	UpdateCurrentHitQuery(WorldRay);

	// if we are using a fixed work plane, replace hit query result w/ workplane hit
	// (perhaps this should be integrated into hit query??)
	if (ToolAPI->GetFixedWorkPlaneMode() >= 0) {
		bool bIsInGrid = false, bIsEmpty = false;
		bool bPlaneHit = FindWorkPlaneCellRayIntersection(WorldRay, CurrentDragInitialCell, CurrentDrawCell, bIsInGrid, bIsEmpty, ActiveHitLocalDrawPlane);
		bHaveCurrentDrawCell = bPlaneHit && bIsInGrid;
		if (bHaveCurrentDrawCell)
			bCanDrawToCurrentDrawCell = ToolAPI->CanApplyActiveDrawActionToCell(CurrentDrawCell);
	}

	ToolAPI->UpdateGridEditPreviewLocation(bHaveCurrentDrawCell,
		(bHaveCurrentDrawCell) ? CurrentDrawCell : ActiveHitQueryResult.HitCellIndex, ActiveHitLocalDrawPlane);

	if (bHaveCurrentDrawCell && bCanDrawToCurrentDrawCell) {
		// todo figure out better position/normal here
		ToolAPI->UpdateGridEditCursorLocation(CurrentDrawCell, ActiveHitLocalDrawPlane.Origin, ActiveHitLocalDrawPlane.Z());
		LastDrawCell = CurrentDrawCell;	bHaveLastDrawCell = true;
	}
}


void ModelGridAppendCellsInteraction::UpdateClickDrag_AxisNormal(const GS::Ray3d& WorldRay)
{
	Vector3i PlaneCellIndex; Vector3d LocalPosition;
	if (ModelGridEditorToolInteraction::FindWorkPlaneNormalAxisCellIndex(WorldRay, ActiveHitLocalDrawPlane, PlaneCellIndex, LocalPosition)) {
		ToolAPI->UpdateGridEditCursorLocation(PlaneCellIndex, LocalPosition, ActiveHitLocalDrawPlane.Z());
		CurrentDrawCell = PlaneCellIndex;
	}
}


void ModelGridAppendCellsInteraction::UpdateClickDrag_InPlane(const GS::Ray3d& WorldRay)
{
	Vector3i PlaneCellIndex; Vector3d LocalPosition;
	if (ModelGridEditorToolInteraction::FindWorkPlaneRayHitCellIndex(WorldRay, ActiveHitLocalDrawPlane, PlaneCellIndex, LocalPosition)) {
		ToolAPI->UpdateGridEditCursorLocation(PlaneCellIndex, LocalPosition, ActiveHitLocalDrawPlane.Z());
		CurrentDrawCell = PlaneCellIndex;
	}
}



void ModelGridAppendCellsInteraction::Render(FToolDataVisualizer& Visualizer, const FTransform& ToLocalTransform, EInteractionState CurrentState)
{
	if (CurrentState == EInteractionState::NoInteraction) return;

	Visualizer.PushTransform(ToLocalTransform);

	if (bHaveCurrentDrawCell)
	{
		// if the hit was a grid-cell hit, we are drawing on an adjacent cell, and we want to draw the hit-cell as a guide
		if (ActiveHitQueryResult.HitType == ModelGridEditHitInfo::EHitType::GridCell)
		{
			AxisBox3d AdjacentCellBounds = ToolAPI->GetLocalCellBounds(ActiveHitQueryResult.HitCellIndex);
			Visualizer.DrawWireBox((FBox)AdjacentCellBounds, FLinearColor::Gray, 0.5f, true);
		}

		AxisBox3d DrawCellBounds = ToolAPI->GetLocalCellBounds(CurrentDrawCell);
		if (bCanDrawToCurrentDrawCell) 
			Visualizer.DrawWireBox((FBox)DrawCellBounds, FLinearColor::Green, 1.0f, true);
		else
			Visualizer.DrawWireBox((FBox)DrawCellBounds, FLinearColor::Red, 2.0f, true);
	}

	Visualizer.PopTransform();
}

