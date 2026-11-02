// Boldinbeauty demo — engine-independent game rules.
//
// This file deliberately has no Unreal includes so the rules can be compiled and
// unit-tested outside the engine (see Demo/BoldInBeautyDemo/Tests). Every number
// either comes from BoldInBeauty_MAIN / the operator files, or is a clearly
// labelled PLACEHOLDER in BIBConfig.h for an item MAIN marks OPEN/DEFERRED.
//
// Rules code must stay exception-free and RTTI-free (Unreal builds with both off).
#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "BIBMeta.h"

namespace BIB
{
	// ---------------------------------------------------------------- roster
	enum class EKit : int
	{
		None = 0,    // unused
		Luna,
		Judge,
		Narrator,
		Placeholder, // no kit data: regular attack, block, parry and dodge only (kept for tests / future operators)
		// Data-driven kits (BIBKitData*.cpp): owner-designed (Alchemist, Arita, Adapt) and designed from the owner's
		// playstyle notes on 2026-10-04 (the rest). See docs/operator-kits.md.
		Alchemist,
		Arita,
		Adapt,
		Blood,
		Death,
		Fujita,
		Gambler,
		Ishii,
		Jester,
		Kuu,
		Media,
		Myrcia,
		Plack,
		Shiraishi,
		Stargazer,
		Timekeeper,
		Weather
	};

	struct FRosterEntry
	{
		const char* Name;
		int Stars;        // summon rarity 3..6 (MAIN §5)
		EKit Kit;
		int BaseHP;       // from operator file; 0 when no kit
		int BaseAttack;   // regular attack damage from operator file; 0 when no kit
	};

	const std::vector<FRosterEntry>& Roster();
	int FindOperator(const char* Name); // -1 when missing
	const char* StarsLabel(int Stars);  // "3★ Rare" etc.

	// ------------------------------------------------------------ currencies
	enum class ECurrency : int
	{
		Gold = 0,      // a.k.a. Money (WORKING alias, MAIN §3)
		Diamonds,
		Bytes,
		AngelHearts,
		RegularTicket,
		SpecialTicket,
		DailyTicket,
		Mileage,
		ArcadeToken,   // PLACEHOLDER arcade currency (owner request 2026-10-04)
		GangCoin,      // gang shop currency, paid at the end of each gang season (owner request 2026-10-06)
		// Owner request 2026-10-07 (classic gacha rebalance, inventory, elements, events). PLACEHOLDER amounts everywhere.
		Charge,        // stamina: campaign replays cost Charge, first clears are free (BIBInventory.h)
		ElementCore,   // Element Attunement upgrades (BIBElement.h)
		EventToken,    // the weekly event shop (BIBEvents.h)
		ArkMedal,      // the Ark shop: earned from arena fights and tower floors (BIBEvents.h)
		Count
	};
	const char* CurrencyName(ECurrency C);

	// ------------------------------------------------------------- equipment
	enum class ESlot : int
	{
		Helmet = 0,
		Chest,
		Leggings,
		Weapon,
		Count
	};
	const char* SlotName(ESlot S);

	enum class EBonus : int
	{
		Damage = 0,
		Defence
	};

	struct FEquipment
	{
		int Id = 0;
		ESlot Slot = ESlot::Helmet;
		EBonus Bonus = EBonus::Damage;
		double Percent = 0.0;   // PLACEHOLDER small % (MAIN §4: exact values OPEN)
		int UpgradeLevel = 0;   // PLACEHOLDER Bytes upgrades
		int EquippedBy = -1;    // roster index or -1
		std::string Name;
	};

	// ------------------------------------------------------------- operators
	struct FOwnedOperator
	{
		bool bOwned = false;
		int Level = 1;
		int UpgradeStars = 0;   // duplicate upgrade stars (separate from summon rarity)
		int DupResource = 0;    // unnamed duplicate resource (operator-specific: PLACEHOLDER, MAIN OPEN)
		int HeartGatesPaid = 0; // Angel-heart gates passed (gate k sits at level 20k)
		int Attunement = 0;     // Element Attunement 0..10 (BIBElement.h, owner request 2026-10-07)
	};

	struct FCombatStats
	{
		double MaxHP = 0.0;
		double DamageMult = 1.0;
		double DefencePercent = 0.0;
	};

	// ------------------------------------------------------------- summoning
	enum class EBannerType : int
	{
		Standard = 0,
		Featured, // = Special banner
		Daily
	};

	struct FBanner
	{
		std::string Id;
		std::string Title;
		EBannerType Type = EBannerType::Standard;
		int FeaturedOperator = -1; // roster index; must be a 5★ or 6★
		int RotationStars = 0;     // 5 or 6: this Featured banner rotates through that rarity's pool every 3 days; 0 = fixed
	};

