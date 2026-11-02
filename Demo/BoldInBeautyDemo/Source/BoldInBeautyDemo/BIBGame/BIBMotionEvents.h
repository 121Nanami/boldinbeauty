// Where each piece of the bb-Showreel motion pack plays in the game (owner 2026-10-09: "use them all"). Every one of the 54
// pieces has a moment (BIBMotion.h plays them; this file decides when):
//
//  Navigation   the Ark menu opens with transition 03 + the nav bar; a station opens with transition 02 + the page header
//               (the station's name slammed in); a personal page with transition 01; page changes inside a panel with
//               transition 01 (fast); Back / Close play the secondary-button presses under the cursor; any other action the
//               primary-button press; Settings opens with the toggles-and-sliders piece; the travel screen loops the loading
//               spinner; toasts in the Ark slide in as notice toasts with the real messages.
//  Economy      a purchase: the shop card (item, price); claimed rewards: the reward popup (real amounts) and the resource
//               floaters with the new wallet; a level-up: LEVEL UP with the real level; the Team page: battle power.
//  Summon       the factory opens with banner select; a summon: ticket consume (when tickets are spent; real count), the summon
//               button charge, then the reveal of the best rarity pulled (rare / epic / legendary / secret) and, for ten, the
//               x10 card backs; mail and quest claims, season pass and campaign map pages, arena rank, tower floor, boss rush
//               round and the two arcade games each have their piece.
//  Combat       stage start: transition 04 (05 on a chapter's boss level) + the skill buttons; the boss: boss intro; a mini
//               boss: the alert; combo milestones: the combo counter (real count); big hits: the critical number (real
//               value) over the angel; ultimate charged: ULTIMATE READY; switching: the squad switcher; parry / perfect dodge /
//               blocked hit: the defense prompts; low health: the HP bar warning; a new target: the target lock on it (real
//               distance); the end: VICTORY (real rewards) or DEFEAT.
//  Mobile       the three joysticks are the touch stick's art (rose ring in the Ark, tactical in fights, hex while climbing or
//               swimming); the action cluster pops in when a fight starts; switch / dodge / block presses flash; the full
//               mobile layout is shown once as the controls tutorial.
#pragma once

#include "CoreMinimal.h"

class AActor;
class ABIBEnemy;

namespace BIB
{
	class FProfile;
	namespace Pages
	{
		struct FUiState;
	}
}

namespace BIBMotionEvents
{
	struct FSnapshot
	{
		int64 Currency[16] = {};
		int32 LevelSum = 0;
		int32 TopLevel = 0;
		int32 SummonCount = 0;
		int32 MailClaimed = 0;
		int32 Equipment = 0;
		int32 Tab = 0;
		FString Page;
	};
	FSnapshot Snap(const BIB::FProfile& Profile, const BIB::Pages::FUiState& State);

	void MenuOpened();
	void StationOpened(const FString& Title, const FString& Code, const FString& StationId);
	void PageOpened(const FString& Title);
	void BackPressed();
	void ClosePressed();
	/** After a panel action: compares the profile before and after and plays the matching piece. */
	void AfterAction(const FSnapshot& Before, const BIB::FProfile& Profile, const BIB::Pages::FUiState& State, const FString& ActionId, const FString& Message);
	void Travel(bool bOn, const FString& Label);
	void ArkToasts(const TArray<FString>& Lines);

	void StageStarted(bool bBossLevel, bool bTouch, const FString& Title);
	void Victory(const FString& Rewards);
	void Defeat();
	void BossIntro(const FString& Chapter);
	void MiniBoss();
	void ComboMilestone(int32 Hits);
	void BigHit(const FVector2D& Screen, float Value);
	void UltimateReady();
	void Switched(bool bTouch);
	void Defense(int32 Kind);   // 0 block, 1 parry, 2 dodge
	void LowHealth();
	void TargetLock(const FVector2D& Screen, float DistanceM);
	void TowerFloor(int32 Floor);
	void BossRushRound(int32 Round);
	void ArcadeStart(int32 Kind);   // 0 night shift, 1 rhythm
	void ArenaRank(const FString& Rank, const FString& Score);
	void TouchPress(const FString& Clip, const FVector2D& Screen);   // mobile-switch-dodge: switch / dodge / block
	void TouchTutorial();

	// ---- Set 2 (2026-10-09): world VFX from the pack, pinned to characters (sheets 09-18) and the screens that had no piece
	/** An operator's ability VFX: Operator = roster name ("Luna"), SlotKey = "M1", "1", "2", "3", "S", "U", "D1".."D3", "U2", ... */
	bool Ability(AActor* Caster, const FString& Operator, const FString& SlotKey, AActor* Target = nullptr);
	/** A shared effect by its slug on an actor (dash-attack-trail, jump-dust, angel-death-dissolve ...). */
	bool Shared(const FString& Slug, AActor* On, bool bFlip = false, const FString& Tag = FString(), bool bLoop = false);
	/** Keeps an angel's looping overlays in step with its state (statuses, stun halo, guard, poise, Judge verdicts). */
	void SyncEnemy(ABIBEnemy* Enemy, const FString& ActiveOperator, bool bJudgeVerdicts);
	void EnemyDied(ABIBEnemy* Enemy);
	/** Mirrors a caster's VFX when it faces left on screen (the pack draws casters facing right). */
	bool FacesLeft(AActor* Caster);
	void Dialogue(bool bOpen, const FString& Speaker, const FString& Line);
	void Section(bool bClear, int32 Index, int32 Count, const FString& Objective);
	void Gauges(const FString& Operator);
	void JudgeMode(int32 Mode, bool bLocked);   // 0 witness, 1 lawyer, 2 judge
	void Exchange(bool bOpen);
}
