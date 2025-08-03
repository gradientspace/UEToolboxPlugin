// Copyright Gradientspace Corp. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class GradientspaceUEScene : ModuleRules
{
	public GradientspaceUEScene(ReadOnlyTargetRules Target) : base(Target)
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
                "GradientspaceGrid"

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
				"SlateCore"

				// ... add private dependencies that you statically link with here ...	
			}
			);
		


		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);


		// optional dependencies when building for editor (to allow for some WITH_EDITOR code blocks)
		//if (Target.bBuildEditor == true)
		//{
		//    PrivateDependencyModuleNames.Add("UnrealEd");
		//}

		bool bUsingPrecompiledGSLibs = Directory.Exists(Path.Combine(PluginDirectory, "source", "GradientspaceBinary"));
		if (bUsingPrecompiledGSLibs)
			PublicDependencyModuleNames.Add("GradientspaceBinary");
	}
}
