// Copyright Gradientspace Corp. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class GradientspaceUECoreEditor : ModuleRules
{
	public GradientspaceUECoreEditor(ReadOnlyTargetRules Target) : base(Target)
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
				"GradientspaceCore",
				"GradientspaceGrid",
				"GeometryCore",
				"DynamicMesh",
				"GeometryAlgorithms",
				"GeometryFramework",

				"GradientspaceUECore",

				"ModelingOperators"		// introduces MeshModelingToolset dependency - get rid of this...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
                "RenderCore",
                "RHI",
                "ImageCore",

                "MeshConversion",

				"UnrealEd"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
