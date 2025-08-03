// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"
#include "GradientspaceUELogging.h"


#define CHECK_GRID_VALID_OR_RETURN(Grid, FunctionName) \
	if ( Grid == nullptr ) { \
		UE_LOG(LogGradientspace, Warning, TEXT("[GSScript:%s] Target Grid is null"), FunctionName ); \
		return Grid; \
	}

#define CHECK_MESH_VALID_OR_RETURN(Mesh, FunctionName) \
	if ( Mesh == nullptr ) { \
		UE_LOG(LogGradientspace, Warning, TEXT("[GSScript:%s] Target Mesh is null"), FunctionName ); \
		return Mesh; \
	}

#define CHECK_OBJ_VALID_OR_RETURN_OTHER(Object, ReturnObject, ObjectName, FunctionName) \
	if ( Object == nullptr ) { \
		UE_LOG(LogGradientspace, Warning, TEXT("[GSScript:%s] %s is null"), FunctionName, ObjectName ); \
		return ReturnObject; \
	}

#define CHECK_DELEGATE_BOUND_OR_RETURN_OBJECT(Delegate, ReturnObject, DelegateName, FunctionName) \
	if ( Delegate.IsBound() == false ) { \
		UE_LOG(LogGradientspace, Warning, TEXT("[GSScript:%s] Delegate %s is not bound, returning"), FunctionName, DelegateName ); \
		return ReturnObject; \
	}

#define CHECK_DELEGATE_BOUND_OR_RETURN(Delegate, DelegateName, FunctionName) \
	if ( Delegate.IsBound() == false ) { \
		UE_LOG(LogGradientspace, Warning, TEXT("[GSScript:%s] Delegate %s is not bound, returning"), FunctionName, DelegateName ); \
		return; \
	}

#define GS_LOG_SCRIPT_ERROR(FunctionName, Message) \
	UE_LOG(LogGradientspace, Warning, TEXT("[GSScript:%s] %s"), FunctionName, Message );