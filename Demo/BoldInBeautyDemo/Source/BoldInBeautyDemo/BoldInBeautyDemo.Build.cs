using UnrealBuildTool;

public class BoldInBeautyDemo : ModuleRules
{
	public BoldInBeautyDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets files include "BIBRules/...", "BIBGame/..." and "BIBUI/..." from anywhere in the module.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"HTTP",
			"RenderCore",
			"RHI",
			"ProceduralMeshComponent",
			"Json",
			"MediaAssets",
			"MoviePlayer",
			"AudioMixer",
			"MediaUtils"
		});

		// Editor builds only: the module asks the editor not to drop Play in Editor to 3 fps when it loses focus
		// (BoldInBeautyDemo.cpp, owner report 2026-10-08).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
