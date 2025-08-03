// Copyright Gradientspace Corp. All Rights Reserved.
#include "GridActor/UGSModelGrid.h"

#include "ModelGrid/ModelGrid.h"
#include "ModelGrid/ModelGridSerializer.h"
#include "GradientspaceUELogging.h"

#include "Templates/TypeHash.h"


using namespace UE::Geometry;

UGSModelGrid::UGSModelGrid(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitializeNewGrid(false);

	//SetEnableTransactions(true);
}

void UGSModelGrid::SetEnableTransactions(bool bEnable)
{
	if (bEnableTransactions != bEnable)
	{
		bEnableTransactions = bEnable;
		if (bEnable)
			SetFlags(RF_Transactional);
		else
			ClearFlags(RF_Transactional);
	}
}

void UGSModelGrid::InitializeNewGrid(bool bBroadcastEvents)
{
	GridLock.Lock();

	FVector3d CellDimensions(50, 50, 50);
	Grid = MakePimpl<GS::ModelGrid>();
	Grid->Initialize(CellDimensions);

	GridLock.Unlock();

	if (bBroadcastEvents)
	{
		ModelGridReplacedEvent.Broadcast(this);
	}
}

void UGSModelGrid::ProcessGrid(TFunctionRef<void(const GS::ModelGrid& Grid)> ProcessFunc)
{
	// TODO support multiple read-only locks?
	GridLock.Lock();

	if (Grid)
	{
		ProcessFunc(*Grid);
	}

	GridLock.Unlock();
}

void UGSModelGrid::EditGrid(TFunctionRef<void(GS::ModelGrid& Grid)> EditFunc,
	bool bDeferUpdateNotification)
{
	EditGridInternal(EditFunc);
	if (GridEditStackDepth == 0 && bDeferUpdateNotification == false)
		ModelGridReplacedEvent.Broadcast(this);
}
void UGSModelGrid::EditGridInternal(TFunctionRef<void(GS::ModelGrid& Grid)> EditFunc)
{
	GridLock.Lock();

	if (Grid)
	{
		EditFunc(*Grid);
		GridEditCounter++;
	}

	GridLock.Unlock();
}

void UGSModelGrid::ResetGrid(bool bDeferUpdateNotification)
{
	EditGrid([](GS::ModelGrid& Grid) {
		GS::ModelGrid NewGrid;
		NewGrid.Initialize(Grid.GetCellDimensions());
		Grid = MoveTemp(NewGrid);
	}, bDeferUpdateNotification);

}


void UGSModelGrid::BeginGridEdits()
{
	ensureMsgf(IsInGameThread(), TEXT("UGSModelGrid::BeginGridEdits called off the Game Thread!!"));

	GridLock.Lock();
	if (GridEditStackDepth == 0) {
		uint32 CurCounter = GridEditCounter;
		InitialGridEditCounter = CurCounter;
	}
	GridEditStackDepth++;
	GridLock.Unlock();
}

void UGSModelGrid::EndGridEdits(bool bDeferNotification)
{
	ensureMsgf(IsInGameThread(), TEXT("UGSModelGrid::EndGridEdits called off the Game Thread!!"));

	// maybe should just be log? seems like a common user error..
	if (GridEditStackDepth == 0) {
		UE_LOG(LogGradientspace, Warning, TEXT("[UGSModelGrid] GridEditsCompleted called more times than BeginGridEdits. Ignoring."));
		return;
	}

	GridLock.Lock();
	GridEditStackDepth--;
	bool bNotify = (bDeferNotification == false)
		&& (GridEditStackDepth == 0)
		&& (GridEditCounter > InitialGridEditCounter);
	GridLock.Unlock();

	if (bNotify)
 		ModelGridReplacedEvent.Broadcast(this);
}


int32 UGSModelGrid::GetGridEditDepth() const
{
	return GridEditStackDepth;
}
void UGSModelGrid::EndAllPendingGridEdits()
{
	while (GridEditStackDepth > 0)
		EndGridEdits(false);
}


void UGSModelGrid::Serialize(FArchive& Archive)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGSModelGrid::Serialize);

	Super::Serialize(Archive);

	FScopeLock ScopeLock(&GridLock);

	if (!Grid) 
		return;

	// skip undo/redo transaction serialization unless we want that...
	if (bEnableTransactions == false && Archive.IsTransacting()) {
		// write the same bytecount we write below so that if we end up trying
		// to restore this record later, we don't crash
		int64 ByteCount = 0;
		Archive << ByteCount;
		return;
	}

	if (Archive.IsLoading())
	{
		InitializeNewGrid(false);
		int64 ByteCount = 0;
		Archive << ByteCount;
		if (ByteCount > 0)
		{
			GS::MemorySerializer Serializer;
			Serializer.InitializeMemory((size_t)ByteCount);
			size_t NumBytes = 0;
			uint8_t* ByteBuffer = Serializer.GetWritableBuffer(NumBytes);
			if (ensure((int64)NumBytes == ByteCount))
			{
				Archive.Serialize( ByteBuffer, ByteCount );

				Serializer.BeginRead();
				GS::ModelGridSerializer::Restore(*Grid, Serializer);
			}
		}
	}
	else
	{
		GS::MemorySerializer Serializer;
		Serializer.BeginWrite();
		GS::ModelGridSerializer::Serialize(*Grid, Serializer);

		size_t NumBytes = 0;
		const uint8_t* ByteBuffer = Serializer.GetBuffer(NumBytes);

		int64 ByteCount = (int64)NumBytes;
		Archive << ByteCount;
		if (ByteCount > 0)
		{
			Archive.Serialize(const_cast<uint8_t*>(ByteBuffer), ByteCount);
		}
	}

}


