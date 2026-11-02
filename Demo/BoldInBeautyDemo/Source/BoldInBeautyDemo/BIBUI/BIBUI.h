// Code-built Slate UI: the fallback gameplay HUD and hub menus used for any screen without a UMG widget class.
#pragma once

#include "CoreMinimal.h"

class ABIBPlayerController;
class SBox;
class SWidget;
enum class EBIBScreen : uint8;

namespace BIBUI
{
	// bIncludeHud = false when a UMG HUD widget class is assigned on the controller.
	TSharedRef<SWidget> BuildRoot(ABIBPlayerController* PC, TSharedPtr<SBox>& OutMenuHost, bool bIncludeHud);
	TSharedRef<SWidget> BuildScreen(ABIBPlayerController* PC, EBIBScreen Screen);
}
