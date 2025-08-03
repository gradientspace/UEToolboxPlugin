// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "Mesh/GenericMeshAPI.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMeshEditor.h"


namespace GS
{
using namespace UE::Geometry;

//
// implementations of IMeshBuilder, IMeshBuilderFactory, and IMeshCollector backed
// by FDynamicMesh3. Currently this is a bit tailored to ModelGrid/WorldGrid usage,
// but it could be generalized
//

class GRADIENTSPACEUECORE_API FDynamicMesh3Builder : public IMeshBuilder
{
public:
	FDynamicMesh3* Mesh = nullptr;
	FDynamicMeshColorOverlay* Colors = nullptr;
	FDynamicMeshNormalOverlay* Normals = nullptr;
	FDynamicMeshUVOverlay* UVs = nullptr;
	FDynamicMeshMaterialAttribute* Materials = nullptr;
	
	bool bDeleteMeshOnDestruct = false;

	FDynamicMesh3Builder(FDynamicMesh3* mesh, bool bIncludeUVs, bool bIncludeMaterials, bool bDeleteMeshOnDestructIn)
	{
		Mesh = mesh;
		bDeleteMeshOnDestruct = bDeleteMeshOnDestructIn;

		// initialize for our defaults   (maybe this should not happen on construction, force call to ResetMesh()?)
		Mesh->Clear();
		Mesh->EnableTriangleGroups();
		Mesh->EnableAttributes();
		Normals = Mesh->Attributes()->PrimaryNormals();

		Mesh->Attributes()->EnablePrimaryColors();
		Colors = Mesh->Attributes()->PrimaryColors();

		Materials = nullptr;
		if (bIncludeMaterials) {
			Mesh->Attributes()->EnableMaterialID();
			Materials = Mesh->Attributes()->GetMaterialID();
		}

		// todo clear UV layer here if bIncludeUVs == false?
		UVs = (bIncludeUVs) ? Mesh->Attributes()->GetUVLayer(0) : nullptr;
	}

	virtual ~FDynamicMesh3Builder()
	{
		if (bDeleteMeshOnDestruct && Mesh != nullptr) {
			delete Mesh;
			Mesh = nullptr;
		}
	}

	void ResetMesh() override
	{
		// argh clear deletes attribute set!!
		Mesh->Clear();
		Mesh->EnableTriangleGroups();
		Mesh->EnableAttributes();
		Normals = Mesh->Attributes()->PrimaryNormals();

		Mesh->Attributes()->EnablePrimaryColors();
		Colors = Mesh->Attributes()->PrimaryColors();

		if (Materials != nullptr) {
			Mesh->Attributes()->EnableMaterialID();
			Materials = Mesh->Attributes()->GetMaterialID();
		}

		if (UVs != nullptr) {
			UVs = Mesh->Attributes()->GetUVLayer(0);
		}
	}

	virtual int AppendVertex(const Vector3d& Position) override {
		return Mesh->AppendVertex(Position);
	}
	virtual int GetVertexCount() const override {
		return Mesh->VertexCount();
	}
	virtual int AllocateGroupID() override {
		return Mesh->AllocateTriangleGroup();
	}
	virtual int AppendTriangle(const Index3i& Triangle, int GroupID) override {
		return Mesh->AppendTriangle((FIndex3i)Triangle, GroupID);
	}
	virtual int GetTriangleCount() const override {
		return Mesh->TriangleCount();
	}
	virtual void SetMaterialID(int TriangleID, int MaterialID) override {
		if (Materials) Materials->SetValue(TriangleID, MaterialID);
	}
	virtual int AppendColor(const Vector4f& Color, bool bIsLinearColor) override {
		ensure(bIsLinearColor == true);
		return (Colors) ? Colors->AppendElement(Color) : -1;
	}
	virtual void SetTriangleColors(int TriangleID, const Index3i& TriColorIndices) override {
		if (Colors) Colors->SetTriangle(TriangleID, TriColorIndices);
	}

	virtual int AppendNormal(const Vector3f& Normal) override {
		return (Normals) ? Normals->AppendElement((FVector3f)Normal) : -1;
	}
	virtual void SetTriangleNormals(int TriangleID, const Index3i& TriNormalIndices) override {
		if (Normals) Normals->SetTriangle(TriangleID, TriNormalIndices);
	}

	virtual int AppendUV(const Vector2f& UV) override {
		return (UVs) ? UVs->AppendElement((FVector2f)UV) : -1;
	}
	virtual void SetTriangleUVs(int TriangleID, const Index3i& TriUVIndices) override {
		if (UVs) UVs->SetTriangle(TriangleID, TriUVIndices);
	}

};

class GRADIENTSPACEUECORE_API FDynamicMesh3BuilderFactory : public IMeshBuilderFactory
{
public:
	bool bEnableUVs = false;
	bool bEnableMaterials = false;

	virtual IMeshBuilder* Allocate()
	{
		FDynamicMesh3* Mesh = new FDynamicMesh3();
		return new FDynamicMesh3Builder(Mesh, bEnableUVs, bEnableMaterials, true);
	}
};


class GRADIENTSPACEUECORE_API FDynamicMesh3Collector : public IMeshCollector
{
public:
	FDynamicMesh3* TargetMesh = nullptr;
	FDynamicMeshEditor Editor;
	FMeshIndexMappings Tmp;

	bool bOwnsMesh = false;

	FDynamicMesh3Collector(FDynamicMesh3* TargetMeshIn, bool bEnableMaterials, bool bEnableUVs) : Editor(TargetMeshIn)
	{
		TargetMesh = TargetMeshIn;
		TargetMesh->Clear();
		TargetMesh->EnableTriangleGroups();
		if (TargetMesh->HasAttributes() == false) 
			TargetMesh->EnableAttributes();
		if (TargetMesh->Attributes()->HasPrimaryColors() == false) 
			TargetMesh->Attributes()->EnablePrimaryColors();
		if (bEnableMaterials && TargetMesh->Attributes()->HasMaterialID() == false)
			TargetMesh->Attributes()->EnableMaterialID();
		if (bEnableUVs && TargetMesh->Attributes()->NumUVLayers() == 0)
			TargetMesh->Attributes()->SetNumUVLayers(1);
	}
	~FDynamicMesh3Collector() {
		if (bOwnsMesh && TargetMesh != nullptr) {
			delete TargetMesh;
			TargetMesh = nullptr;
		}
	}

	FDynamicMesh3* TakeMesh() {
		bOwnsMesh = false;
		Editor.Mesh = nullptr;
		return TargetMesh;
	}

	virtual void AppendMesh(const IMeshBuilder* Builder)
	{
		const FDynamicMesh3Builder* DynamicMeshBuilder = static_cast<const FDynamicMesh3Builder*>(Builder);
		Tmp.Reset();
		Editor.AppendMesh(DynamicMeshBuilder->Mesh, Tmp);
	}

	virtual AxisBox3d GetBounds() const
	{
		return (AxisBox3d)TargetMesh->GetBounds(true);
	}
};


}
