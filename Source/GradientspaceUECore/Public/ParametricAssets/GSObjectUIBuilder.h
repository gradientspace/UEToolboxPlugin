#pragma once

#include "UObject/Object.h"
#include "UObject/Interface.h"

#include "InteractiveToolsContext.h"

#include "GSObjectUIBuilder.generated.h"


class UCombinedTransformGizmo;
class UTransformProxy;

//GRADIENTSPACEUECORE_API


UINTERFACE(MinimalAPI, NotBlueprintable)
class UGSObjectUIBuilder : public UInterface
{
	GENERATED_BODY()
};

class IGSObjectUIBuilder
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ObjectUI")
	GRADIENTSPACEUECORE_API 
	virtual void AddVector3Gizmo(UObject* SourceObject, FString PropertyName, TScriptInterface<IGSObjectUIBuilder>& UIBuilder) {
		UIBuilder = nullptr;
	}

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ObjectUI")
	GRADIENTSPACEUECORE_API 
	virtual void AddTransformGizmo(UObject* SourceObject, FString PropertyName, TScriptInterface<IGSObjectUIBuilder>& UIBuilder) {
		UIBuilder = nullptr;
	}

};






/**
 */
UCLASS(Transient, MinimalAPI, NotBlueprintable)
class UGSObjectUIBuilder_ITF : public UObject, public IGSObjectUIBuilder
{
	GENERATED_BODY()


public:
	virtual void Initialize(UInteractiveToolsContext* ToolsContext);

	virtual void SetWorldTransformSource(TFunction<FTransform()> TransformSourceFunc);
	virtual void SetOnModifiedFunc(TFunction<void(UObject* TargetObject, FString PropertyName)> ObjectModifiedFunc);

	virtual void Reset();


public:
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ObjectUI")
	GRADIENTSPACEUECORE_API 
	virtual void AddVector3Gizmo(UObject* SourceObject, FString PropertyName, TScriptInterface<IGSObjectUIBuilder>& UIBuilder) override;

	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ObjectUI")
	GRADIENTSPACEUECORE_API 
	virtual void AddTransformGizmo(UObject* SourceObject, FString PropertyName, TScriptInterface<IGSObjectUIBuilder>& UIBuilder) override;

protected:

	UPROPERTY()
	TObjectPtr<UInteractiveToolsContext> ToolsContext;

	UPROPERTY()
	TArray<TObjectPtr<UInteractiveGizmo>> GizmoKeepalive;

	TFunction<FTransform()> TransformSourceFunc = []() { return FTransform(); };

	TFunction<void(UObject* TargetObject, FString PropertyName)> ObjectModifiedFunc;

	enum class EPropertyTypes
	{
		Vector3 = 0,
		Transform = 1
	};

	struct FObjectPropertyWidgetInfo
	{
		TWeakObjectPtr<UObject> Object;
		FString PropertyName;
		FProperty* Property;
		EPropertyTypes PropertyType;

		TWeakObjectPtr<UCombinedTransformGizmo> Gizmo;
		TWeakObjectPtr<UTransformProxy> TransformProxy;
	};
	TArray<FObjectPropertyWidgetInfo> Widgets;

};
