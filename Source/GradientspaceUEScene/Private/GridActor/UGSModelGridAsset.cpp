// Copyright Gradientspace Corp. All Rights Reserved.
#include "GridActor/UGSModelGridAsset.h"


UGSModelGridAsset::UGSModelGridAsset()
{
	ModelGrid = CreateDefaultSubobject<UGSModelGrid>(TEXT("ModelGrid"));
	InternalMaterialSet = CreateDefaultSubobject<UGSGridMaterialSet>(TEXT("InternalMaterialSet"));
}

