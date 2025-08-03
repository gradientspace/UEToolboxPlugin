// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Math/Ray.h"

#include "Math/GSFrame3.h"
#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridEditor.h"
#include "ModelGrid/ModelGridEditMachine.h"
#include "ModelGrid/ModelGridCollision.h"

#include "Engine/HitResult.h"

class FToolDataVisualizer;


struct GRADIENTSPACEUETOOLCORE_API ModelGridEditHitInfo
{
	GS::Vector3d WorldHitLocation;
	GS::Vector3d LocalHitLocation;

	GS::Vector3d WorldHitNormal;
	GS::Vector3d LocalHitNormal;

	GS::Vector3i HitCellIndex;

	GS::Vector3d LocalHitCellFaceNormal;
	GS::Vector3d WorldHitCellFaceNormal;

	enum class EHitType
	{
		GridSurface = 0, GridCell = 1, GroundPlane = 10, SceneSurfaces = 11
	};
	EHitType HitType = EHitType::GridSurface;
};



// grid hit result struct

class GRADIENTSPACEUETOOLCORE_API IModelGridEditToolStateAPI
{
public:
	virtual ~IModelGridEditToolStateAPI() {}

	// reads

	virtual GS::Frame3d GetGridFrame() = 0;
	virtual GS::AxisBox3d GetLocalCellBounds(GS::Vector3i CellIndex) = 0;

	// wrapper around PreviewGeometry->FindRayIntersection in tool
	virtual bool FindModelGridSurfaceRayIntersection(const GS::Ray3d& WorldRay, ModelGridEditHitInfo& HitInfoOut) = 0;

	// wrapper around Collider.FindNearestHitCell  (no collider access needed otherwise...)
	virtual bool FindOccupiedCellRayIntersection(const GS::Ray3d& WorldRay, ModelGridEditHitInfo& HitInfoOut) = 0;

	//! find intersection of worldray with scene objects, and then return cell containing that intersection
	virtual bool FindSceneRayIntersectionCell(const GS::Ray3d& WorldRay, FHitResult& WorldHitResultOut, ModelGridEditHitInfo& HitInfoOut) = 0;

	//! find intersection of worldray with ground plane (z=0/etc), and then return cell containing that intersection
	virtual bool FindGroundRayIntersectionCell(const GS::Ray3d& WorldRay, FHitResult& WorldHitResultOut, ModelGridEditHitInfo& HitInfoOut) = 0;

	// todo should replace this 'bIsInGrid' pattern with a return struct that can return not-in-grid value
	virtual GS::Vector3i GetCellAtPosition(const GS::Vector3d& Position, bool bIsWorldPosition, bool& bIsInGrid) = 0;
	virtual void QueryCellAtIndex(const GS::Vector3i& CellIndex, bool& bIsInGrid, bool& bIsEmpty) = 0;

	virtual bool CanApplyActiveDrawActionToCell(const GS::Vector3i& CellIndex) = 0;

	// return workplane axis 0/1/2, or -1 for no plane specified
	virtual int GetFixedWorkPlaneMode() const = 0;

	// 
	struct DrawModifiers
	{
		bool bDrawOnGround = true;
		bool bDrawOnWorldObjects = false;
	};
	virtual DrawModifiers GetDrawModifiers() const = 0;


	//! ToolAPI reports this back to Interaction to report what kind of operation it is going to do on BeginDrag/UpdateDrag/EndDrag
	enum EInteractionMode
	{
		//! continuous action based on hit position, eg like drawing, sculpting, etc
		SingleCell_Continuous,
		//! action based on start and end cells, where end cell will be based off initial workplane-normal-axis
		StartEndCells_NormalAxis,
		//! action based on start and end cells, where end cell will lie in the initial workplane
		StartEndCells_InPlane
	};

	virtual EInteractionMode GetInteractionMode() const = 0;


	//
	// writes
	//

	// set the preview location (ie hover/etc)
	virtual void UpdateGridEditPreviewLocation(bool bHaveValidPreview, GS::Vector3i NewLocation, GS::Frame3d LocationDrawPlaneLocal) = 0;

