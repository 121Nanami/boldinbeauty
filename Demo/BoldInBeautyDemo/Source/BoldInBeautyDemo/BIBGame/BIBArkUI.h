// The Ark's interface (owner request 2026-10-07): the 2D lobby became the Ark, and its screens became this.
//
//  - Ark HUD: nothing on screen while walking except the three-line menu button in the top right, plus what is relevant right
//    now: the "F  Open <station>" prompt inside a station's ring, the tutorial line while the tutorial runs, an arrival banner
//    after a teleport and a short toast for messages.
//  - Menu (EBIBScreen::ArkMenu): a flow chart. Menu > The Ark > Town Square > Armoury: each click opens the next column, joined
//    by connector lines; leaves either teleport the player to a place (GO) or open a personal page straight away (OPEN).
//  - Panel (EBIBScreen::ArkPanel): a glass panel over the right of the screen with the station's counters (tabs), the page
//    (BIBRules/BIBPages.h, the same pages as before) as cards and buttons, and the 3D Ark still visible on the left.
//  - Battle HUD: during stages only what the fight needs: the stage objective, the active operator (HP, ultimate), the squad,
//    the ability bar with cooldowns, the kit's prompt, short message toasts, dialogue and the operator panels when open.
//    Controls are listed only while F1 is held. -BIBClassicHud brings the old UMG HUD back.
#pragma once

#include "CoreMinimal.h"

class ABIBPlayerController;
class SWidget;

namespace BIBArkUI
{
	/** The always-present layer: Ark HUD and (unless the classic HUD is used) the battle HUD; each shows itself when it applies. */
	TSharedRef<SWidget> BuildLayer(ABIBPlayerController* PC, bool bBattleHud);
	TSharedRef<SWidget> BuildMenu(ABIBPlayerController* PC);
	TSharedRef<SWidget> BuildPanel(ABIBPlayerController* PC);
}
