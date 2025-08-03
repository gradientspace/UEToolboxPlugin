// Copyright Gradientspace Corp. All Rights Reserved.
#include "Settings/GSToolSubsystem.h"

#include "Engine/Engine.h"

UGSToolSubsystem* UGSToolSubsystem::Get()
{
	UGSToolSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGSToolSubsystem>();
	ensure(Subsystem);
	return Subsystem;
}

void UGSToolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	DefaultImpl = MakeShared<GSDefaultToolSubsystemAPI>();
	SetActiveAPI(DefaultImpl.Get());
}

void UGSToolSubsystem::SetActiveAPI(IGSToolSubsystemAPI* NewImplementation)
{
	check(NewImplementation != nullptr);
	Impl = NewImplementation;
}

void UGSToolSubsystem::RegisterToolHotkeyBindings(UInteractiveTool* Tool)
{
	Impl->RegisterToolHotkeyBindings(Tool);
}
void UGSToolSubsystem::UnRegisterToolHotkeyBindings(UInteractiveTool* Tool)
{
	Impl->UnRegisterToolHotkeyBindings(Tool);
}

void UGSToolSubsystem::EnableToolFeedbackWidget(UInteractiveTool* Tool)
{
	Impl->EnableToolFeedbackWidget(Tool);
}
void UGSToolSubsystem::SetToolFeedbackStrings(const GS::GSErrorSet& ErrorSet)
{
	Impl->SetToolFeedbackStrings(ErrorSet);
}
void UGSToolSubsystem::DisableToolFeedbackWidget(UInteractiveTool* Tool)
{
	Impl->DisableToolFeedbackWidget(Tool);
}








void GSDefaultToolSubsystemAPI::RegisterToolHotkeyBindings(UInteractiveTool* Tool)
{
}
void GSDefaultToolSubsystemAPI::UnRegisterToolHotkeyBindings(UInteractiveTool* Tool)
{
}
void GSDefaultToolSubsystemAPI::EnableToolFeedbackWidget(UInteractiveTool* Tool)
{
}
void GSDefaultToolSubsystemAPI::SetToolFeedbackStrings(const GS::GSErrorSet& ErrorSet)
{
}
void GSDefaultToolSubsystemAPI::DisableToolFeedbackWidget(UInteractiveTool* Tool)
{
}