	struct FPityState
	{
		int Since5 = 0;         // rolls since last 5★-or-higher
		int Since6 = 0;         // rolls since last 6★
		bool bLostFiftyFifty = false;
	};

	enum class EPay : int
	{
		Ticket = 0,
		Diamonds
	};

	struct FSummonResult
	{
		int Operator = -1;
		int Stars = 3;
		bool bNew = false;
		bool bFeatured = false;
		bool bPity = false;           // forced by 100/2000-roll pity
		bool bTenPullGuarantee = false;
		bool bFiftyFiftyGuarantee = false;
	};

	struct FSummonOutcome
	{
		bool bOk = false;
		std::string Message;
		std::vector<FSummonResult> Results;
	};

	struct FCost
	{
		int Gold = 0;
		int Bytes = 0;
	};

	struct FActionResult
	{
		bool bOk = false;
		std::string Message;
	};

	// ----------------------------------------------------------------- tutorial
	// New accounts play a short guided tutorial (BIBTutorial.h). The step is saved with the profile (by name, see
	// TutorialStepName), so a player who quits half-way resumes where they were.
	enum class ETutorialStep : int
	{
		OpenOperators = 0, // open the Operator tab
		SelectLuna,        // tap Luna
		LevelUpLuna,       // level Luna up once
		OpenTeam,          // open the Team tab (the squad is empty)
		AddLuna,           // tap an empty slot: only Luna can be added
		OpenSummon,        // open the Summon tab
		SummonKuu,         // forced first summon: Kuu
		OpenCampaign,      // open the Campaign tab
		PlayLevel1,        // play Chapter 1 level 1; clearing it unlocks Plack for free
		Complete,
		Count
	};

	// ----------------------------------------------------------------- account
	// Who is playing. Filled from the signed-in session (or the offline guest), never read from a save file: the server alone
	// decides who is an admin.
	struct FAccountInfo
	{
		std::string UserId;    // the account server's id, e.g. "10000001"
		std::string Username;
		std::string Email;
		bool bSignedIn = false;
		bool bGuest = false;   // offline play without an account (saved locally, never an admin)
		bool bAdmin = false;   // the owner/admin account
	};

	// ----------------------------------------------------------------- profile
	class FProfile
	{
	public:
		explicit FProfile(uint32_t Seed = 20090101u);

		void ResetToNewGame();

		// currencies
		int Get(ECurrency C) const;
		void Add(ECurrency C, int Amount);
		bool Spend(ECurrency C, int Amount);

		// For the save system: the next equipment id, and the default banner set (rotation applied for RotationIndex).
		int NextEquipmentIdValue() const { return NextEquipmentId; }
		void SetNextEquipmentId(int Id) { NextEquipmentId = Id; }
		void ResetBannersToDefault() { PlaceDefaultBanners(); }

		// Raw stored amounts, for the save system. Get() reports UnlimitedAmount while bUnlimited is set.
		int Raw(ECurrency C) const;
		void SetRaw(ECurrency C, int Amount);

		// operators
		std::vector<FOwnedOperator> Operators; // indexed by roster index
		bool Owns(int RosterIndex) const;
		// Grants the operator or, when already owned, the duplicate reward. Returns true when new.
		bool GrantOperator(int RosterIndex);
		// Owns every operator at level 1, no duplicate resources (admin account, developer profile, test hooks).
		void GrantAllOperators();

		// progression (MAIN §3)
		int LevelCap(int RosterIndex) const;
		static FCost LevelUpCost(int FromLevel);
		static int HeartGateCost(int GateNumber); // gate 1 = level 20
		bool NeedsHeartGate(int RosterIndex) const;
		static int UpgradeStarCost(int TargetStar);
		static int DiamondsToCover(int GoldShort, int BytesShort);
		FActionResult TryLevelUp(int RosterIndex, bool bSupplementWithDiamonds);
		FActionResult TryPayHeartGate(int RosterIndex);
		FActionResult TryUpgradeStar(int RosterIndex);
		FCombatStats GetCombatStats(int RosterIndex) const;

		// equipment (MAIN §4)
		std::vector<FEquipment> Inventory;
		FEquipment& AddRandomEquipment();
		FEquipment& AddEquipment(ESlot Slot, EBonus Bonus, double Percent, const std::string& Name);
		/** At Config::EquipmentInventoryCap items, drops the weakest unequipped, un-upgraded item to make room. */
		void MakeRoomForEquipment();
		FActionResult Equip(int EquipmentId, int RosterIndex);
		FActionResult Unequip(int EquipmentId);
		FActionResult UpgradeEquipment(int EquipmentId);
		static int EquipmentUpgradeCost(int CurrentUpgradeLevel);
		FEquipment* FindEquipment(int EquipmentId);
		const FEquipment* EquippedIn(int RosterIndex, ESlot Slot) const;

