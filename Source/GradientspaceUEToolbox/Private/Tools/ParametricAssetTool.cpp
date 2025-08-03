#include "Tools/ParametricAssetTool.h"

#include "InteractiveToolManager.h"

#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/AssetUserData.h"

#include "Editor.h"

#include "ParametricAssets/GSAssetProcessor.h"
#include "ParametricAssets/GSStaticMeshProcessor.h"
#include "ParametricAssets/GSObjectUIBuilder.h"


#define LOCTEXT_NAMESPACE "UGSParametricAssetTool"





static UGSStaticMeshAssetProcessor* FindStaticMeshProcessor(UStaticMesh* StaticMesh)
{
	if (!StaticMesh) return nullptr;
	const TArray<UAssetUserData*>* UserDatas = StaticMesh->GetAssetUserDataArray();
	if (!UserDatas) return nullptr;
	for (UAssetUserData* AssetUserData : *UserDatas)
	{
		if (AssetUserData == nullptr) continue;
		if (UGSStaticMeshAssetProcessor* Processor = Cast<UGSStaticMeshAssetProcessor>(AssetUserData))
			return Processor;
	}
	return nullptr;
}


static TPair<UStaticMesh*,UStaticMeshComponent*> FindStaticMesh(const FToolBuilderState& SceneState)
{
	for (AActor* Actor : SceneState.SelectedActors)
	{
		for (UActorComponent* Component : Actor->GetComponents()) {
			if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Component)) {
				if (SMC->GetStaticMesh() != nullptr)
					return { SMC->GetStaticMesh(), SMC };
			}
		}
	}
	for (UActorComponent* Component : SceneState.SelectedComponents)
	{
		if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Component)) {
			if (SMC->GetStaticMesh() != nullptr)
				return { SMC->GetStaticMesh(), SMC };
		}
	}
	return { nullptr, nullptr };
}

bool UGSParametricAssetToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return (FindStaticMesh(SceneState).Key != nullptr);
}

UInteractiveTool* UGSParametricAssetToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	TPair<UStaticMesh*, UStaticMeshComponent*> Found = FindStaticMesh(SceneState);;
	UStaticMesh* Asset = Found.Key;
	check(Asset != nullptr);
	UGSParametricAssetTool* Tool = NewObject<UGSParametricAssetTool>(SceneState.ToolManager);
	Tool->SetTargetWorld(SceneState.World);
	Tool->SetTargetStaticMesh(Asset);
	Tool->SetOwningComponent(Found.Value);
	return Tool;
}


void UGSParametricAssetTool::SetTargetWorld(UWorld* World) {
	TargetWorld = World;
}
void UGSParametricAssetTool::SetTargetStaticMesh(UStaticMesh* Mesh) {
	TargetStaticMesh = Mesh;
}
void UGSParametricAssetTool::SetOwningComponent(UStaticMeshComponent* Component) {
	OwningComponent = Component;
}

int UGSParametricAssetTool::LastDebugInstanceIdentifier = -1;