	//! set initial cursor location for stateful InteractionModes like StartEndCells_NormalAxis
	virtual void InitializeGridEditCursorLocation(GS::Vector3i NewLocation, GS::Vector3d LocalPosition, GS::Vector3d LocalNormal) = 0;

	// move live tool cursor to this location (eg draw/erase/paint/etc at this location)
	virtual void UpdateGridEditCursorLocation(GS::Vector3i NewLocation, GS::Vector3d LocalPosition, GS::Vector3d LocalNormal) = 0;

	virtual void TrySetActiveGridSelection(GS::Vector3i SelectedCell) = 0;
	virtual void ClearActiveGridSelection() = 0;

	// currently unused...
	virtual void AccessModelGrid(TFunctionRef<void(const GS::ModelGrid&)> ProcessFunc) = 0;

};


class GRADIENTSPACEUETOOLCORE_API ModelGridEditorToolInteraction
{
public:
	virtual ~ModelGridEditorToolInteraction() {}

	virtual void SetCurrentState(TSharedPtr<IModelGridEditToolStateAPI> ToolStateAPI) {
		ToolAPI = ToolStateAPI;
	}

	virtual bool HoverUpdate(const GS::Ray3d& WorldRay, double& HoverDistance) = 0;
	virtual bool CanBeginClickDrag(const GS::Ray3d& WorldRay) = 0;
	virtual void BeginClickDrag(const GS::Ray3d& WorldRay) = 0;
	virtual void UpdateClickDrag(const GS::Ray3d& WorldRay) = 0;


	enum class EInteractionState
	{
		NoInteraction,
		Hovering,
		ClickDragging
	};

	virtual void Render(FToolDataVisualizer& Visualizer, const FTransform& ToLocalTransform, EInteractionState CurrentState) = 0;

protected:
	TSharedPtr<IModelGridEditToolStateAPI> ToolAPI;


public:
	// utility functions shared by various subclasses

	// given a ray and initial cell, find a hit on the current workplane (defined by ToolAPI)
	bool FindWorkPlaneCellRayIntersection(const GS::Ray3d& WorldRay, GS::Vector3i InitialCell, GS::Vector3i& WorkPlaneCellOut, bool& bIsInGrid, bool& bIsEmpty, GS::Frame3d& WorkPlaneHitFrameOut) const;

	// find closest point along normal to WorkPlane to WorldRay, return it's cell and location
	bool FindWorkPlaneNormalAxisCellIndex(const GS::Ray3d& WorldRay, const GS::Frame3d& WorkPlaneLocal, GS::Vector3i& AxisCellIndexOut, GS::Vector3d& AxisPosLocalOut) const;

	// find intersection of WorldRay and WorkPlane, return cell and location
	bool FindWorkPlaneRayHitCellIndex(const GS::Ray3d& WorldRay, const GS::Frame3d& WorkPlaneLocal, GS::Vector3i& PlaneCellIndexOut, GS::Vector3d& PlanePosLocalOut) const;
};


class GRADIENTSPACEUETOOLCORE_API ModelGridEditExistingCellsInteraction : public ModelGridEditorToolInteraction
{
public:
	virtual bool HoverUpdate(const GS::Ray3d& WorldRay, double& HoverDistance) override;
	virtual bool CanBeginClickDrag(const GS::Ray3d& WorldRay) override;
	virtual void BeginClickDrag(const GS::Ray3d& WorldRay) override;
	virtual void UpdateClickDrag(const GS::Ray3d& WorldRay) override;
	virtual void Render(FToolDataVisualizer& Visualizer, const FTransform& ToLocalTransform, EInteractionState CurrentState) override;

	ModelGridEditHitInfo ActiveHitLocation;
	bool bHavePreviewHit;
	bool bActiveHitIsValidExistingCell;
	GS::Frame3d ActiveHitLocalDrawPlane;

	void UpdateCurrentHitQuery(const GS::Ray3d& WorldRay);

