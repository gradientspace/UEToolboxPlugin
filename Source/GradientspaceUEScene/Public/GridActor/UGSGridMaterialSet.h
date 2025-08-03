// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "ModelGrid/MaterialReferenceSet.h"

#include "UGSGridMaterialSet.generated.h"


class UMaterialInterface;


/**
 * GridMaterialID is basically the UStruct/BP version of GS::MaterialReferenceID.
 * This is slightly complicated by the fact that uint32 cannot be exposed in BP...
 */
USTRUCT(BlueprintType)
struct GRADIENTSPACEUESCENE_API FGSGridMaterialID
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Material")
	int ReferenceType = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Material")
	int Index = -1;

	bool operator==(const FGSGridMaterialID& OtherID) const
	{
		return ReferenceType == OtherID.ReferenceType && Index == OtherID.Index;
	}
};


/**
 * Reference to a UE MaterialInterface and the ID of that material in a MaterialSet
 */
USTRUCT(BlueprintType)
struct GRADIENTSPACEUESCENE_API FGSGridExternalMaterial
{
	GENERATED_BODY()

	/** Reference to UE Material */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material")
	TObjectPtr<UMaterialInterface> Material;

	/** Friendly Name for this Material (used for UI/etc) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FString Name;

	/** UE representation of GS::MaterialReferenceID for this Material */
	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Material")
	UPROPERTY(BlueprintReadOnly, Category = "Material")
	FGSGridMaterialID MaterialID;

	//! convert back to ModelGrid-level GS::MaterialReferenceID
	GS::MaterialReferenceID GetMaterialRefID() const
	{
		return GS::MaterialReferenceID((GS::EMaterialReferenceType)MaterialID.ReferenceType, (uint32_t)MaterialID.Index);
	}
};



/**
 * ICellMaterialToIndexMap maps ModelGrid cell material IDs (GS::MaterialReferenceID) to 
 * MaterialIDs IDs in a linear material array (eg usable in UE rendering). FReferenceSetMaterialMap is
 * an implementation of the interface designed for UGSGridMaterialSet (ie UE-level MaterialSet asset).
 * 
 * This can be passed to (eg) meshing algorithms that need to output a mesh w/ specific MaterialID indices.
 */
class GRADIENTSPACEUESCENE_API FReferenceSetMaterialMap : public GS::ICellMaterialToIndexMap
{
public:
	GS::MaterialReferenceSet ReferenceSet;
	TMap<uint32_t, int32> InternalIDToMaterialIDMap;

	// these arrays are 1-1 and probably should just be a struct?
	TArray<uint32_t> InternalIDList;
	TArray<UMaterialInterface*> MaterialList;
	TArray<FString> NameList;

public:
	void AppendMappedMaterial(uint32_t UseInternalID, UMaterialInterface* Material, const FString* UseName = nullptr);
	virtual int GetMaterialID(GS::EGridCellMaterialType MaterialType, GS::GridMaterial Material) override;
};


/**
 * GridMaterialSet is the serializable material set associated with an Asset or Component
 */
UCLASS(BlueprintType, MinimalAPI)
class UGSGridMaterialSet : public UObject
{
	GENERATED_BODY()
public:
	UGSGridMaterialSet();

	GRADIENTSPACEUESCENE_API
	void SetDefaultMaterial(UMaterialInterface* Material, FString NewName = TEXT(""));

	GRADIENTSPACEUESCENE_API
	FGSGridMaterialID FindOrAddExternalMaterial(UMaterialInterface* Material);

	GRADIENTSPACEUESCENE_API
	UMaterialInterface* FindExternalMaterial(FGSGridMaterialID GridMaterialID);

	GRADIENTSPACEUESCENE_API
	void BuildMaterialMap(FReferenceSetMaterialMap& MapOut) const;

	GRADIENTSPACEUESCENE_API
	void CopyFrom(const UGSGridMaterialSet* FromSet);

	// helper function to handle cases where MaterialSet is null
	GRADIENTSPACEUESCENE_API
	static void BuildMaterialMapForSet(UGSGridMaterialSet* MaterialSetOrNull, FReferenceSetMaterialMap& MapOut);

protected:
	/** Material used by default for grid cells with simple cell/face colors, often a VertexColor Material */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GridMaterialSet")
	FGSGridExternalMaterial DefaultMaterial;

	/** Additional Materials assigned to grid cells with custom Material */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GridMaterialSet")
	TArray<FGSGridExternalMaterial> ExternalMaterials;

	// can override this to change default material for a subclass type
	virtual void InitializeDefaultMaterial(FGSGridExternalMaterial& GridMaterial);

protected:
	GS::MaterialReferenceSet ReferenceSet;

	void UpdateReferenceSet();

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
};