void UGSParametricAssetTool::Setup()
{
	static int INSTANCE_COUNTER_GEN = 0;
	DebugInstanceIdentifier = INSTANCE_COUNTER_GEN++;
	LastDebugInstanceIdentifier = DebugInstanceIdentifier;

	Settings = NewObject<UGSParametricAssetTool_Settings>(this);
	// restore properties?
	AddToolPropertySource(Settings);

	Settings->Asset = TargetStaticMesh.Get();
	UpdateActiveAssetProcessor(Settings->Asset);	

	TSharedPtr<FGSActionButtonTarget> TempActionsTarget = MakeShared<FGSActionButtonTarget>();
	TempActionsTarget->ExecuteCommand = [this](FString CommandName) { OnActionButtonClicked(CommandName); };
	TempActionsTarget->IsCommandEnabled = [this](FString CommandName) { return OnActionButtonIsEnabled(CommandName); };
	Settings->Actions.Target = TempActionsTarget;
	Settings->Actions.AddAction(TEXT("Rebuild"), LOCTEXT("Rebuild", "Rebuild"),
		LOCTEXT("RebuildTooltip", "Rebuild Tooltip"));
	Settings->Actions.SetUIPreset_LargeSize();
	Settings->Actions.PadLeft = Settings->Actions.PadRight = 15;


	ManagementProps = NewObject<UGSParametricAssetTool_ManageProcessor>(this);
	AddToolPropertySource(ManagementProps);

	TSharedPtr<FGSActionButtonTarget> ManageActionsTarget = MakeShared<FGSActionButtonTarget>();
	ManageActionsTarget->ExecuteCommand = [this](FString CommandName) { OnActionButtonClicked(CommandName); };
	ManageActionsTarget->IsCommandEnabled = [this](FString CommandName) { return OnActionButtonIsEnabled(CommandName); };
	ManageActionsTarget->IsCommandVisible = [this](FString CommandName) { return OnActionButtonIsVisible(CommandName); };
	ManagementProps->Actions.Target = ManageActionsTarget;
	ManagementProps->Actions.AddAction(TEXT("AddProcessor"), LOCTEXT("AddProcessorLabel", "Add Processor"),
		LOCTEXT("AddProcessorTooltip", "Add Processor Tooltip"));
	ManagementProps->Actions.AddAction(TEXT("RemoveProcessor"), LOCTEXT("RemoveProcessorLabel", "Remove Processor"),
		LOCTEXT("RemoveProcessorTooltip", "RemoveProcessor Tooltip"));


	UIBuilder = NewObject<UGSObjectUIBuilder_ITF>(this);
	UIBuilder->Initialize( Cast<UInteractiveToolsContext>(GetToolManager()->GetOuter()) );
	UIBuilder->SetWorldTransformSource(
		[this]() { return OwningComponent.IsValid() ? OwningComponent->GetComponentTransform() : FTransform(); });

	UIBuilder->SetOnModifiedFunc(
		[this](UObject*, FString) { OnSettingsModified(); });

	NumBasePropertySets = 2;
	UpdateParametricObjectPropertySets();

	BlueprintPreCompileHandle = GEditor->OnBlueprintPreCompile().AddUObject(this, &UGSParametricAssetTool::OnBlueprintPreCompile);
	BlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddUObject(this, &UGSParametricAssetTool::OnBlueprintCompiled);
}

void UGSParametricAssetTool::Shutdown(EToolShutdownType ShutdownType)
{
	if (UIBuilder) {
		UIBuilder->Reset();
		UIBuilder = nullptr;
	}

	DisconnectFromParametricObjectPropertySets();

	// disconnect from current asset processor
	UpdateActiveAssetProcessor(nullptr);

	GEditor->OnBlueprintPreCompile().Remove(BlueprintPreCompileHandle);
	GEditor->OnBlueprintCompiled().Remove(BlueprintCompiledHandle);

	TargetStaticMesh = nullptr;
	OwningComponent = nullptr;
	TargetWorld = nullptr;

	// force GC on shutdown of this tool to try to detect dangling-stuff errors 
	GEngine->ForceGarbageCollection(true);
}

void UGSParametricAssetTool::OnTick(float DeltaTime)
{
	static int TickCounter = 0;
	static FDateTime LastUpdateTime = FDateTime::Now();
	if (Settings->bEnableAutomaticRebuild && bRebuildPending && TickCounter++ % 2 == 0 &&
		(FDateTime::Now() - LastUpdateTime).GetTotalSeconds() > 0.25)
	{
		OnActionButtonClicked(TEXT("Rebuild"));
		LastUpdateTime = FDateTime::Now();
	}
}

void UGSParametricAssetTool::Render(IToolsContextRenderAPI* RenderAPI)
{
}

void UGSParametricAssetTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
}


void UGSParametricAssetTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
}