	virtual FLinearColor GetActiveCellPreviewColor() const { return FLinearColor::Red; }
	virtual float GetActiveCellLineThickness() const { return 1.0f; }

	// can this be folded into base class?? duplicated in other interactions...
	IModelGridEditToolStateAPI::EInteractionMode ActiveInterationMode;
	GS::Vector3i CurrentDragInitialCell;
	virtual void UpdateClickDrag_Continuous(const GS::Ray3d& WorldRay);
	virtual void UpdateClickDrag_AxisNormal(const GS::Ray3d& WorldRay);
	virtual void UpdateClickDrag_InPlane(const GS::Ray3d& WorldRay);
};



class GRADIENTSPACEUETOOLCORE_API ModelGridPaintCellsInteraction : public ModelGridEditExistingCellsInteraction
{
public:
};

class GRADIENTSPACEUETOOLCORE_API ModelGridEraseCellsInteraction : public ModelGridEditExistingCellsInteraction
{
public:
	virtual FLinearColor GetActiveCellPreviewColor() const override { return FLinearColor::Black; }
	virtual float GetActiveCellLineThickness() const override { return 2.0f; }
};

class GRADIENTSPACEUETOOLCORE_API ModelGridReplaceCellsInteraction : public ModelGridEditExistingCellsInteraction
{
public:
	//virtual FLinearColor GetActiveCellPreviewColor() const override { return FLinearColor::Black; }
	//virtual float GetActiveCellLineThickness() const override { return 2.0f; }
};

class GRADIENTSPACEUETOOLCORE_API ModelGridSelectCellInteraction : public ModelGridEditExistingCellsInteraction
{
public:
	virtual bool CanBeginClickDrag(const GS::Ray3d& WorldRay) override;
	virtual void BeginClickDrag(const GS::Ray3d& WorldRay) override;
	virtual void UpdateClickDrag(const GS::Ray3d& WorldRay) override;

	virtual FLinearColor GetActiveCellPreviewColor() const override { return FLinearColor::Blue; }
	virtual float GetActiveCellLineThickness() const override { return 2.0f; }
};


class GRADIENTSPACEUETOOLCORE_API ModelGridAppendCellsInteraction : public ModelGridEditorToolInteraction
{
public:
	virtual bool HoverUpdate(const GS::Ray3d& WorldRay, double& HoverDistance) override;
	virtual bool CanBeginClickDrag(const GS::Ray3d& WorldRay) override;
	virtual void BeginClickDrag(const GS::Ray3d& WorldRay) override;
	virtual void UpdateClickDrag(const GS::Ray3d& WorldRay) override;
	virtual void Render(FToolDataVisualizer& Visualizer, const FTransform& ToLocalTransform, EInteractionState CurrentState) override;

	// already spaghetti...

	// this is the nearest-hit result - could be a filled cell, in which case our
	// draw cell would be a face-neighbour, or it could be a ground/world hit, in
	// which case we would fill this cell 
	// (note in those cases the cell must be empty, or we would have had a closer grid-hit...)
	ModelGridEditHitInfo ActiveHitQueryResult;
	GS::Frame3d ActiveHitLocalDrawPlane;

	GS::Vector3i CurrentDrawCell;
	bool bHaveCurrentDrawCell;
	bool bCanDrawToCurrentDrawCell;

	GS::Vector3i LastDrawCell;
	bool bHaveLastDrawCell;

	void UpdateCurrentHitQuery(const GS::Ray3d& WorldRay);

	// can this be folded into base class?? duplicated in other interactions...
	IModelGridEditToolStateAPI::EInteractionMode ActiveInterationMode;
	GS::Vector3i CurrentDragInitialCell;
	virtual void UpdateClickDrag_Continuous(const GS::Ray3d& WorldRay);
	virtual void UpdateClickDrag_AxisNormal(const GS::Ray3d& WorldRay);
	virtual void UpdateClickDrag_InPlane(const GS::Ray3d& WorldRay);
};
