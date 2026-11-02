#include "BoldInBeautyDemo.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace
{
	TAutoConsoleVariable<int32> CVarBIBPlayNoBackgroundThrottle(
		TEXT("bib.Editor.FullSpeedPlayInBackground"),
		1,
		TEXT("1 = Play in Editor keeps its real frame rate when the editor loses focus (the editor's \"Use Less CPU when in ")
		TEXT("Background\" otherwise caps it at 3 fps), 0 = the editor preference decides."),
		ECVF_Default);
}

// Owner report 2026-10-08: "it runs at 3 fps". With Editor Preferences > Performance > "Use Less CPU when in Background" on
// (the engine default, and set in this Mac's EditorSettings.ini), UEditorEngine::GetMaxTickRate caps the editor - and so Play
// in Editor - at exactly 3 fps whenever another app has focus. While a play session runs this module asks the editor not to
// throttle; an idle editor still saves CPU in the background.
class FBoldInBeautyDemoModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddLambda([]()
		{
			if (GEditor)
			{
				GEditor->ShouldDisableCPUThrottlingDelegates.Add(UEditorEngine::FShouldDisableCPUThrottling::CreateLambda([]()
				{
					return GEditor && GEditor->PlayWorld != nullptr && CVarBIBPlayNoBackgroundThrottle.GetValueOnGameThread() != 0;
				}));
			}
		});
#endif
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
	}

private:
	FDelegateHandle PostEngineInitHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FBoldInBeautyDemoModule, BoldInBeautyDemo, "BoldInBeautyDemo");

DEFINE_LOG_CATEGORY(LogBIB);
