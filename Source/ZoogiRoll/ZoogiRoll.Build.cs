using UnrealBuildTool;

public class ZoogiRoll : ModuleRules
{
	public ZoogiRoll(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore", "Slate", "SlateCore", "UMG", "Niagara", "NiagaraCore", "MediaAssets", "ProceduralMeshComponent", "AudioMixer", "Json", "OnlineSubsystem", "ImageCore", "ApplicationCore", "WebBrowser", "WebBrowserWidget"
		});
		if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "NiagaraEditor" });
		// ZoogiHaptics vibrator feature probe needs AndroidJNI.h (Launch module) on Android Shipping/Dev cooks.
		// Oct 31 b41: Apple GCVirtualController for Battle Royale (ZoogiApplePad.cpp)
		if (Target.Platform == UnrealTargetPlatform.IOS) PublicFrameworks.AddRange(new string[] { "GameController", "CoreHaptics", "AudioToolbox" }); // b66 CoreHaptics: 5 s clash rumble
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("Launch");
		}
	}
}
