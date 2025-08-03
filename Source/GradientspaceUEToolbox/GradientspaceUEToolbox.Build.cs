// Copyright Gradientspace Corp. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class GradientspaceUEToolbox : ModuleRules
{
	public GradientspaceUEToolbox(ReadOnlyTargetRules Target) : base(Target)
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

		bool bIncludePluginManager = Directory.Exists(
			Path.GetFullPath(Path.Combine(PluginDirectory, "Source", "GradientspaceUEPluginManager")));
		if ( bIncludePluginManager )
			PublicDefinitions.Add("WITH_GS_PLUGIN_MANAGER");

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
                "EditorFramework",
                "UnrealEd",
                "ToolWidgets",
                "EditorWidgets",
				"EditorSubsystem",

                "GeometryCore",
                "DynamicMesh",
				"MeshConversion",
				"GeometryAlgorithms",
                "InteractiveToolsFramework",
				"GeometryFramework",
				"ModelingComponents",
				"MeshModelingTools",

				// todo make these optional when building for editor...
				"ModelingComponentsEditorOnly",
                "ModelingEditorUI",
                "ModelingToolsEditorMode",

                "GradientspaceCore",
                "GradientspaceGrid",
                "GradientspaceIO",

				"GradientspaceUECore",
                "GradientspaceUECoreEditor",
                "GradientspaceUEScene",
                "GradientspaceUEToolCore"

				// ... add other public dependencies that you statically link with here ...
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
                "AssetDefinition",
                "AssetTools",
                "ContentBrowser",

                "PropertyEditor",		// for details customizations
				"LevelEditor",
				"ToolMenus",

				"EditorInteractiveToolsFramework",
				
				"GradientspaceUECore",
				"GradientspaceUEWidgets",

				"DesktopWidgets",
				"ModelingEditorUI",

                "DesktopPlatform",		// for popup platform-specific directory picker
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



		if ( bIncludePluginManager )
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"GradientspaceUEPluginManager"
				});
		}


		bool bUsingPrecompiledGSLibs = Directory.Exists(Path.Combine(PluginDirectory, "source", "GradientspaceBinary"));
		if (bUsingPrecompiledGSLibs)
			PublicDependencyModuleNames.Add("GradientspaceBinary");
	}
}