void UGSParametricAssetTool::UpdateActiveAssetProcessor(UStaticMesh* StaticMesh)
{
	auto DisconnectExisting = [this]()
	{
		if (ActiveAssetProcessorListener.IsValid()) {
			if (UGSStaticMeshAssetProcessor* CurProcessor = ActiveAssetProcessor.Get())
				CurProcessor->OnParameterModified.Remove(ActiveAssetProcessorListener);
			ActiveAssetProcessorListener.Reset();
		}
		ActiveAssetProcessor.Reset();
	};

	UGSStaticMeshAssetProcessor* CurSMProcessor = (StaticMesh != nullptr) ? FindStaticMeshProcessor(StaticMesh) : nullptr;
	if (StaticMesh == nullptr || CurSMProcessor == nullptr) {
		DisconnectExisting();
		return;
	}
	if (CurSMProcessor == ActiveAssetProcessor.Get())
		return;
	
	// need to connect to new, non-null processor
	DisconnectExisting();
	ActiveAssetProcessor = CurSMProcessor;
	ActiveAssetProcessorListener = 
		CurSMProcessor->OnParameterModified.AddUObject(this, &UGSParametricAssetTool::OnAssetProcessorSetupModified);
}
void UGSParametricAssetTool::OnAssetProcessorSetupModified(UGSAssetProcessorUserData* Processor)
{
	UpdateParametricObjectPropertySets();
}


void UGSParametricAssetTool::DisconnectFromParametricObjectPropertySets()
{
	// un-listen to parameter changes
	for (int k = 0; k < ActivePropertyModifiedListeners.Num(); ++k)
	{
		if (UGSObjectProcessor* ObjProcessor = ActivePropertyModifiedSources[k].Get())
			ObjProcessor->OnParameterModified.Remove(ActivePropertyModifiedListeners[k]);
	}
	ActivePropertyModifiedListeners.Reset();
	ActivePropertyModifiedSources.Reset();

	// remove existing parameter objects
	while (ToolPropertyObjects.Num() > NumBasePropertySets) {
		ToolPropertyObjects.Pop(false);
	}
}

void UGSParametricAssetTool::UpdateParametricObjectPropertySets()
{
	UpdateTargetInfo();

	UIBuilder->Reset();
	TScriptInterface<IGSObjectUIBuilder> UIBuilderInterface = TScriptInterface<IGSObjectUIBuilder>(UIBuilder);

	DisconnectFromParametricObjectPropertySets();

	// this makes calling BP from C++ safe, somehow?
	FEditorScriptExecutionGuard Guard;

	// re-add 
	const TArray<UAssetUserData*>* AssetUserDatas = Settings->Asset->GetAssetUserDataArray();
	if (AssetUserDatas) {
		for (UAssetUserData* Data : *AssetUserDatas)
		{
			if (UGSStaticMeshAssetProcessor* SMData = Cast<UGSStaticMeshAssetProcessor>(Data))
			{
				ToolPropertyObjects.Add(SMData);

				TArray<UObject*> ChildObjects;
				SMData->OnCollectChildParameterObjects(ChildObjects);
				for (UObject* Obj : ChildObjects) {

					if (UGSObjectProcessor* ObjProcessor = Cast<UGSObjectProcessor>(Obj))
					{
						FDelegateHandle Handle = ObjProcessor->OnParameterModified.AddLambda([this](UGSObjectProcessor*) { OnSettingsModified(); });
						ActivePropertyModifiedListeners.Add(Handle);
						ActivePropertyModifiedSources.Add(ObjProcessor);

						ObjProcessor->OnRegisterPropertyWidgets(UIBuilderInterface);
					}

					ToolPropertyObjects.Add(Obj);
				}
			}
		}
	}

	OnPropertySetsModified.Broadcast();
}







bool UGSParametricAssetTool::OnAddProcessor(UStaticMesh* TargetMesh, TSubclassOf<UGSStaticMeshAssetProcessor> ProcessorType)
{
	UGSStaticMeshAssetProcessor* FoundProcessor = FindStaticMeshProcessor(TargetMesh);
	if (FoundProcessor != nullptr)
		return false;

	UClass* ProcessorClass = ProcessorType;
	if (ProcessorClass && TargetMesh) {
		UGSStaticMeshAssetProcessor* NewProcessor = NewObject<UGSStaticMeshAssetProcessor>(TargetMesh, ProcessorClass);
		if (NewProcessor != nullptr)
		{
			GetToolManager()->BeginUndoTransaction(LOCTEXT("AddProcessorAction", "Add Processor"));
			TargetMesh->Modify();
			TargetMesh->AddAssetUserData(NewProcessor);
			GetToolManager()->EndUndoTransaction();
			UpdateActiveAssetProcessor(TargetMesh);
			return true;
		}
	}
	return false;
}

