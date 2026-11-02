// Boldinbeauty demo — combat numbers used by the Unreal layer.
//
// SOURCE      = Luna.md / Judge.md / Narrator:Diamon.md / BoldInBeauty_MAIN.
// PLACEHOLDER = not specified anywhere yet (OPEN/DEFERRED); needed only so the demo runs.
// Distances are Unreal centimetres, times are seconds.
#pragma once

namespace BIBTuning
{
	// ------------------------------------------------------------ shared movement — PLACEHOLDER
	constexpr float WalkSpeed = 450.f;
	constexpr float RunSpeed = 700.f;
	constexpr float BaseJumpZ = 560.f;                // with PlayerGravityScale: ~85 cm, quick up and down (2026-10-09)
	constexpr float PlayerGravityScale = 1.9f;        // was 1.0 (floaty); owner: "jump height feels way to high"
	constexpr float MaxJumpMult = 1.25f;              // kit jump multipliers are capped here (Luna's x2 jumped 3.6 m)
	constexpr float MouseSensitivity = 0.35f;

	// ------------------------------------------------------------ shared actions — PLACEHOLDER
	constexpr float BlockDamageFactor = 0.5f;
	constexpr float ParryWindow = 0.3f;
	constexpr float ParryCooldown = 1.0f;
	constexpr float ParryStun = 1.0f;
	constexpr float DodgeDuration = 0.25f;
	constexpr float DodgeSpeed = 1600.f;
	constexpr float DodgeCooldown = 1.0f;
	constexpr float SwitchCooldown = 1.0f;
	constexpr float MeleeRange = 260.f;

	// ------------------------------------------------------------ world — SOURCE (MAIN §7) + PLACEHOLDER
	constexpr float SectionHalfSize = RunSpeed * 8.f; // "about 8 seconds running from centre to a wall"
	constexpr float StartRoomHalfSize = 1500.f;
	constexpr float WallHeight = 600.f;
	constexpr float CeilingHeight = 2200.f;           // invisible, so inverted gravity has a roof
	constexpr float DoorWidth = 700.f;

	// ------------------------------------------------------------ enemies — PLACEHOLDER
	constexpr float GruntHP = 2000.f, GruntDamage = 40.f, GruntSpeed = 330.f;
	constexpr float BruteHP = 5000.f, BruteDamage = 80.f, BruteSpeed = 260.f;
	constexpr float MiniBossHP = 40000.f, MiniBossDamage = 120.f, MiniBossSpeed = 300.f;
	constexpr float BossHP = 120000.f, BossDamage = 200.f, BossSpeed = 280.f;
	constexpr float EnemyAttackInterval = 1.6f;
	constexpr float EnemyAttackWindup = 0.5f;
	// Chapters 2-13 add ranged, mage and assassin enemies; their numbers are in BIBRules/BIBCampaign.cpp (the archetype table).
	constexpr float RangedHitSlack = 100.f;           // extra flight range of a shot (shots fly since 2026-10-13, BIBEnemyBrain.cpp)
	// Since 2026-10-13 (goal 2) the angels' brain (BIBRules/BIBAngelBrain.h) decides when the assassin lunges (telegraphed, at
	// AssassinDashSpeed) and when the boss slams (its Nova; cooldown in BIB::Angel::MoveCooldown). The old fixed timers
	// (AssassinDashMin/MaxDistance, AssassinDashCooldown, BossSlamInterval) are kept for reference only.
	constexpr float AssassinDashSpeed = 1700.f, AssassinDashMinDistance = 700.f, AssassinDashMaxDistance = 1800.f, AssassinDashCooldown = 4.f;
	constexpr float BossSlamInterval = 7.f, BossSlamWindup = 1.5f, BossSlamRadius = 700.f, BossSlamDamage = 300.f;
	constexpr float BossInstantKillFraction = 0.15f;  // "die instantly or do huge damage for bosses"

