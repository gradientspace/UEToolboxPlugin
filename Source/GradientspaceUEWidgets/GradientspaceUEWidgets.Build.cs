// Copyright Gradientspace Corp. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class GradientspaceUEWidgets : ModuleRules
{
	public GradientspaceUEWidgets(ReadOnlyTargetRules Target) : base(Target)
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

				// ... add other public dependencies that you statically link with here ...
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
				"UnrealEd",
				"EditorFramework",
				"Projects",

				"PropertyEditor",		// for details customizations

				// todo make these optional when building for editor...
				"ModelingEditorUI",

				// ... add private dependencies that you statically link with here ...	
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		// not clear we need this...
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"UnrealEd",
				"EditorFramework",
				"Projects",
				"PropertyEditor",		// for details customizations
				"ModelingEditorUI",
			});
		}

	}
}
