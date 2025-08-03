// Copyright Gradientspace Corp. All Rights Reserved.
#include "Utility/GSMaterialGraphUtil.h"

#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"


#include "Engine/Texture.h"
#include "Engine/Texture2D.h"

UMaterialGraph* GS::GetMaterialGraphForMaterial(UMaterialInterface* MaterialInterface)
{
	if (!MaterialInterface) 
		return nullptr;
	UMaterial* ParentMaterial = MaterialInterface->GetMaterial();
	if (!ParentMaterial)
		return nullptr;
	UMaterialGraph* MaterialGraph = ParentMaterial->MaterialGraph;
	if (MaterialGraph)
		return MaterialGraph;

	// create a material graph from the material if necessary
	// (is outering it to the ParentMaterial the right thing to do??)
	UObject* NewGraphObj = NewObject<UEdGraph>(ParentMaterial, UMaterialGraph::StaticClass(), NAME_None, RF_Transient);
	UMaterialGraph* NewMatGraph = Cast<UMaterialGraph>(NewGraphObj);
	if (!NewMatGraph)
		return nullptr;

	NewMatGraph->Material = ParentMaterial;
	NewMatGraph->RebuildGraph();

	return NewMatGraph;
}


bool GS::FindTextureNodeInMaterial(
	UMaterialGraph* Graph,
	const UTexture2D* FindTexture,
	UMaterialGraphNode*& FoundNode,
	UMaterialExpressionTextureBase*& FoundExpression)
{
	FoundNode = nullptr;
	FoundExpression = nullptr;
	if (!ensure(Graph != nullptr))
		return false;

	for (TObjectPtr<UEdGraphNode> Node : Graph->Nodes)
	{
		UMaterialGraphNode* MatGraphNode = Cast<UMaterialGraphNode>(Node);
		if (!MatGraphNode)
			continue;

		UMaterialExpressionTextureBase* TexExpression = Cast<UMaterialExpressionTextureBase>(MatGraphNode->MaterialExpression);
		if (!TexExpression)
			continue;

		if (TexExpression->Texture == FindTexture)
		{
			FoundNode = MatGraphNode;
			FoundExpression = TexExpression;
			return true;
		}
	}

	return false;
}



void GS::FindActiveTextureSetForMaterial(UMaterialInterface* Material, TArray<MaterialTextureInfo>& TexturesOut,
	bool bIncludeBaseMaterialsForMIs)
{
	// if this is a base UMaterial, we don't have to worry about overrides and can just return the referenced textures
	if (UMaterial* BaseMaterial = Cast<UMaterial>(Material))
	{
		TArrayView<const TObjectPtr<UObject>> ReferencedTextures = Material->GetReferencedTextures();
		for (const TObjectPtr<UObject> TexObject : ReferencedTextures)
		{
			if (const UTexture2D* Tex2D = Cast<UTexture2D>(TexObject))
			{
				TexturesOut.Add(MaterialTextureInfo{ Tex2D, false });
			}
		}
		return;
	}

	// this will return base materials and overrides...but no way to know if both overridden and still used elsewhere...
	//TSet<const UTexture*> AllTextures;
	//Material->GetReferencedTexturesAndOverrides(AllTextures);

	UMaterialGraph* Graph = GS::GetMaterialGraphForMaterial(Material);
	for (TObjectPtr<UEdGraphNode> Node : Graph->Nodes)
	{
		UMaterialGraphNode* MatGraphNode = Cast<UMaterialGraphNode>(Node);
		if (!MatGraphNode)
			continue;
		UMaterialExpressionTextureBase* TexExpression = Cast<UMaterialExpressionTextureBase>(MatGraphNode->MaterialExpression);
		if (!TexExpression)
			continue;

		if (UMaterialExpressionTextureObjectParameter* ObjectParameter = Cast<UMaterialExpressionTextureObjectParameter>(TexExpression))
		{
			UTexture* FoundTex;
			if (Material->GetTextureParameterValue(ObjectParameter->ParameterName, FoundTex, false)) {
				if (const UTexture2D* Tex2D = Cast<UTexture2D>(FoundTex)) {
					TexturesOut.Add(MaterialTextureInfo{ Tex2D, true, ObjectParameter->ParameterName});
				}
			}
		}
		else if (UMaterialExpressionTextureSampleParameter2D* Sample2DParameter = Cast<UMaterialExpressionTextureSampleParameter2D>(TexExpression))
		{
			UTexture* FoundTex;
			if (Material->GetTextureParameterValue(Sample2DParameter->ParameterName, FoundTex, false)) {
				if (const UTexture2D* Tex2D = Cast<UTexture2D>(FoundTex)) {
					TexturesOut.Add(MaterialTextureInfo{ Tex2D, true, Sample2DParameter->ParameterName });
				}
			}
		}
		else
		{
			if (bIncludeBaseMaterialsForMIs) {
				if (const UTexture2D* Tex2D = Cast<UTexture2D>(TexExpression->Texture)) {
					TexturesOut.Add(MaterialTextureInfo{ Tex2D, false });
				}
			}
		}

	}

}