void UGSParametricAssetTool::OnRemoveProcessor(UStaticMesh* TargetMesh)
{
	UGSStaticMeshAssetProcessor* FoundProcessor = FindStaticMeshProcessor(TargetMesh);
	if (FoundProcessor != nullptr) {
		GetToolManager()->BeginUndoTransaction(LOCTEXT("RemoveProcessor", "Remove Processor"));
		TargetMesh->Modify();
		TargetMesh->RemoveUserDataOfClass(FoundProcessor->GetClass());
		GetToolManager()->EndUndoTransaction();
		UpdateActiveAssetProcessor(TargetMesh);
	}
}



bool UGSParametricAssetTool::OnExecuteProcessor(UStaticMesh* TargetMesh, bool bEmitTransaction)
{
	UStaticMesh* StaticMesh = TargetStaticMesh.Get();
	if (!StaticMesh) 
		return false;

	UGSStaticMeshAssetProcessor* Processor = StaticMesh->GetAssetUserData<UGSStaticMeshAssetProcessor>();
	if (!Processor)
		return false;

	bool bProcessed = false;

	if (bEmitTransaction) {
		GetToolManager()->BeginUndoTransaction(LOCTEXT("RunProcessor", "Run Processor"));
		StaticMesh->Modify();
	}
	
	Processor->OnProcessStaticMeshAsset(StaticMesh);
	bProcessed = true;

	if (bEmitTransaction)
		GetToolManager()->EndUndoTransaction();

	return bProcessed;
}


void UGSParametricAssetTool::OnActionButtonClicked(FString CommandName)
{
	if (CommandName == TEXT("AddProcessor"))
	{
		OnAddProcessor(TargetStaticMesh.Get(), ManagementProps->ProcessorType);
		UpdateParametricObjectPropertySets();
	}
	else if (CommandName == TEXT("RemoveProcessor"))
	{
		OnRemoveProcessor(TargetStaticMesh.Get());
		UpdateParametricObjectPropertySets();
	}
	else if (CommandName == TEXT("Rebuild"))
	{
		if (UStaticMesh* StaticMesh = TargetStaticMesh.Get())
			OnExecuteProcessor(StaticMesh, false);
		bRebuildPending = false;
	}
}


bool UGSParametricAssetTool::OnActionButtonIsEnabled(FString CommandName) const
{
	if (CommandName == TEXT("AddProcessor")) {
		return !ManagementProps->bTargetHasProcessor;
	}
	else if (CommandName == TEXT("RemoveProcessor")) {
		return ManagementProps->bTargetHasProcessor;
	}
	else 
		return true;
}

bool UGSParametricAssetTool::OnActionButtonIsVisible(FString CommandName) const
{
	if (CommandName == TEXT("AddProcessor")) {
		return !ManagementProps->bTargetHasProcessor;
	}
	else if (CommandName == TEXT("RemoveProcessor")) {
		return ManagementProps->bTargetHasProcessor;
	}
	else
		return true;
}



void UGSParametricAssetTool::UpdateTargetInfo()
{
	ManagementProps->bTargetHasProcessor = ActiveAssetProcessor.IsValid();
}
void UGSParametricAssetTool::PostEditUndo()
{
	UpdateActiveAssetProcessor(TargetStaticMesh.Get());
	// above will do this... (maybe?)
	UpdateTargetInfo();
}



void UGSParametricAssetTool::OnBlueprintPreCompile(UBlueprint* Blueprint)
{
	// TODO: figure out if this Blueprint is relevant to us? could be very difficult due to
	// connected dependencies...
	bRebuildPending = true;
}

void UGSParametricAssetTool::OnBlueprintCompiled()
{
}



#undef LOCTEXT_NAMESPACE