		// team (MAIN §6: bring 3 operators, one active at a time)
		int Team[3] = {-1, -1, -1};
		FActionResult SetTeamSlot(int Slot, int RosterIndex);
		// A squad is playable when every filled slot holds a different owned operator with a kit and it is as full as the
		// collection allows: three operators, or all of them while the player owns fewer than three (a new account owns only
		// Luna, then Kuu). An empty squad is never playable.
		bool TeamIsPlayable() const;
		int RequiredTeamSize() const;
		int PlayableOwnedCount() const;
		// Puts the operator into the first empty squad slot. False when it is already in the squad or no slot is free.
		bool AutoPlaceInTeam(int RosterIndex);

		// summoning (MAIN §2)
		std::vector<FBanner> Banners;
		std::vector<FPityState> Pity; // parallel to Banners; pity resets per banner
		int FindBanner(const std::string& Id) const;
		FSummonOutcome Summon(int BannerIndex, int Count, EPay Pay);
		FSummonOutcome TutorialSummon(); // Chapter 1: Kuu from the tutorial 1× summon
		bool bTutorialSummonDone = false;
		FSummonOutcome RedeemMileage(int BannerIndex);

		// days (Daily ticket: one per day, expires, never accumulates)
		int Day = 1;
		void AdvanceDay();

		// merchant (PLACEHOLDER prices)
		FActionResult BuyFromMerchant(int OfferIndex);
		static int MerchantOfferCount();
		static std::string MerchantOfferLabel(int OfferIndex);

		// meta-game: mail, quests, season pass, Outer World, arcade, tower, arena, settings (BIBMeta.h)
		FMeta Meta;

		// stages (StagesCleared = highest campaign level cleared, counted through all 13 chapters: 8 = Chapter 1 done, 16 = Chapter 2
		// done; levels unlock in order, see BIBMeta.h for the stage id scheme)
		int StagesCleared = 0;
		std::vector<std::string> ApplyStageRewards(int StageNumber, bool bChapterFinal);

		// ---- accounts (owner request 2026-10-04)
		FAccountInfo Account;
		// Admin account: every currency reads Config::UnlimitedAmount and never decreases, duplicates and Mileage included.
		bool bUnlimited = false;
		// Starting states. ResetToNewGame() stays the developer/test profile (Luna, Judge and Narrator, no tutorial).
		void StartNewPlayer();   // only Luna, an empty squad, the tutorial pending
		void StartAdmin();       // everything unlocked, unlimited currencies, no tutorial
		// Re-starts whichever kind of profile this is (admin, signed-in player, guest or developer). The demo "Reset profile".
		void ResetForAccount();
		// Binds the session to this profile: name, id, and (for an admin) the entitlements. Keeps all progress.
		void ApplyAccount(const FAccountInfo& Info);
		void ApplyAdminEntitlements();

		// ---- tutorial
		int TutorialStep = static_cast<int>(ETutorialStep::Complete);
		bool TutorialActive() const { return TutorialStep < static_cast<int>(ETutorialStep::Complete); }

		// ---- featured banner rotation (owner request 2026-10-04: every 3 days)
		// SetTime gives the profile the current UTC time. When a new 3-day period has begun, the Featured banners switch to the
		// next operator of their pool and that banner's pity resets (WORKING: MAIN §2 says 5-star pity resets each banner).
		int64_t NowUnix = 0;       // last time given to SetTime; 0 = never
		int64_t RotationIndex = 0; // the period the Featured banners currently show
		static int64_t RotationIndexAt(int64_t UnixSeconds);
		static int64_t RotationStartUnix(int64_t Index);
		void SetTime(int64_t UnixSeconds);
		int64_t SecondsUntilRotation() const; // -1 when SetTime has not been called
		static int FeaturedOperatorFor(int Stars, int64_t Index);
		void ApplyRotation(bool bResetPityOnChange);

		// demo-only helpers
		void GrantDemoCurrency();

		std::mt19937& Rng() { return Random; }

	private:
		int Currencies[static_cast<int>(ECurrency::Count)] = {};
		int NextEquipmentId = 1;
		std::mt19937 Random;

		double Roll01();
		int PickUniform(const std::vector<int>& Candidates);
		std::vector<int> OperatorsOfStars(int Stars, int Excluding) const;
		int RollRarity();
		FSummonResult RollOne(int BannerIndex, bool bCountPity, bool bForceAtLeastEpic);
		void PlaceDefaultBanners();
	};
}
