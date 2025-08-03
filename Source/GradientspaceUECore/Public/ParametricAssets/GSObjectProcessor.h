#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "ParametricAssets/GSObjectUIBuilder.h"

#include "GSObjectProcessor.generated.h"


/**
 * Base class for Object Processors
 */
UCLASS(MinimalAPI, Abstract)
class UGSObjectProcessor : public UObject
{
	GENERATED_BODY()
public:


	DECLARE_MULTICAST_DELEGATE_OneParam(FOnObjectProcessorParameterModified, UGSObjectProcessor*);
	FOnObjectProcessorParameterModified OnParameterModified;

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



public:

	/**
	 * Implement this callback to register widgets for BP instance variables via the UIBuilder
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "ObjectProcessor|Events")
	GRADIENTSPACEUECORE_API void OnRegisterPropertyWidgets(const TScriptInterface<IGSObjectUIBuilder>& UIBuilder);
	virtual void OnRegisterPropertyWidgets_Implementation(const TScriptInterface<IGSObjectUIBuilder>& UIBuilder) {}

};
