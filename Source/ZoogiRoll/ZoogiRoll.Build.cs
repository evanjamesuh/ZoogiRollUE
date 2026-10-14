using UnrealBuildTool;

public class ZoogiRoll : ModuleRules
{
	public ZoogiRoll(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore", "Slate", "SlateCore", "UMG", "Niagara", "NiagaraCore", "MediaAssets", "ProceduralMeshComponent", "AudioMixer", "Json", "OnlineSubsystem", "ImageCore", "ApplicationCore"
		});
		if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "NiagaraEditor" });
	}
}
