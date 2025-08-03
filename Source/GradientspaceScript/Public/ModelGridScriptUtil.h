// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "ModelGrid/ModelGridTypes.h"
#include "ModelGridScriptTypes.h"

namespace GS
{

inline EModelGridBPCellType ConvertToBPCellType(GS::EModelGridCellType CellType)
{
	return ((int)CellType <= (int)EModelGridBPCellType::LastKnownType) ? (EModelGridBPCellType)(int)CellType : EModelGridBPCellType::Unknown;
}


}

