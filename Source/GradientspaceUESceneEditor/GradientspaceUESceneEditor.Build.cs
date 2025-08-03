// Copyright Gradientspace Corp. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class GradientspaceUESceneEditor : ModuleRules
{
	public GradientspaceUESceneEditor(ReadOnlyTargetRules Target) : base(Target)
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
                "GeometryCore",
                "DynamicMesh",
				"MeshConversion",
				"GeometryFramework",

				"GradientspaceCore",
                "GradientspaceUECore",
                "GradientspaceGrid",
				"GradientspaceUEScene"

				// ... add other public dependencies that you statically link with here ...
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
                "InputCore",
				"Slate",
				"SlateCore",
				"AssetDefinition",
				"UnrealEd",

				"GeometryScriptingCore",
				"GeometryScriptingEditor"

				// ... add private dependencies that you statically link with here ...	
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