void UGSModelGrid::PostLoad()
{
	Super::PostLoad();

	// post change event...is this the right place?
	ModelGridReplacedEvent.Broadcast(this);
}

#if WITH_EDITOR
void UGSModelGrid::PostEditUndo()
{
	Super::PostEditUndo();
	ModelGridReplacedEvent.Broadcast(this);
}
#endif






//
// T3D Encoding/Decoding 
// Used to implement copy/paste and duplication in Editor
// see similar UDynamicMesh.cpp functions for more info
//


namespace GS
{

class FGSModelGridReferenceStorage
{
protected:
	struct FSourceModelGridRef {
		TSoftObjectPtr<UGSModelGrid> SourceGrid;
		FDateTime CreationTimestamp;
	};

	static TMap<int32, FSourceModelGridRef> StoredGrids;
	static FRandomStream KeyGenerator;
public:

	static void Initialize()
	{
		static bool bInitialized = false;
		if (!bInitialized) {
			KeyGenerator = FRandomStream( (int32)FDateTime::UtcNow().ToUnixTimestamp() );
			bInitialized = true;
		}
	}

	static void CleanupCache()
	{
		const int32 ExpiryTimeoutSeconds = 300;		// 5 minutes

		TArray<int32> ToRemove;
		for (TPair<int32, FSourceModelGridRef>& Pair : StoredGrids) {
			if ((FDateTime::Now() - Pair.Value.CreationTimestamp).GetTotalSeconds() > (double)ExpiryTimeoutSeconds)
				ToRemove.Add(Pair.Key);
		}
		for (int32 ID : ToRemove)
			StoredGrids.Remove(ID);
	}

	static int32 StoreObjectReference(UGSModelGrid* SourceGrid)
	{
		Initialize();
		int32 StorageKey = FMath::Abs((int32)KeyGenerator.GetUnsignedInt());

		FSourceModelGridRef NewMesh;
		NewMesh.CreationTimestamp = FDateTime::Now();
		NewMesh.SourceGrid = SourceGrid;
		StoredGrids.Add(StorageKey, MoveTemp(NewMesh));

		CleanupCache();
		return StorageKey;
	}

	static TUniquePtr<GS::ModelGrid> FindObjectReferenceAndGetDataCopy(int32 StorageKey, bool& bFoundOut)
	{
		TUniquePtr<GS::ModelGrid> ResultGrid;
		bFoundOut = false;

		FSourceModelGridRef* Found = StoredGrids.Find(StorageKey);
		if (Found != nullptr && Found->SourceGrid.IsValid())
		{
			ResultGrid = MakeUnique<GS::ModelGrid>();
			UGSModelGrid* SourceGrid = Found->SourceGrid.Get();
			if (SourceGrid) {
				SourceGrid->ProcessGrid([&](const GS::ModelGrid& ReadGrid) {
					*ResultGrid = ReadGrid;
					bFoundOut = true;
				});
			}
		}

		CleanupCache();
		return ResultGrid;
	}
};
// declare static members
TMap<int32, FGSModelGridReferenceStorage::FSourceModelGridRef> FGSModelGridReferenceStorage::StoredGrids;
FRandomStream FGSModelGridReferenceStorage::KeyGenerator;

}



void UGSModelGrid::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	Super::ExportCustomProperties(Out, Indent);
	Out.Logf(TEXT("%sCustomProperties "), FCString::Spc(Indent));
	Out.Logf(TEXT("ModelGridData "));

	// store object reference to circumvent expensive T3D generation/parsing
	int32 StoredObjectKey = GS::FGSModelGridReferenceStorage::StoreObjectReference(this);
	Out.Logf(TEXT("STOREDKEY=%d "), StoredObjectKey);

	// todo: implement base64 encoding like UDynamicMesh

	Out.Logf(TEXT("\r\n"));

}

void UGSModelGrid::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	Super::ImportCustomProperties(SourceText, Warn);

	if (FParse::Command(&SourceText, TEXT("ModelGridData")))	
	{
		// we are overwriting grid
		InitializeNewGrid(false);

		// extract stored object key from string
		static const TCHAR KeyToken[] = TEXT("STOREDKEY=");
		const TCHAR* FoundKeyStart = FCString::Strifind(SourceText, KeyToken);
		if (FoundKeyStart)
		{
			SourceText = FoundKeyStart + FCString::Strlen(KeyToken);
			int32 ObjectKey = FCString::Atoi(SourceText);

			// try to  find grid via key
			bool bFoundValid = false;
			// this data is a copy that we now own
			TUniquePtr<GS::ModelGrid> FoundGrid = GS::FGSModelGridReferenceStorage::FindObjectReferenceAndGetDataCopy(ObjectKey, bFoundValid);

			if (FoundGrid.IsValid() && bFoundValid)
			{
				// do not post notifications from this edit - we are just deserializing, let other things handle it
				this->EditGridInternal([&](GS::ModelGrid& EditGrid) {
					EditGrid = MoveTemp(*FoundGrid);
				});
				return;
			}
		}

		ensureMsgf(false, TEXT("UGSModelGrid: stored object pointer is not valid and T3D Import via text encoding is not supported yet"));
		return;

		// todo: implement base64 decoding like UDynamicMesh
	}
}
