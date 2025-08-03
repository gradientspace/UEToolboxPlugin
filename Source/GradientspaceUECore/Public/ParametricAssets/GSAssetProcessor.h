#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/AssetUserData.h"

#include "GSAssetProcessor.generated.h"


/**
 * Base class for Asset processors, which can be assigned to UE Assets
 * as UAssetUserData.
 */
UCLASS(MinimalAPI, Abstract)	// note: inherits DefaultToInstanced and EditInlineNew from UAssetUserData
class UGSAssetProcessorUserData : public UAssetUserData
{
	GENERATED_BODY()
public:

	/**
	 * CollectChildParameterObjects is used by UI-type things to determine which child objects
	 * of this Processor might contain parameters that should be exposed in (eg) details panels, etc
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Gradientspace|Asset Processor")
	GRADIENTSPACEUECORE_API void OnCollectChildParameterObjects(UPARAM(ref) TArray<UObject*>& ChildObjectsInOut);
	virtual void OnCollectChildParameterObjects_Implementation(TArray<UObject*>& ChildObjectsInOut) { }




	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssetProcessorParameterModified, UGSAssetProcessorUserData*);
	FOnAssetProcessorParameterModified OnParameterModified;

	virtual void BroadcastParameterModified()
	{
		OnParameterModified.Broadcast(this);
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		BroadcastParameterModified();
	}
#endif
};
