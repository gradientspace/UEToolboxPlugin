#pragma once

#include "InteractiveToolBuilder.h"
#include "InteractiveTool.h"
#include "PropertyTypes/ActionButtonGroup.h"

#include "ParametricAssetTool.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UGSObjectProcessor;
class UGSAssetProcessorUserData;
class UGSStaticMeshAssetProcessor;
class UBlueprint;
class UGSObjectUIBuilder_ITF;

UCLASS()
class GRADIENTSPACEUETOOLBOX_API UGSParametricAssetToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};



UCLASS(MinimalAPI)
class UGSParametricAssetTool_Settings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	UPROPERTY(VisibleAnywhere, Category = "Target Object", meta=(TransientToolProperty, NoResetToDefault, DisplayName="Static Mesh Asset"))
	TObjectPtr<UStaticMesh> Asset;

	UPROPERTY(EditAnywhere, Category = "Target Object", meta=(DisplayName="AutoRebuild on Changes"))
	bool bEnableAutomaticRebuild = true;

	UPROPERTY(EditAnywhere, Category = "Target Object", meta = (TransientToolProperty, NoResetToDefault))
	FGSActionButtonGroup Actions;
};



UCLASS(MinimalAPI)
class UGSParametricAssetTool_ManageProcessor : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Manage Processor", meta=(EditCondition="bTargetHasProcessor==false", EditConditionHides, HideEditConditionToggle, NoResetToDefault))
	TSubclassOf<UGSStaticMeshAssetProcessor> ProcessorType;

	UPROPERTY(EditAnywhere, Category = "Manage Processor", meta = (TransientToolProperty, NoResetToDefault))
	FGSActionButtonGroup Actions;

	UPROPERTY(meta = (TransientToolProperty))
	bool bTargetHasProcessor = false;
};







UCLASS()
class UGSParametricAssetTool : public UInteractiveTool
	//public IInteractiveToolCameraFocusAPI
{
	GENERATED_BODY()
public:
	void SetTargetWorld(UWorld* World);
	void SetTargetStaticMesh(UStaticMesh* Mesh);
	void SetOwningComponent(UStaticMeshComponent* Component);

	virtual void Setup() override;
	//virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;


	virtual bool OnAddProcessor(UStaticMesh* TargetMesh, TSubclassOf<UGSStaticMeshAssetProcessor> ProcessorType);
	virtual void OnRemoveProcessor(UStaticMesh* TargetMesh);
	virtual bool OnExecuteProcessor(UStaticMesh* TargetMesh, bool bEmitTransaction);

protected:
	UPROPERTY()
	TObjectPtr<UGSParametricAssetTool_Settings> Settings;

	UPROPERTY()
	TObjectPtr<UGSParametricAssetTool_ManageProcessor> ManagementProps;
	
	int NumBasePropertySets = 0;

	UPROPERTY()
	TObjectPtr<UGSObjectUIBuilder_ITF> UIBuilder;

protected:
	TWeakObjectPtr<UWorld> TargetWorld;

	TWeakObjectPtr<UStaticMesh> TargetStaticMesh;

	TWeakObjectPtr<UStaticMeshComponent> OwningComponent;	

	TArray<FDelegateHandle> ActivePropertyModifiedListeners;
	TArray<TWeakObjectPtr<UGSObjectProcessor>> ActivePropertyModifiedSources;


	FDelegateHandle ActiveAssetProcessorListener;
	TWeakObjectPtr<UGSStaticMeshAssetProcessor> ActiveAssetProcessor;
	void UpdateActiveAssetProcessor(UStaticMesh* StaticMesh);
	void OnAssetProcessorSetupModified(UGSAssetProcessorUserData* Processor);


	virtual void OnActionButtonClicked(FString CommandName);

	virtual bool OnActionButtonIsEnabled(FString CommandName) const;
	virtual bool OnActionButtonIsVisible(FString CommandName) const;

	virtual void UpdateParametricObjectPropertySets();
	virtual void DisconnectFromParametricObjectPropertySets();

	bool bRebuildPending = false;
	virtual void OnSettingsModified() { bRebuildPending = true; }


	void UpdateTargetInfo();

	// this is just for debugging, to detect things happening in shutdown instances of tool (which keeps happening...)
	int DebugInstanceIdentifier = 0;
	static int LastDebugInstanceIdentifier;

#if WITH_EDITOR
	virtual void PostEditUndo() override;

	void OnBlueprintPreCompile(UBlueprint* Blueprint);
	FDelegateHandle BlueprintPreCompileHandle;

	void OnBlueprintCompiled();
	FDelegateHandle BlueprintCompiledHandle;
#endif
};
