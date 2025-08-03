// Copyright Gradientspace Corp. All Rights Reserved.
#include "GradientspaceUEWidgetsModule.h"

#include "Modules/ModuleManager.h"
#if WITH_EDITOR
#include "PropertyEditorModule.h"
#endif

#define LOCTEXT_NAMESPACE "FGradientspaceUEWidgetsModule"

void FGradientspaceUEWidgetsModule::StartupModule()
{
	PropertiesToUnregisterOnShutdown.Reset();
	ClassesToUnregisterOnShutdown.Reset();

}

void FGradientspaceUEWidgetsModule::ShutdownModule()
{
#if WITH_EDITOR
	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		for (FName ClassName : ClassesToUnregisterOnShutdown)
		{
			PropertyEditorModule->UnregisterCustomClassLayout(ClassName);
		}
		for (FName PropertyName : PropertiesToUnregisterOnShutdown)
		{
			PropertyEditorModule->UnregisterCustomPropertyTypeLayout(PropertyName);
		}
	}
#endif
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGradientspaceUEWidgetsModule, GradientspaceUEWidgets)
