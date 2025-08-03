// Copyright Gradientspace Corp. All Rights Reserved.
#include "Tools/GradientspaceFeedbackTool.h"

#include "InteractiveToolManager.h"

#include "HAL/Platform.h"
#include "HAL/PlatformProcess.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/EngineBaseTypes.h"

#ifdef WITH_GS_PLUGIN_MANAGER
#include "GradientspacePluginSubsystem.h"
#endif

#define LOCTEXT_NAMESPACE "GradientspaceFeedbackTool"

UInteractiveTool* UGradientspaceFeedbackToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UGradientspaceFeedbackTool>(SceneState.ToolManager);
}

UGradientspaceFeedbackTool::UGradientspaceFeedbackTool()
{
	UInteractiveTool::SetToolDisplayName(LOCTEXT("ToolName", "Send Feedback"));
}

void UGradientspaceFeedbackTool::Setup()
{
	Settings = NewObject<UGradientspaceFeedbackToolSettings>(this);
	AddToolPropertySource(Settings);

	ActionButtonsTarget = MakeShared<FGSActionButtonTarget>();
	ActionButtonsTarget->ExecuteCommand = [this](FString CommandName) { OnActionButtonClicked(CommandName); };
	ActionButtonsTarget->IsCommandEnabled = [this](FString CommandName) { return IsActionButtonEnabled(CommandName); };
	ActionButtonsTarget->IsCommandVisible = [this](FString CommandName) { return IsActionButtonVisible(CommandName); };
	Settings->FeedbackButtons.Target = ActionButtonsTarget;

	Settings->FeedbackButtons.AddAction("OpenFeedbackEmail",
		LOCTEXT("OpenFeedbackEmail", "Send via Email"),
		LOCTEXT("OpenFeedbackEmailTooltip", "send gradientspace this feedback via Email"));

	Settings->FeedbackButtons.AddAction("OpenFeedbackWebsite",
		LOCTEXT("OpenFeedbackWebsite", "Send via Web"),
		LOCTEXT("OpenFeedbackWebsiteTooltip", "send gradientspace this feedback via your Browser"));

	Settings->LinkButtons.Target = ActionButtonsTarget;
	Settings->LinkButtons.AddAction("OpenPluginWebsite",
		LOCTEXT("OpenPluginWebsite", "Plugin Website"),
		LOCTEXT("OpenPluginWebsiteTooltip", "Open the Gradientspace UE Toolbox Plugin Website"));
	Settings->LinkButtons.AddAction("OpenDiscord",
		LOCTEXT("OpenDiscord", "Discord"),
		LOCTEXT("OpenDiscordTooltip", "Open the Gradientspace Discord server"));
	Settings->LinkButtons.AddAction("OpenYouTube",
		LOCTEXT("OpenYoutube", "YouTube"),
		LOCTEXT("OpenYoutubeTooltip", "Open the Gradientspace YouTube Channel"));

#ifdef WITH_GS_PLUGIN_MANAGER
	FGSPluginVersionNumber CurrentVersion;
	if (UGradientspacePluginSubsystem::GetCurrentVersion(TEXT("GradientspaceUEToolbox"), CurrentVersion))
	{
		Settings->InstalledVersion = FString::Printf(TEXT("%d.%d.%d"), CurrentVersion.MajorVersion, CurrentVersion.MinorVersion, CurrentVersion.PatchVersion);
	}
	else
	{
		Settings->InstalledVersion = TEXT("(unknown!)");
	}

	Settings->UpdateButtons.AddAction("CheckForUpdates",
		LOCTEXT("CheckForUpdates", "Check for Updates"),
		LOCTEXT("CheckForUpdatesTooltip", "Check if a new version of the Gradientspace UE Toolbox Plugin is available"));
	Settings->UpdateButtons.AddAction("DownloadUpdate",
		LOCTEXT("DownloadUpdate", "Download Update"),
		LOCTEXT("DownloadUpdateTooltip", "Launch a web browser to download the updated version"));
#endif
	Settings->UpdateButtons.Target = ActionButtonsTarget;
}



