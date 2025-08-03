// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "HAL/CriticalSection.h"
#include "Templates/PimplPtr.h"
#include "UGSModelGrid.generated.h"

namespace GS { class ModelGrid; }

UCLASS(BlueprintType, MinimalAPI)
class UGSModelGrid : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	GRADIENTSPACEUESCENE_API 
	void InitializeNewGrid(bool bBroadcastEvents);

	GRADIENTSPACEUESCENE_API
	void SetEnableTransactions(bool bEnable);


public:
	//! Access the internal ModelGrid for reading. This function locks the grid, and so is safe to call from multiple threads.
	GRADIENTSPACEUESCENE_API
	virtual void ProcessGrid( TFunctionRef<void(const GS::ModelGrid& Grid)> ProcessFunc );

	//! Edit the internal ModelGrid. This function locks the grid, and so is safe to call from multiple threads.
	GRADIENTSPACEUESCENE_API
	virtual void EditGrid( 
		TFunctionRef<void(GS::ModelGrid& Grid)> EditFunc,
		bool bDeferUpdateNotification = false);

	//! removes all cells from current grid - implemented via EditGrid
	GRADIENTSPACEUESCENE_API
	virtual void ResetGrid(bool bDeferUpdateNotification = false);

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnModelGridReplaced, UGSModelGrid*);
	FOnModelGridReplaced& OnModelGridReplaced() { return ModelGridReplacedEvent; }
protected:
	FOnModelGridReplaced ModelGridReplacedEvent;


public:
	/**
	 * In situations where multiple calls to EditGrid() might be made in a single game tick,
	 * this function can be used to prevent change notifications from going out until 
	 * EndGridEdits() is called. This can be used to avoid redundant updates from
	 * occurring in higher-level objects listening to those change notifications.
	 * 
	 * This function increments an "active edits" counter, so nested pairs of Begin/End calls are supported. 
	 * This is intended to be used in the context of single function calls or BP evaluations.
	 * Framework-level code may clear edits to ensure that updates happen on the next frame.
	 * 
	 * This is exposed to BP for advanced users, please be careful.
	 */
	UFUNCTION(BlueprintCallable, Category = "ModelGrid")
	virtual void BeginGridEdits();

	/**
	 * Decrement the active-grid-edits counter. Must be paired with a BeginGridEdits call.
	 * Change notification will be posted if EditGrid() was called at all between outer Begin/End.
	 * @param bDeferNotification if true, change notification will not be sent
	 */
	UFUNCTION(BlueprintCallable, Category = "ModelGrid")
	virtual void EndGridEdits(bool bDeferNotification = false);

	//! return the current depth of nested BeginGridEdits calls
	virtual int32 GetGridEditDepth() const;
	//! unwind the stack of edits by repeatedly calling EndGridEdits, useful for framework code to clean up Begin/End mistakes
	virtual void EndAllPendingGridEdits();


protected:
	TPimplPtr<GS::ModelGrid> Grid;
	FCriticalSection GridLock;


	bool bEnableTransactions = false;

	std::atomic<int32> GridEditCounter = 0;
	std::atomic<int32> GridEditStackDepth = 0;
	std::atomic<int32> InitialGridEditCounter;		// set on BeginGridEdits

	//! allows editing grid but does not post any notifications/etc
	GRADIENTSPACEUESCENE_API
	virtual void EditGridInternal(TFunctionRef<void(GS::ModelGrid& Grid)> EditFunc);

public:
	GRADIENTSPACEUESCENE_API virtual void Serialize(FArchive& Archive) override;
	GRADIENTSPACEUESCENE_API virtual void PostLoad() override;
#if WITH_EDITOR
	GRADIENTSPACEUESCENE_API virtual void PostEditUndo() override;
#endif
	// serialize to/from T3D
	GRADIENTSPACEUESCENE_API virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;
	GRADIENTSPACEUESCENE_API virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;

};
