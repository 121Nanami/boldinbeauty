using UnrealBuildTool;
using System.Collections.Generic;

public class BoldInBeautyDemoTarget : TargetRules
{
	public BoldInBeautyDemoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("BoldInBeautyDemo");
	}
}
