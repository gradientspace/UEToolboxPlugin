// Copyright Gradientspace Corp. All Rights Reserved.
#include "HotSpotTexturingFunctions.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMeshEditor.h"
#include "UDynamicMesh.h"
#include "Parameterization/DynamicMeshUVEditor.h"

#include "Generators/RectangleMeshGenerator.h"

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGradientspaceScriptLibrary_HotSpotTexturing"



//static void AppendPrimitive(
//	UDynamicMesh* TargetMesh,
//	FMeshShapeGenerator* Generator,
//	FTransform Transform,
//	FGeometryScriptPrimitiveOptions PrimitiveOptions,
//	FVector3d PreTranslate = FVector3d::Zero())
//{
//	if (TargetMesh->IsEmpty())
//	{
//		TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
//		{
//			EditMesh.Copy(Generator);
//			ApplyPrimitiveOptionsToMesh(EditMesh, Transform, PrimitiveOptions, PreTranslate);
//		}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
//	}
//	else
//	{
//		FDynamicMesh3 TempMesh(Generator);
//		ApplyPrimitiveOptionsToMesh(TempMesh, Transform, PrimitiveOptions, PreTranslate);
//		TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
//		{
//			FMeshIndexMappings TmpMappings;
//			FDynamicMeshEditor Editor(&EditMesh);
//			Editor.AppendMesh(&TempMesh, TmpMappings);
//		}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
//	}
//}


UDynamicMesh* UGradientspaceScriptLibrary_HotSpotTexturing::AppendHotSpotRectangleGridV1(
	UDynamicMesh* TargetMesh,
	float PixelDimension,
	int32 Subdivisions,
	float PaddingPixelWidth,
	bool bFlipX,
	bool bFlipY,
	bool bRepeatLast,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr) {
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("HotSpotTexturing_AppendHotSpotRectangleGrid", "AppendHotSpotRectangleGrid: TargetMesh is Null"));
		return TargetMesh;
	}

	FDynamicMesh3 BuildMesh;
	TargetMesh->ProcessMesh([&](const FDynamicMesh3& Mesh) { BuildMesh.EnableMatchingAttributes(Mesh); });

	FDynamicMeshEditor Editor(&BuildMesh);
	FMeshIndexMappings TmpMappings;

	auto SetToNewGroup = [&](FDynamicMesh3& Mesh) {
		int32 NewGroupID = BuildMesh.AllocateTriangleGroup();
		for (int32 tid : Mesh.TriangleIndicesItr())
		{
			Mesh.SetTriangleGroup(tid, NewGroupID);
		}
	};

	auto AppendRectangle = [&](double Width, double Height, double CenterX, double CenterY)
	{
		FRectangleMeshGenerator RectGenerator;
		RectGenerator.Normal = FVector3f::UnitZ();
		RectGenerator.WidthVertexCount = RectGenerator.HeightVertexCount = 0;
		RectGenerator.bSinglePolyGroup = true;
		RectGenerator.Origin = FVector3d::Zero();
		RectGenerator.Width = Width - (double)(2 * PaddingPixelWidth);
		RectGenerator.Height = Height - (double)(2 * PaddingPixelWidth);
		RectGenerator.Generate();
		FDynamicMesh3 NewRect(&RectGenerator);
		MeshTransforms::Translate(NewRect, FVector3d(CenterX, CenterY, 0));
		TmpMappings.Reset();
		SetToNewGroup(NewRect);
		Editor.AppendMesh(&NewRect, TmpMappings);
	};


	double CurHalfDimension = PixelDimension * 0.5;
	FVector3d CurOrigin(0, 0, 0);
	int32 StepCount = 1;
	int32 LayerSubdivisions = Subdivisions;
	do {
		// make square for this dimension
		bool bIsFinalRepeatedCorner = (bRepeatLast && StepCount == (Subdivisions+1) );
		if (bIsFinalRepeatedCorner) {
			CurHalfDimension *= 2;
		}

		AppendRectangle(CurHalfDimension, CurHalfDimension, 
			CurOrigin.X + CurHalfDimension/2, CurOrigin.Y + CurHalfDimension/2);

		double XOrigin = CurOrigin.X + CurHalfDimension, YOrigin = CurOrigin.Y + CurHalfDimension;
		double SubDimension = CurHalfDimension;
		for (int32 k = 1; k < LayerSubdivisions + (bRepeatLast?1:0) && !bIsFinalRepeatedCorner ; ++k)
		{
			bool bIsRepeated = (bRepeatLast && k == LayerSubdivisions);

			if (!bIsRepeated) SubDimension /= 2;

			AppendRectangle(SubDimension, CurHalfDimension, XOrigin+SubDimension/2, CurOrigin.Y+CurHalfDimension/2);
			XOrigin += SubDimension;

			AppendRectangle(CurHalfDimension, SubDimension, CurOrigin.Y + CurHalfDimension/2, YOrigin + SubDimension / 2);
			YOrigin += SubDimension;
		}

		LayerSubdivisions -= 1;
		CurOrigin += FVector3d(CurHalfDimension, CurHalfDimension, 0);
		CurHalfDimension *= 0.5;

	} while (StepCount++ < (Subdivisions + (bRepeatLast?1:0)) );

	MeshTransforms::Translate(BuildMesh, FVector3d(-PixelDimension / 2, -PixelDimension / 2, 0));

	if (bFlipX || bFlipY)
	{
		MeshTransforms::Scale(BuildMesh, FVector3d((bFlipX) ? -1 : 1, (bFlipY) ? -1 : 1, 1), FVector3d::Zero(), true);
	}


	FDynamicMeshUVEditor UVEditor(&BuildMesh, 0, true);
	FDynamicMeshUVOverlay* UVOverlay = UVEditor.GetOverlay();
	TArray<int32> AllTris; for (int32 tid : BuildMesh.TriangleIndicesItr()) { AllTris.Add(tid); }
	UVEditor.SetTriangleUVsFromProjection(AllTris, FFrame3d());
	for (int32 elemID : UVOverlay->ElementIndicesItr())
	{
		FVector2f UV = UVOverlay->GetElement(elemID);
		UV.X -= PixelDimension * 0.5; UV.Y -= PixelDimension * 0.5;
		UV *= 1.0 / PixelDimension;
		UVOverlay->SetElement(elemID, UV);
	}

	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
	{
		FMeshIndexMappings TmpMappings;
		FDynamicMeshEditor Editor(&EditMesh);
		Editor.AppendMesh(&BuildMesh, TmpMappings);
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}




#undef LOCTEXT_NAMESPACE
