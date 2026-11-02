using UnrealBuildTool;
using System.Collections.Generic;

public class BoldInBeautyDemoEditorTarget : TargetRules
{
	public BoldInBeautyDemoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("BoldInBeautyDemo");
	}
}