	// ------------------------------------------------------------ Luna — SOURCE unless marked
	// Basic-attack strings (2026-10-09, owner gave full creative control over movesets): every hand-written kit swings a 4-hit
	// string; per-hit damage = attack x step multiplier x this factor (about twice the old basic damage per second).
	// Luna 0.3 -> 0.45 (2026-10-13 rebalance, owner: "you are allowed to alter hand written kits ... violently unbalanced like
	// luna's"; PROPOSED). The string's timing (hit frames, step lengths) is in Config/DefaultGame.ini [BIB.CombatFeel] Combo<n>.*.
	constexpr float LunaComboFactor = 0.45f, JudgeComboFactor = 0.75f, NarratorComboFactor = 1.f, PlaceholderComboFactor = 0.8f;
	constexpr float LunaPunchCooldown = 2.f;
	constexpr float LunaPunchDuration = 1.f;
	constexpr float LunaCancelHealthFraction = 0.15f; // "Requires loss of 15% of max health to cancel"
	constexpr float BlackholeWindup = 0.5f, BlackholeShoot = 1.f, BlackholeOrbit = 3.f, BlackholeFinal = 2.f;
	// 2026-10-13 rebalance (PROPOSED; owner numbers were tick 1250, explosion 2800): Luna sat at power 1.69 against the 4-star band
	// 0.75-0.90 (Tests/kit_balance.cpp yardstick); now ~0.84 with the ultimate changes below.
	constexpr float BlackholeHitDamage = 1250.f, BlackholeTickDamage = 500.f, BlackholeExplosionDamage = 2200.f;
	constexpr float BlackholeRecovery = 0.4f;         // PROPOSED: Luna is locked for the windup + this, the orb flies on by itself
	                                                  // (was locked 4.5 s through windup, shot and orbit)
	constexpr float BlackholeCooldown = 8.f;          // PLACEHOLDER (not given)
	constexpr float BlackholeOrbRadius = 220.f;       // PLACEHOLDER
	constexpr float BlackholeSuckRadius = 700.f;      // PLACEHOLDER
	constexpr float BlackholeExplosionRadius = 650.f; // PLACEHOLDER ("medium sized")
	constexpr float BlackholeTravelSpeed = 900.f;     // PLACEHOLDER
	constexpr float OrbitingWindup = 2.f, OrbitingDuration = 90.f, OrbitingCooldown = 30.f;
	constexpr float OrbitingContactDelay = 0.2f, OrbitingTickInterval = 1.f, OrbitingDamage = 149.f;
	constexpr float OrbitingRadius = 260.f;           // PLACEHOLDER
	constexpr float OrbitingPullRadius = 900.f;       // PLACEHOLDER
	constexpr float OrbitingPullSpeed = 160.f;        // PLACEHOLDER ("slowly pulls")
	constexpr float EventHorizonDps = 250.f, EventHorizonNukeDamage = 2000.f, EventHorizonCooldown = 50.f;
	constexpr float EventHorizonRadius = 3000.f;      // PLACEHOLDER ("massive area")
	constexpr float EventHorizonPullSpeed = 450.f;    // PLACEHOLDER
	constexpr float EventHorizonCentre = 170.f;       // PLACEHOLDER (enemy "reaches the middle")
	constexpr float EventHorizonNukeRadius = 600.f;   // PLACEHOLDER ("close range")
	constexpr float ExpelBaseDamage = 750.f, ExpelDamagePerSecond = 250.f, ExpelMaxCharge = 8.f, ExpelCooldown = 45.f;
	constexpr float ExpelInterruptHealthFraction = 0.10f;
	constexpr float ExpelSmallRadius = 350.f, ExpelMediumRadius = 800.f, ExpelLargeRadius = 1600.f; // PLACEHOLDER sizes
	// 2026-10-13 rebalance (PROPOSED; owner numbers were a 5 s cutscene, 25 s, 55000 to everything on the map, and the ultimate's
	// own damage recharged it): the cinematic beat (BIBPlayerFeel.cpp) is the cutscene; then a short settle, a 10 s fuse, 9000 to
	// everything within 16 m; Luna's ultimate charges at half rate; one Destabilisation at a time.
	constexpr float DestabilisationCutscene = 0.6f, DestabilisationDuration = 10.f, DestabilisationFlip = 2.5f;
	constexpr float DestabilisationDamage = 9000.f;
	constexpr float DestabilisationRadius = 1600.f;
	constexpr float LunaUltGainMult = 0.5f;

