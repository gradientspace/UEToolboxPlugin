#include "ParametricAssets/GSObjectUIBuilder.h"

#include "BaseGizmos/CombinedTransformGizmo.h"
#include "BaseGizmos/TransformGizmoUtil.h"

namespace Local
{

FStructProperty* FindValidStructProperty(
	UObject* Object, FString PropertyName, FString CppTypeName)
{
	if (Object == nullptr ) {
		UE_LOG(LogTemp, Warning, TEXT("Object for property %s is null"), *PropertyName);
		return nullptr;
	}

	for (TFieldIterator<FProperty> PropIt(Object->GetClass(), EFieldIterationFlags::Default); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (Property->GetName() == PropertyName)
		{
			FString PropertyCPPType = Property->GetCPPType(nullptr, 0);
			if (FStructProperty* StructProp = CastField<FStructProperty>(*PropIt))
			{
				if (PropertyCPPType == CppTypeName)
					return StructProp;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Property %s is not a StructProperty with CppType %s (is %s)"), *PropertyName, *CppTypeName, *PropertyCPPType);
				return nullptr;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Could not find a Property named %s of CppType %s"), *PropertyName, *CppTypeName);
	return nullptr;
}

}



void UGSObjectUIBuilder_ITF::Initialize(UInteractiveToolsContext* ToolsContextIn)
{
	this->ToolsContext = ToolsContextIn;
}


void UGSObjectUIBuilder_ITF::SetWorldTransformSource(TFunction<FTransform()> TransformSourceFuncIn)
{
	TransformSourceFunc = TransformSourceFuncIn;
	// todo update any existing gizmos?
}

void UGSObjectUIBuilder_ITF::SetOnModifiedFunc(TFunction<void(UObject* TargetObject, FString PropertyName)> ObjectModifiedFuncIn)
{
	ObjectModifiedFunc = ObjectModifiedFuncIn;
}


void UGSObjectUIBuilder_ITF::Reset()
{
	for (FObjectPropertyWidgetInfo& WidgetInfo : Widgets) {
		if ( WidgetInfo.Gizmo.IsValid() )
			ToolsContext->GizmoManager->DestroyGizmo( WidgetInfo.Gizmo.Get() );
	}
	ToolsContext->GizmoManager->DestroyAllGizmosByOwner(this);

	Widgets.Reset();
	GizmoKeepalive.Reset();
}


void UGSObjectUIBuilder_ITF::AddVector3Gizmo(UObject* SourceObject, FString PropertyName, TScriptInterface<IGSObjectUIBuilder>& UIBuilder)
{
	UIBuilder = TScriptInterface<IGSObjectUIBuilder>(this);

	FStructProperty* StructProperty = Local::FindValidStructProperty(SourceObject, PropertyName, TEXT("FVector"));
	if (!StructProperty) {
		// print error etc
		return;	
	}

	FObjectPropertyWidgetInfo NewWidgetInfo;
	NewWidgetInfo.Object = SourceObject;
	NewWidgetInfo.PropertyName = PropertyName;
	NewWidgetInfo.Property = StructProperty;
	NewWidgetInfo.PropertyType = EPropertyTypes::Vector3;

	const FVector* ValuePtr = StructProperty->ContainerPtrToValuePtr<FVector>(SourceObject);
	FVector LocalPosition = (ValuePtr != nullptr) ? *ValuePtr : FVector::Zero();

	FTransform InitialTransform = TransformSourceFunc();
	FVector WorldPos = InitialTransform.TransformPosition(LocalPosition);
	InitialTransform.SetTranslation(WorldPos);

	//ETransformGizmoSubElements GizmoElements = ETransformGizmoSubElements::StandardTranslateRotate;
	ETransformGizmoSubElements GizmoElements = ETransformGizmoSubElements::TranslateAllAxes | ETransformGizmoSubElements::TranslateAllPlanes;
	UCombinedTransformGizmo* NewGizmo = UE::TransformGizmoUtil::CreateCustomTransformGizmo(
		ToolsContext->ToolManager, GizmoElements, this, FString::Printf(TEXT("%s.%s"), *SourceObject->GetName(), *PropertyName));
	NewGizmo->ActiveGizmoMode = EToolContextTransformGizmoMode::Translation;
	NewGizmo->bUseContextCoordinateSystem = true;

	UTransformProxy* GizmoProxy = NewObject<UTransformProxy>(this);
	GizmoProxy->SetTransform(InitialTransform);
	NewGizmo->SetActiveTarget(GizmoProxy, ToolsContext->ToolManager);		// do we always want to route through ToolManager here?

	// todo these need to be safer!
	GizmoProxy->OnTransformChanged.AddWeakLambda(this, [this, StructProperty, SourceObject, PropertyName](UTransformProxy*, FTransform NewTransform)
	{
		FVector NewWorldPos = NewTransform.GetLocation();
		FTransform InitialTransform = TransformSourceFunc();
		FVector NewLocalPos = InitialTransform.InverseTransformPosition(NewWorldPos);

		// todo need to open transaction...

		FVector* ValuePtr = StructProperty->ContainerPtrToValuePtr<FVector>(SourceObject);
		*ValuePtr = NewLocalPos;

		if (this->ObjectModifiedFunc)
			this->ObjectModifiedFunc(SourceObject, PropertyName);

	});
	//GizmoProxy->OnBeginTransformEdit.AddWeakLambda(this, [this](UTransformProxy* Proxy)
	//{
	//});
	//GizmoProxy->OnEndTransformEdit.AddWeakLambda(this, [this](UTransformProxy* Proxy)
	//{
	//});
	//GizmoProxy->OnTransformChangedUndoRedo.AddWeakLambda(this, [this](UTransformProxy*, FTransform NewTransform)
	//{
	//});

	GizmoKeepalive.Add(NewGizmo);
	NewWidgetInfo.Gizmo = NewGizmo;
	NewWidgetInfo.TransformProxy = GizmoProxy;
	Widgets.Add(NewWidgetInfo);
}





void UGSObjectUIBuilder_ITF::AddTransformGizmo(UObject* SourceObject, FString PropertyName, TScriptInterface<IGSObjectUIBuilder>& UIBuilder)
{
	UIBuilder = TScriptInterface<IGSObjectUIBuilder>(this);

	FStructProperty* StructProperty = Local::FindValidStructProperty(SourceObject, PropertyName, TEXT("FTransform"));
	if (!StructProperty) {
		// print error etc
		return;
	}

	FObjectPropertyWidgetInfo NewWidgetInfo;
	NewWidgetInfo.Object = SourceObject;
	NewWidgetInfo.PropertyName = PropertyName;
	NewWidgetInfo.Property = StructProperty;
	NewWidgetInfo.PropertyType = EPropertyTypes::Transform;

	const FTransform* ValuePtr = StructProperty->ContainerPtrToValuePtr<FTransform>(SourceObject);
	FTransform LocalTransform = (ValuePtr != nullptr) ? *ValuePtr : FTransform();

	FTransform InitialTransform = TransformSourceFunc();
	FQuat WorldRotation = InitialTransform.TransformRotation(LocalTransform.GetRotation());
	FVector WorldPos = InitialTransform.TransformPosition(LocalTransform.GetLocation());
	InitialTransform.SetTranslation(WorldPos);
	InitialTransform.SetRotation(WorldRotation);

	ETransformGizmoSubElements GizmoElements = ETransformGizmoSubElements::StandardTranslateRotate;
	UCombinedTransformGizmo* NewGizmo = UE::TransformGizmoUtil::CreateCustomTransformGizmo(
		ToolsContext->ToolManager, GizmoElements, this, FString::Printf(TEXT("%s.%s"), *SourceObject->GetName(), *PropertyName));
	NewGizmo->ActiveGizmoMode = EToolContextTransformGizmoMode::Combined;
	NewGizmo->bUseContextCoordinateSystem = true;

	UTransformProxy* GizmoProxy = NewObject<UTransformProxy>(this);
	GizmoProxy->SetTransform(InitialTransform);
	NewGizmo->SetActiveTarget(GizmoProxy, ToolsContext->ToolManager);		// do we always want to route through ToolManager here?

	// todo these need to be safer!
	GizmoProxy->OnTransformChanged.AddWeakLambda(this, [this, StructProperty, SourceObject, PropertyName](UTransformProxy*, FTransform NewTransform)
	{
		FVector NewWorldPos = NewTransform.GetLocation();
		FQuat NewWorldRot = NewTransform.GetRotation();
		FTransform InitialTransform = TransformSourceFunc();
		FVector NewLocalPos = InitialTransform.InverseTransformPosition(NewWorldPos);
		FQuat NewLocalRot = InitialTransform.InverseTransformRotation(NewWorldRot);
		FTransform NewLocalTransform(NewLocalRot, NewLocalPos);

		// todo need to open transaction...

		FTransform* ValuePtr = StructProperty->ContainerPtrToValuePtr<FTransform>(SourceObject);
		*ValuePtr = NewLocalTransform;

		if (this->ObjectModifiedFunc)
			this->ObjectModifiedFunc(SourceObject, PropertyName);

	});
	//GizmoProxy->OnBeginTransformEdit.AddWeakLambda(this, [this](UTransformProxy* Proxy)
	//{
	//});
	//GizmoProxy->OnEndTransformEdit.AddWeakLambda(this, [this](UTransformProxy* Proxy)
	//{
	//});
	//GizmoProxy->OnTransformChangedUndoRedo.AddWeakLambda(this, [this](UTransformProxy*, FTransform NewTransform)
	//{
	//});

	GizmoKeepalive.Add(NewGizmo);
	NewWidgetInfo.Gizmo = NewGizmo;
	NewWidgetInfo.TransformProxy = GizmoProxy;
	Widgets.Add(NewWidgetInfo);
}