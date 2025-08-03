// Copyright Gradientspace Corp. All Rights Reserved.
#include "GridActor/UGSGridMaterialSet.h"

#include "Materials/Material.h"
#include "MaterialDomain.h"

using namespace GS;








void FReferenceSetMaterialMap::AppendMappedMaterial(uint32_t UseInternalID, UMaterialInterface* Material, const FString* UseName)
{
	int NewIndex = MaterialList.Num();
	InternalIDToMaterialIDMap.Add(UseInternalID, NewIndex);

	InternalIDList.Add(UseInternalID);
	MaterialList.Add(Material);
	NameList.Add( (UseName != nullptr) ? *UseName : Material->GetName() );
}

int FReferenceSetMaterialMap::GetMaterialID(GS::EGridCellMaterialType MaterialType, GS::GridMaterial Material)
{
	const int32* Found = nullptr;
	if (MaterialType == EGridCellMaterialType::SolidRGBIndex)
	{
		Found = InternalIDToMaterialIDMap.Find(Material.GetIndex8());
	}
	return (Found != nullptr) ? *Found : 0;
}





UGSGridMaterialSet::UGSGridMaterialSet()
{
	InitializeDefaultMaterial(DefaultMaterial);
}


namespace GSInternal
{
	static FGSGridExternalMaterial GetBuiltInDefaultMaterial()
	{
		FGSGridExternalMaterial Mat;
		//Mat.Material = LoadObject<UMaterial>(nullptr, TEXT("/GradientspaceUEToolbox/Materials/M_GridEditMaterial"));
		Mat.Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/GradientspaceUEToolbox/Materials/M_GridPreviewMaterial"));
		if (Mat.Material == nullptr)
			Mat.Material = UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);
		Mat.Name = TEXT("Default (Color)");
		Mat.MaterialID.ReferenceType = (int)GS::EMaterialReferenceType::DefaultMaterial;
		Mat.MaterialID.Index = 0;
		return Mat;
	}
}

void UGSGridMaterialSet::InitializeDefaultMaterial(FGSGridExternalMaterial& GridMaterial)
{
	GridMaterial = GSInternal::GetBuiltInDefaultMaterial();
}

void UGSGridMaterialSet::SetDefaultMaterial(UMaterialInterface* Material, FString NewName)
{
	DefaultMaterial.Material = Material;
	if (NewName.Len() > 0)
		DefaultMaterial.Name = NewName;
}

FGSGridMaterialID UGSGridMaterialSet::FindOrAddExternalMaterial(UMaterialInterface* Material)
{
	for (FGSGridExternalMaterial& Mat : ExternalMaterials)
	{
		if (Mat.Material == Material)
			return Mat.MaterialID;
	}

	uint32 NewID = ReferenceSet.RegisterExternalMaterial((uint64_t)Material);
	GS::MaterialReferenceID RefID(NewID);

	FGSGridExternalMaterial NewMat;
	NewMat.MaterialID.ReferenceType = (int32)RefID.MaterialType;
	NewMat.MaterialID.Index = (int32)RefID.Index;
	NewMat.Material = Material;
	ExternalMaterials.Add(NewMat);

	return NewMat.MaterialID;
}



UMaterialInterface* UGSGridMaterialSet::FindExternalMaterial(FGSGridMaterialID GridMaterialID)
{
	for (FGSGridExternalMaterial& Mat : ExternalMaterials)
	{
		if (Mat.MaterialID == GridMaterialID)
			return Mat.Material;
	}
	return nullptr;
}


void UGSGridMaterialSet::CopyFrom(const UGSGridMaterialSet* FromSet)
{
	DefaultMaterial = FromSet->DefaultMaterial;
	ExternalMaterials = FromSet->ExternalMaterials;
	UpdateReferenceSet();
}


void UGSGridMaterialSet::BuildMaterialMap(FReferenceSetMaterialMap& MapOut) const
{
	MapOut.ReferenceSet = this->ReferenceSet;

	MapOut.AppendMappedMaterial(DefaultMaterial.GetMaterialRefID().PackedValues, DefaultMaterial.Material, &DefaultMaterial.Name);

	for (const FGSGridExternalMaterial& ExternalMaterial : ExternalMaterials)
	{
		MapOut.AppendMappedMaterial(ExternalMaterial.GetMaterialRefID().PackedValues, ExternalMaterial.Material, &ExternalMaterial.Name);
	}
}


void UGSGridMaterialSet::BuildMaterialMapForSet(UGSGridMaterialSet* MaterialSet, FReferenceSetMaterialMap& MapOut)
{
	if (MaterialSet == nullptr)
	{
		// does not initialize ReferenceSet...
		FGSGridExternalMaterial DefaultMaterial = GSInternal::GetBuiltInDefaultMaterial();
		MapOut.AppendMappedMaterial(DefaultMaterial.GetMaterialRefID().PackedValues, DefaultMaterial.Material, &DefaultMaterial.Name);
	}
	else
	{
		MaterialSet->BuildMaterialMap(MapOut);
	}
}



void UGSGridMaterialSet::UpdateReferenceSet()
{
	ReferenceSet.Reset();

	// first pass, re-add all known materials
	for (const FGSGridExternalMaterial& ExternalMaterial : ExternalMaterials)
	{
		if (ExternalMaterial.MaterialID.Index >= 0)
		{
			uint32 NewID = ReferenceSet.RegisterExternalMaterial((uint64_t)ExternalMaterial.Material, ExternalMaterial.MaterialID.Index );
			GS::MaterialReferenceID RefID(NewID);
			check(ExternalMaterial.MaterialID.Index == RefID.Index);
		}
	}
	// second pass, allocate any new materials
	for (FGSGridExternalMaterial& ExternalMaterial : ExternalMaterials)
	{
		if (ExternalMaterial.MaterialID.Index < 0)
		{
			uint32 NewID = ReferenceSet.RegisterExternalMaterial((uint64_t)ExternalMaterial.Material, -1);
			GS::MaterialReferenceID RefID(NewID);
			// update ExternalMaterial with new allocated info
			ExternalMaterial.MaterialID.ReferenceType = (int32)RefID.MaterialType;
			ExternalMaterial.MaterialID.Index = (int32)RefID.Index;
		}
	}
}


void UGSGridMaterialSet::PostLoad()
{
	Super::PostLoad();
	UpdateReferenceSet();
}

#if WITH_EDITOR
void UGSGridMaterialSet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateReferenceSet();
}
void UGSGridMaterialSet::PostEditUndo()
{
	Super::PostEditUndo();
	UpdateReferenceSet();
}
#endif