	// ------------------------------------------------------------ Judge — SOURCE unless marked
	constexpr float JudgeWalkSpeed = 260.f;           // PLACEHOLDER ("walk slowly", "8 speed")
	constexpr float JudgeJumpZ = 260.f;               // PLACEHOLDER ("can slightly jump", "5 jump")
	constexpr float JudgePunchCooldown = 0.8f;        // PLACEHOLDER
	constexpr float JudgeInnocentSelfDamageFraction = 0.05f; // PLACEHOLDER ("set amounts of damage")
	constexpr int32 JudgeHitsToCondemn = 10;
	constexpr float JudgeDamageFractionToCondemn = 0.10f;
	constexpr float PaperworkWitnessWindup = 1.f, PaperworkWitnessCooldown = 15.f;
	constexpr int32 PaperworkOrders = 3;
	constexpr float PaperworkJudgeWindup = 1.25f, PaperworkJudgeCooldown = 25.f, PaperworkStun = 3.f;
	constexpr float LawyerQuietInterval = 0.5f, LawyerRecovery = 2.f;
	constexpr float CourtCooldown = 5.f, CourtWindup = 1.f, CourtJudgeWindup = 2.5f, CourtPenaltyLock = 30.f;
	constexpr float GavelWindup = 0.5f, GavelSpacing = 0.5f, GavelForeSwing = 0.75f, GavelCooldown = 20.f;
	constexpr float GavelStunPerHit = 0.5f, GavelStackLifetime = 60.f;
	constexpr float GavelRadius = 650.f;              // PLACEHOLDER ("medium")
	constexpr float RetributionWindup = 1.f, RetributionDuration = 1.f, RetributionForeSwing = 1.f;
	constexpr float RetributionCooldown = 15.f, RetributionStun = 1.f;
	constexpr float RetributionRadius = 380.f;        // PLACEHOLDER ("short range")
	constexpr float JusticeStartRadius = 700.f;       // PLACEHOLDER ("large")
	constexpr float JusticeGrowthPerSecond = 0.20f, JusticeFailSafe = 12.f, JusticeRegAttackBuff = 45.f;

	// ------------------------------------------------------------ Narrator / Diano — SOURCE unless marked
	constexpr float NarratorStartSpeed = 25.f, NarratorSpeedGain = 25.f, NarratorExplodeSpeed = 1000.f;
	constexpr float NarratorExplodeDamage = 12500.f;
	constexpr float DianoStartSpeed = 125.f, DianoSpeedGain = 125.f, DianoExplodeSpeed = 5000.f, DianoExplodeDamage = 30000.f;
	constexpr float SpeedJumpThreshold = 100.f;       // Narrator; Diano needs 5x
	constexpr float SelfExplosionRadius = 4000.f;     // PLACEHOLDER ("massive area")
	// Owner request 2026-10-04: above the base speed (the starting speed: 25, Diano 125) the operator is pushed forward at
	// (Speed - base speed) and also walks faster by that amount. The cm/s per speed point and the cap are PLACEHOLDERS.
	constexpr float SpeedToCmPerSecond = 1.5f;        // PLACEHOLDER movement mapping
	constexpr float SpeedMoveCap = 2600.f;            // PLACEHOLDER (keeps the demo controllable)
	constexpr float SpeedPerX = 125.f;                // "1x speed is 125"
	constexpr float ExplosionSpeedCost = 100.f, ExplosionDamage = 1250.f, ExplosionCooldown = 1.f; // cooldown PLACEHOLDER
	constexpr float ExplosionRadius = 650.f;          // PLACEHOLDER ("medium sized")
	constexpr float StillAliveParry = 0.3f, StillAliveCooldown = 1.f, StillAliveLongCooldown = 45.f;
	constexpr float CloseEnoughWindup = 3.f, CloseEnoughThreshold = 0.20f, CloseEnoughBossThreshold = 0.05f;
	constexpr float SwitchDuration = 25.f, SwitchTransform = 0.5f, SwitchBlockWindow = 3.f;
	constexpr float SwitchCounterDamage = 10000.f, SwitchBlockBonus = 5.f;
	constexpr float AccelerationWindow = 0.3f, AccelerationChain = 0.5f, AccelerationCooldown = 10.f;
	constexpr float AccelerationDamage = 499.f, AccelerationSpeedGain = 100.f, AccelerationRepeatLock = 8.f;
	constexpr float AccelerationDashSpeed = 2200.f;   // PLACEHOLDER
	constexpr float EndlessGodTick = 0.1f, EndlessGodHealth = 80.f, EndlessGodDamage = 20.f, EndlessGodSeconds = 2.f;
	constexpr float EndlessGodPermanentHealth = 4000.f, EndlessGodPermanentDamage = 1000.f;
	constexpr float ImperfectionBurn = 999.f, ImperfectionCooldown = 60.f;
	constexpr float ImperfectionAutoAt = 4999.f, ImperfectionAutoAttack = 19.f, ImperfectionAutoHealth = 77.f, ImperfectionAutoSpeed = 125.f;
	constexpr float BodyCheckCooldown = 0.5f;         // PLACEHOLDER
	constexpr float WallHitCooldown = 0.5f;           // PLACEHOLDER
	constexpr float NarratorPunchCooldown = 0.5f;     // PLACEHOLDER
	constexpr float ExchangeStep = 0.10f;             // PLACEHOLDER (demo exchanges 10% per click)
	constexpr float ExchangeFloor = 0.10f;            // "lower it by up to 90%"
}
