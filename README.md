# UEToolbox

This repo contains an open-source distribution of the Gradientspace UE Toolbox plugin for Unreal Ehgine 5. The website for this plugin can be found at https://www.gradientspace.com/uetoolbox. 

# FAB Version

If you just want to use this plugin in Unreal Engine, you might prefer to use the version distributed on Epic's FAB marketplace. 
[Here is a link to that version](https://www.fab.com/listings/0588e89f-d6fe-482d-882c-bd0e62d8e1d6). 

# Host UE Project repository

This repository is *just* the plugin source, which is not directly buildable - it must be built as part of a UE project or UE5 engine source distribution.
It's set up this way so that it's easy to include this repo as a git submodule in other projects, or to just grab the source zip and drop it into your project.

We have another github repo https://github.com/gradientspace/UEToolboxPlugin_Dev which includes a UE host project set up for development of the plugin, as well as this repo as a submodule, and build scripts for FAB distribution.
If you want to try out the plugin, or do some work on it, start there.


# Source Information

The plugin is (currently) comprised of 11 modules. 3 of those modules - **GradientspaceCore**, **GradientspaceIO**, and **GradientspaceGrid** - are standalone open-source MIT-licensed C++ libraries that are also buildable as UE modules. You can find the code for those modules on github at:

* [GradientspaceCore](https://github.com/gradientspace/GradientspaceCore)
* [GradientspaceIO](https://github.com/gradientspace/GradientspaceIO)
* [GradientspaceGrid](https://github.com/gradientspace/GradientspaceGrid)

Note that these modules are included in this repo as git submodules. If your git client does not automatically sync the submodules, try running `git submodule update --init --recursive`, or use the *GIT_fetch_all_submodules.bat* script in the root directory.

The other 8 modules - **GradientspaceUECore**, **GradientspaceUECoreEditor**, **GradientspaceUEScene**, **GradientspaceUESceneEditor**, **GradientspaceUEToolCore**, **GradientspaceUEToolbox**, **GradientspaceScript**, and **GradientspaceUEWidgets** are regular UE plugin modules.


# License Information

The Gradientspace UE Toolbox plugin code can be acquired through several different channels, and those channels have different licenses.

## Github Open-Source Version License

The license on this repository is [MPL 2.0](https://www.mozilla.org/en-US/MPL/2.0/). The basic premise with this license is that if you get the source from github, modify it in some way, and redistribute it, your modified versions of our code also need to be MPL2.0-licensed and distributed. This applies at the file level, so you are free to incorporate some tools or features of this plugin into yours as long as you publish your changes to our files under MPL2.0. 

The intention here is to not allow someone else to just download and re-sell this plugin as their own. If you feel like you have a use case that (1) isn't that but (2) doesn't work with MPL2.0, contact rms@gradientspace.com and we'll likely be happy to help.

## FAB Standard License

If you get the code from FAB, you have access to it under the FAB Standard License. This License allows you to do basically whatever you want with the code within your game project or studio. With this license you can make any changes, including using the code however you like, in the context of your game/studio, and there are no restrictions on redistributing it in compiled form (ie on your shipped game). However you cannot *redistribute* any of the plugin source code you get from FAB (whether or not you modify it).

## Custom License

If neither of these options work for you, contact rms@gradientspace.com to discuss other options.
