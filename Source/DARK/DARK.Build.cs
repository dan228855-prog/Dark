// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DARK : ModuleRules
{
	public DARK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "PhysicsCore", "AnimationCore", "MoviePlayer", "EngineSettings" });

		PublicIncludePaths.AddRange(new string[] {
			"DARK",
			"DARK/Variant_Horror",
			"DARK/Variant_Horror/UI",
			"DARK/Variant_Shooter",
			"DARK/Variant_Shooter/AI",
			"DARK/Variant_Shooter/UI",
			"DARK/Variant_Shooter/Weapons"
		});

		// Масштабирование и генерация кадров: DLSS / Reflex (плагины NVIDIA) — только Windows.
		// FSR управляется консольными переменными, зависимость не нужна.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "DLSSBlueprint", "StreamlineDLSSGBlueprint", "StreamlineReflexBlueprint" });
			PublicDefinitions.Add("DARC_WITH_NVIDIA=1");
		}
		else
		{
			PublicDefinitions.Add("DARC_WITH_NVIDIA=0");
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
