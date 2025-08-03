// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"
#include "UObject/ObjectMacros.h"

#include "ModelGridScriptTypes.generated.h"



// numbers should remain compatible with GS::EModelGridCellType
UENUM(BlueprintType)
enum class EModelGridBPCellType : uint8
{
	Empty = 0,
	Filled = 1,
	Slab = 4,
	Ramp = 5,
	Corner = 6,
	Pyramid = 7,
	Peak = 8,
	Cylinder = 9,
	CutCorner = 10,

	LastKnownType = 10 UPARAM(Hidden),

	Unknown = 255
};


UENUM(BlueprintType)
enum class EModelGridBPAxisDirection : uint8
{
	PlusZ = 0,
	PlusY = 1,
	PlusX = 2,
	MinusX = 3,
	MinusY = 4,
	MinusZ = 5
};
UENUM(BlueprintType)
enum class EModelGridBPAxisRotation : uint8
{
	Zero = 0 UMETA(DisplayName = "0"),
	Ninety = 1 UMETA(DisplayName = "90"),
	OneEighty = 2 UMETA(DisplayName = "180"),
	TwoSeventy = 3 UMETA(DisplayName = "270")
};
UENUM(BlueprintType)
enum class EModelGridBPDimensionType : uint8
{
	Quarters = 0,
	Thirds = 1
};

USTRUCT(BlueprintType)
struct GRADIENTSPACESCRIPT_API FModelGridCellOrientationV1
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation")
	EModelGridBPAxisDirection AxisDirection = EModelGridBPAxisDirection::PlusZ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation")
	EModelGridBPAxisRotation AxisRotation = EModelGridBPAxisRotation::Zero;
};

USTRUCT(BlueprintType)
struct GRADIENTSPACESCRIPT_API FModelGridCellTransformV1
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation")
	FModelGridCellOrientationV1 Orientation = FModelGridCellOrientationV1();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation")
	EModelGridBPDimensionType DimensionType = EModelGridBPDimensionType::Quarters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation")
	FIntVector Dimensions = FIntVector(15, 15, 15);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation")
	FIntVector Translation = FIntVector(0,0,0);
};



UENUM(BlueprintType)
enum class EModelGridBPCellMaterialType : uint8
{
	RGBAColor = 0,
	RGBColorPlusIndex = 1,

	PerFaceColors = 8
};

USTRUCT(BlueprintType)
struct GRADIENTSPACESCRIPT_API FModelGridBPCellMaterialV1
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CellMaterial")
	EModelGridBPCellMaterialType MaterialType = EModelGridBPCellMaterialType::RGBAColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CellMaterial")
	FColor SolidColor = FColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CellMaterial")
	int Index = 0;
};





USTRUCT(BlueprintType)
struct GRADIENTSPACESCRIPT_API FMGCellFaceNbrsV1
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours")
	FIntVector CellIndex = FIntVector(0, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours")
	EModelGridBPCellType PlusX = EModelGridBPCellType::Empty;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours")
	EModelGridBPCellType MinusX = EModelGridBPCellType::Empty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours")
	EModelGridBPCellType PlusY = EModelGridBPCellType::Empty;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours")
	EModelGridBPCellType MinusY = EModelGridBPCellType::Empty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours")
	EModelGridBPCellType PlusZ = EModelGridBPCellType::Empty;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours")
	EModelGridBPCellType MinusZ = EModelGridBPCellType::Empty;
};



UENUM(BlueprintType)
enum class EMGPlaneAxes : uint8
{
	PlaneYZ = 0,
	PlaneXZ = 1,
	PlaneXY = 2
};

UENUM(BlueprintType)
enum class EMGPlaneDirections : uint8
{
	MinusU = 7 UMETA(DisplayName="(-1,0)"),
	PlusU = 3 UMETA(DisplayName="(+1,0)"),
	MinusV = 1 UMETA(DisplayName="(0,-1)"),
	PlusV = 5 UMETA(DisplayName="(0,+1)"),
	MinusUMinusV = 0 UMETA(DisplayName="(-1,-1)"),
	PlusUMinusV = 2 UMETA(DisplayName="(+1,-1)"),
	PlusUPlusV = 4 UMETA(DisplayName="(+1,+1)"),
	MinusUPlusV = 6 UMETA(DisplayName="(-1,+1)")
};



USTRUCT(BlueprintType)
struct GRADIENTSPACESCRIPT_API FMGCellPlaneNbrsV1
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours")
	FIntVector CellIndex = FIntVector(0, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours")
	EMGPlaneAxes PlaneAxes = EMGPlaneAxes::PlaneXY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours", meta = (DisplayName="(-1,0)"))
	EModelGridBPCellType MinusU = EModelGridBPCellType::Empty;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours", meta = (DisplayName="(+1,0)"))
	EModelGridBPCellType PlusU = EModelGridBPCellType::Empty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours", meta = (DisplayName="(0,-1)"))
	EModelGridBPCellType MinusV = EModelGridBPCellType::Empty;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours", meta = (DisplayName="(0,+1)"))
	EModelGridBPCellType PlusV = EModelGridBPCellType::Empty;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours", meta = (DisplayName="(-1,-1)"))
	EModelGridBPCellType MinusUMinusV = EModelGridBPCellType::Empty;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours", meta = (DisplayName="(+1,-1)"))
	EModelGridBPCellType PlusUMinusV = EModelGridBPCellType::Empty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours", meta = (DisplayName="(+1,+1)"))
	EModelGridBPCellType PlusUPlusV = EModelGridBPCellType::Empty;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceNeighbours", meta = (DisplayName="(-1,+1)"))
	EModelGridBPCellType MinusUPlusV = EModelGridBPCellType::Empty;

};

