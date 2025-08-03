// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "UDynamicMesh.h"
#include "GridActor/UGSModelGrid.h"
#include "ModelGridMeshingFunctions.generated.h"


UENUM(BlueprintType)
enum class EModelGridHiddenRemovalStrategy : uint8
{
	/** Do not remove any hidden faces */
	NoHiddenRemoval = 0,
	/** Remove faces that can be trivially detected as hidden based on the grid connectivity */
	FastBoxFaceRemoval = 1,
	/** Remove any cell faces that are geometrically detected to be coincident+identical-but-opposing, ie are guaranteed to obscure eachother (relatively fast). */
	RemoveIdenticalFacePairs = 2,
	/** Remove any hidden geometry at all, by doing a geometric self-union operation (more expensive) */
	RemoveAllHiddenAreas = 3
};


USTRUCT(BlueprintType)
struct GRADIENTSPACESCRIPT_API FModelGridMeshingOptionsV1
{
	GENERATED_BODY()

	/** Method of hidden-face removal to employ during meshing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gradientspace|ModelGrid")
	EModelGridHiddenRemovalStrategy HiddenRemoval = EModelGridHiddenRemovalStrategy::RemoveAllHiddenAreas;

	/** Retriangulate patches of coplanar triangles, ie changes mesh but not appearance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gradientspace|ModelGrid")
	bool bSimplifyMesh = false;

	/** Preserve borders of regions of different color when doing bSimplifyMesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gradientspace|ModelGrid")
	bool bPreserveColorBorders = true;

	/** Preserve borders of regions of different Material when doing bSimplifyMesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gradientspace|ModelGrid")
	bool bPreserveMaterialBorders = true;
};


UCLASS(meta = (ScriptName = "GSS_ModelGridMeshing"), MinimalAPI)
class UGSScriptLibrary_ModelGridMeshing : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Generate a mesh for the given ModelGrid, by appending per-cell meshes and 
	 * then optionally doing hidden-face removal and retriangulation
	 */
	UFUNCTION(BlueprintCallable, Category = "Gradientspace|ModelGrid", meta = (ScriptMethod, DisplayName = "GenerateMeshForModelGrid", Keywords="gss, grid"))
	static GRADIENTSPACESCRIPT_API UPARAM(DisplayName = "Target Grid") UGSModelGrid*
	GenerateMeshForModelGridV1(
		UGSModelGrid* TargetGrid,
		UDynamicMesh* TargetMesh,
		const FModelGridMeshingOptionsV1& Options );

};