// Copyright Gradientspace Corp. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class GradientspaceScript : ModuleRules
{
	public GradientspaceScript(ReadOnlyTargetRules Target) : base(Target)
	{
        //#UEPLUGINTOOL

        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] { }
			);
		PrivateIncludePaths.AddRange(
			new string[] { }
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "GeometryCore",
                "DynamicMesh",
				"MeshConversion",
                "InteractiveToolsFramework",
				"GeometryFramework",
				"GeometryScriptingCore",

				"GradientspaceCore",
                "GradientspaceIO",
                "GradientspaceGrid",

				"GradientspaceUECore",
				"GradientspaceUEScene"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[] {} 
			);

        // add UnrealEd dependency if building for editor
        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.Add("EditorFramework");
            PrivateDependencyModuleNames.Add("UnrealEd");
            PrivateDependencyModuleNames.Add("Projects");
        }


		bool bUsingPrecompiledGSLibs = Directory.Exists(Path.Combine(PluginDirectory, "source", "GradientspaceBinary"));
		if (bUsingPrecompiledGSLibs)
			PublicDependencyModuleNames.Add("GradientspaceBinary");
	}
}