static FString LocalEncodeURI(const FString& InString)
{
	static const TCHAR* EncodingMap[][2] =
	{
		{ TEXT("%"), TEXT("%25") },		// replace "%" first to avoid double-encoding characters
		{ TEXT(":"), TEXT("%3A") },
		{ TEXT("/"), TEXT("%2F") },
		{ TEXT("?"), TEXT("%3F") },
		{ TEXT("#"), TEXT("%23") },
		{ TEXT("["), TEXT("%5B") },
		{ TEXT("]"), TEXT("%5D") },
		{ TEXT("@"), TEXT("%40") },
		{ TEXT("!"), TEXT("%21") },
		{ TEXT("$"), TEXT("%24") },
		{ TEXT("&"), TEXT("%26") },
		{ TEXT("'"), TEXT("%27") },
		{ TEXT("("), TEXT("%28") },
		{ TEXT(")"), TEXT("%29") },
		{ TEXT("*"), TEXT("%2A") },
		{ TEXT("+"), TEXT("%2B") },
		{ TEXT(","), TEXT("%2C") },
		{ TEXT(";"), TEXT("%3B") },
		{ TEXT("="), TEXT("%3D") },
		{ TEXT(" "), TEXT("%20") }
	};

	FString EncodedString = InString;

	int N = GetNum(EncodingMap);
	for (int i = 0; i < N; ++i) {
		const TCHAR* Orig = EncodingMap[i][0];
		const TCHAR* Encoded = EncodingMap[i][1];
		EncodedString.ReplaceInline(Orig, Encoded, ESearchCase::CaseSensitive);
	}

	return EncodedString;
}


bool UGradientspaceFeedbackTool::IsActionButtonEnabled(FString CommandName)
{
	return true;
}
bool UGradientspaceFeedbackTool::IsActionButtonVisible(FString CommandName)
{
	if (CommandName == TEXT("DownloadUpdate"))
	{
		return Settings->bNewerVersionAvaiable;
	}
	return true;
}


void UGradientspaceFeedbackTool::OnActionButtonClicked(FString CommandName)
{
	if (CommandName == TEXT("OpenFeedbackEmail"))
	{
		FString URLString = TEXT("mailto:rms@gradientspace.com?&subject=feedback&body=");
		FString FeedbackText = Settings->FeedbackText;
		FeedbackText = LocalEncodeURI(FeedbackText);
		FURL::FilterURLString(FeedbackText);
		// for some reason the above function can't do this replacement for newlines...
		FeedbackText = FeedbackText.Replace(TEXT("\r\n"), TEXT("%0A"));
		URLString += FeedbackText;
		FPlatformProcess::LaunchURL(*URLString, nullptr, nullptr);
	}
	else if (CommandName == TEXT("OpenFeedbackWebsite"))
	{
		FPlatformProcess::LaunchURL(TEXT("https://www.gradientspace.com/contact"), nullptr, nullptr);
	}
	else if (CommandName == TEXT("OpenPluginWebsite"))
	{
		FPlatformProcess::LaunchURL(TEXT("https://www.gradientspace.com/uetoolbox"), nullptr, nullptr);
	}
	else if (CommandName == TEXT("OpenDiscord"))
	{
		FPlatformProcess::LaunchURL(TEXT("https://discord.gg/2Dnjr5afSz"), nullptr, nullptr);
	}
	else if (CommandName == TEXT("OpenYouTube"))
	{
		FPlatformProcess::LaunchURL(TEXT("https://www.youtube.com/@gradientspace"), nullptr, nullptr);
	}
#ifdef WITH_GS_PLUGIN_MANAGER
	else if (CommandName == TEXT("CheckForUpdates"))
	{
		Settings->AvailableVerson = FString::Printf(TEXT("..."));
		Settings->bUpdateCheckHasRun = true;
		UGradientspacePluginSubsystem::ForceCheckForUpdates([this]()
		{
			FGSPluginVersionNumber NewVersion = UGradientspacePluginSubsystem::GetAvailableVersion(TEXT("GradientspaceUEToolbox"), Settings->bNewerVersionAvaiable);
			Settings->AvailableVerson = FString::Printf(TEXT("%d.%d.%d"), NewVersion.MajorVersion, NewVersion.MinorVersion, NewVersion.PatchVersion);
			Settings->bUpdateCheckHasRun = true;
		});
	}
	else if (CommandName == TEXT("DownloadUpdate"))
	{
		FGSPluginPlatformVersionInfo PlatformInfo; FGSPluginVersionInfo VersionInfo;
		if (UGradientspacePluginSubsystem::GetUpdateInfo(TEXT("GradientspaceUEToolbox"), VersionInfo, PlatformInfo))
		{
			FPlatformProcess::LaunchURL(*PlatformInfo.DownloadURL, nullptr, nullptr);
		}
	}
#endif
}

#undef LOCTEXT_NAMESPACE
