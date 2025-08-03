// Copyright Gradientspace Corp. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class GradientspaceUEToolCore : ModuleRules
{
	public GradientspaceUEToolCore(ReadOnlyTargetRules Target) : base(Target)
	{
        //#UEPLUGINTOOL

        string PluginDirectory = Path.Combine(ModuleDirectory, "..", "..");
        bool bIsGSDevelopmentBuild = File.Exists(
            Path.GetFullPath(Path.Combine(PluginDirectory, "..", "GRADIENTSPACE_DEV_BUILD.txt")));

        if (bIsGSDevelopmentBuild) {
			PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
			bUseUnity = false;
		} else	{
            PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        }  

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
                "ApplicationCore",
                "Slate",
                "SlateCore",
                "Engine",
                "InputCore",

                "GeometryCore",
                "DynamicMesh",
				"MeshConversion",
				"GeometryAlgorithms",
                "InteractiveToolsFramework",
				"GeometryFramework",
				"ModelingComponents",
				"MeshModelingTools",

                "GradientspaceCore",
                "GradientspaceGrid",
                "GradientspaceIO",

				"GradientspaceUECore",
                "GradientspaceUEScene"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"RHI",
				"RenderCore",
				"ImageCore",

                "Landscape",

                "Projects",
                "DeveloperSettings",

				"GradientspaceUECore",

				"ImageCore",
				"ImageWrapper",

				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);



		// this is for FActorLabelUtilities in ModelGridEditorTool...should get rid of that somehow
        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }


		bool bUsingPrecompiledGSLibs = Directory.Exists(Path.Combine(PluginDirectory, "source", "GradientspaceBinary"));
		if (bUsingPrecompiledGSLibs)
			PublicDependencyModuleNames.Add("GradientspaceBinary");
	}
}
