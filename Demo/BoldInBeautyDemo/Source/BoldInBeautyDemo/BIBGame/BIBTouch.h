// Mobile controls (owner request 2026-10-09: "make a mobile version that feels nice on mobile to play use a joystick for movements
// like most titles genshin zzz etc"). A full-screen touch layer under the HUD:
//   * left 45% of the screen: a floating joystick (appears where the thumb lands); pushed to the rim it runs;
//   * the rest of the screen: drag to turn the camera;
//   * bottom right: the Zenless-style button arc - a big attack button, dodge, parry, jump, block, Move 1-3, Special, Ultimate
//     and Switch - each showing its key's cooldown from the HUD; in the Ark only jump and interact.
// On iOS / Android it is always on; on a desktop it is turned on with -BIBTouch (mouse = one finger) for testing.
#pragma once

#include "CoreMinimal.h"

class ABIBPlayerController;
class SWidget;

namespace BIBTouch
{
	/** True on phones / tablets, or with -BIBTouch on the command line. */
	BOLDINBEAUTYDEMO_API bool Enabled();
	TSharedRef<SWidget> Build(ABIBPlayerController* PC);
}
