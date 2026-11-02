#include "BIBAngelBrain.h"

#include <algorithm>
#include <cmath>

namespace BIB
{
	namespace Angel
	{
		// Named (not anonymous) so unity builds cannot clash with other files' helpers.
		namespace BrainImpl
		{
			constexpr double Pi = 3.14159265358979323846;
			constexpr int RoleCount = 7;
			constexpr int MoveCount = static_cast<int>(EMove::Count);
			constexpr double TelegraphFloor = 0.25;   // no knob can push a telegraph under this (fairness)
			constexpr double TimelineSeconds = 2.5;

			double Clamp01(double Value) { return std::clamp(Value, 0.0, 1.0); }
			int RoleIndex(ERole Role) { return std::clamp(static_cast<int>(Role), 0, RoleCount - 1); }
			bool IsBossRole(ERole Role) { return Role == ERole::MiniBoss || Role == ERole::Boss; }
			bool IsRangedRole(ERole Role) { return Role == ERole::Ranged || Role == ERole::Mage; }

			FKnobs MakeKnobs(double Reaction, double Aggression, double Dodge, double Block, double Adapt, double Punish, double Feint,
				double Turn, double Telegraph, double DodgeCooldown, double RetreatHealth, double Sight, double Memory, double IntervalMult)
			{
				FKnobs Knobs;
				Knobs.ReactionSeconds = Reaction;
				Knobs.Aggression = Aggression;
				Knobs.DodgeChance = Dodge;
				Knobs.BlockChance = Block;
				Knobs.AdaptRate = Adapt;
				Knobs.PunishChance = Punish;
				Knobs.FeintChance = Feint;
				Knobs.TurnRateDeg = Turn;
				Knobs.MinTelegraph = Telegraph;
				Knobs.DodgeCooldown = DodgeCooldown;
				Knobs.RetreatHealth = RetreatHealth;
				Knobs.SightRange = Sight;
				Knobs.MemorySeconds = Memory;
				Knobs.AttackIntervalMult = IntervalMult;
				return Knobs;
			}

			FAiTuning MakeDefaults()
			{
				FAiTuning Tuning;
				// Base values before Difficulty (1.15) and the chapter. PROPOSED.
				Tuning.Tier[static_cast<int>(ETier::Regular)] = MakeKnobs(0.30, 0.55, 0.25, 0.25, 1.0, 0.55, 0.25, 360.0, 0.35, 2.5, 0.25, 3000.0, 4.0, 1.0);
				Tuning.Tier[static_cast<int>(ETier::Elite)] = MakeKnobs(0.24, 0.70, 0.40, 0.35, 1.4, 0.70, 0.35, 300.0, 0.30, 1.8, 0.20, 4000.0, 6.0, 0.9);
				Tuning.Tier[static_cast<int>(ETier::Boss)] = MakeKnobs(0.20, 0.80, 0.50, 0.40, 1.7, 0.80, 0.40, 240.0, 0.30, 1.5, 0.15, 6000.0, 8.0, 0.85);
				return Tuning;
			}

			FAiTuning& LiveTuning()
			{
				static FAiTuning Tuning = MakeDefaults();
				return Tuning;
			}

			FAiStats& LiveStats()
			{
				static FAiStats Stats;
				return Stats;
			}

			/** Signed angle (radians, -pi..pi) from A to B. */
			double AngleBetween(const FV2& A, const FV2& B)
			{
				return std::atan2(A.X * B.Y - A.Y * B.X, A.X * B.X + A.Y * B.Y);
			}

			ESide SideOf(const FV2& Direction, const FV2& Facing)
			{
				const FV2 Dir = Normal(Direction);
				const FV2 Face = Normal(Facing);
				const double Ahead = Dot(Dir, Face);
				if (Ahead > 0.5)
				{
					return ESide::Forward;
				}
				if (Ahead < -0.5)
				{
					return ESide::Back;
				}
				return Dot(Dir, Perp(Face)) > 0.0 ? ESide::Left : ESide::Right;
			}

			/** How much the habits steer this tier's choices (0..1). */
			double HabitWeight(const FKnobs& Knobs) { return std::min(1.0, 0.5 * std::max(0.0, Knobs.AdaptRate)); }

			bool Recently(const FBrainState& State, EMove Move)
			{
				return State.Recent[0] == Move || State.Recent[1] == Move || State.Recent[2] == Move;
			}

			void Remember(FBrainState& State, EMove Move)
			{
				State.Recent[2] = State.Recent[1];
				State.Recent[1] = State.Recent[0];
				State.Recent[0] = Move;
			}

			/** The windup multiplier of a move on the archetype's telegraph (before the MinTelegraph clamp). */
			double WindupMult(EMove Move)
			{
				switch (Move)
				{
				case EMove::Combo: return 0.9;
				case EMove::Sweep: return 1.3;
				case EMove::Unblockable: return 1.35;
				case EMove::Blink: return 0.9;
				case EMove::DashStrike: return 1.0;
				case EMove::AntiAir: return 0.9;
				case EMove::Nova: return 1.5;
				case EMove::Volley: return 1.2;
				case EMove::Charge: return 1.4;
				case EMove::Lunge: return 1.2;
				default: return 1.0;
				}
			}

			/** Which moves a role's kit holds. */
			bool InKit(ERole Role, EMove Move)
			{
				switch (Role)
				{
				case ERole::Melee:
					return Move == EMove::Strike || Move == EMove::Combo || Move == EMove::Feint || Move == EMove::Unblockable || Move == EMove::Blink ||
						Move == EMove::DashStrike || Move == EMove::Sweep || Move == EMove::AntiAir;
				case ERole::Brute:
					return Move == EMove::Strike || Move == EMove::Combo || Move == EMove::Sweep || Move == EMove::Charge || Move == EMove::Unblockable ||
						Move == EMove::Nova;
				case ERole::Ranged:
					return Move == EMove::Strike || Move == EMove::Volley || Move == EMove::AntiAir;
				case ERole::Mage:
					return Move == EMove::Strike || Move == EMove::Nova || Move == EMove::AntiAir;
				case ERole::Assassin:
					return Move == EMove::Strike || Move == EMove::Combo || Move == EMove::Feint || Move == EMove::Unblockable || Move == EMove::Blink ||
						Move == EMove::Lunge;
				case ERole::MiniBoss:
				case ERole::Boss:
					return Move != EMove::Guard && Move != EMove::Flank && Move != EMove::Count;
				}
				return false;
			}

			/** Moves that strike from range (no melee reach needed). */
			bool IsRangedMove(ERole Role, EMove Move)
			{
				if (Move == EMove::Volley || Move == EMove::AntiAir)
				{
					return true;
				}
				return Move == EMove::Strike && IsRangedRole(Role);
			}

			/** Gap closers: the distance band (cm, measured beyond the reach) they are used from. */
			bool GapBand(EMove Move, double& Near, double& Far)
			{
				switch (Move)
				{
				case EMove::Blink: Near = 350.0; Far = 1400.0; return true;
				case EMove::DashStrike: Near = 300.0; Far = 1100.0; return true;
				case EMove::Charge: Near = 250.0; Far = 1200.0; return true;
				case EMove::Lunge: Near = 150.0; Far = 750.0; return true;
				default: return false;
				}
			}

			FV2 SideVector(ESide Side, const FV2& Facing)
			{
				const FV2 Left = Perp(Normal(Facing));
				switch (Side)
				{
				case ESide::Left: return Left;
				case ESide::Right: return Left * -1.0;
				case ESide::Back: return Normal(Facing) * -1.0;
				case ESide::Forward: return Normal(Facing);
				default: return {};
				}
			}

			/** Records the decision once (continuous intents only when they start) and returns it. */
			FAction Finish(const FThink& In, FBrainState& State, const FAction& Action)
			{
				const bool bEvent = Action.Intent == EIntent::Attack || Action.Intent == EIntent::Sidestep || Action.Intent == EIntent::Backstep ||
					Action.Intent == EIntent::Guard;
				if (bEvent || Action.Intent != State.LastIntent)
				{
					CountAction(In.Role, Action);
				}
				State.LastIntent = Action.Intent;
				return Action;
			}
		}

		// ------------------------------------------------------------------ math
		double Length(const FV2& A) { return std::sqrt(A.X * A.X + A.Y * A.Y); }
		double Dot(const FV2& A, const FV2& B) { return A.X * B.X + A.Y * B.Y; }
		FV2 Normal(const FV2& A)
		{
			const double Size = Length(A);
			return Size > 1e-6 ? FV2{A.X / Size, A.Y / Size} : FV2{};
		}
		FV2 Perp(const FV2& A) { return {-A.Y, A.X}; }
		FV2 Rotate(const FV2& A, double Radians)
		{
			const double C = std::cos(Radians);
			const double S = std::sin(Radians);
			return {A.X * C - A.Y * S, A.X * S + A.Y * C};
		}

		// ------------------------------------------------------------------ knobs
		FAiTuning& AiTuning() { return BrainImpl::LiveTuning(); }
		void ResetAiTuning() { BrainImpl::LiveTuning() = BrainImpl::MakeDefaults(); }

		std::vector<FAiConfigKey> AiConfigKeys(FAiTuning& Target)
		{
			std::vector<FAiConfigKey> Keys;
			for (int Index = 0; Index < TierCount; ++Index)
			{
				const std::string Prefix = std::string(TierName(static_cast<ETier>(Index))) + ".";
				FKnobs& K = Target.Tier[Index];
				Keys.push_back({Prefix + "ReactionSeconds", &K.ReactionSeconds});
				Keys.push_back({Prefix + "Aggression", &K.Aggression});
				Keys.push_back({Prefix + "DodgeChance", &K.DodgeChance});
				Keys.push_back({Prefix + "BlockChance", &K.BlockChance});
				Keys.push_back({Prefix + "AdaptRate", &K.AdaptRate});
				Keys.push_back({Prefix + "PunishChance", &K.PunishChance});
				Keys.push_back({Prefix + "FeintChance", &K.FeintChance});
				Keys.push_back({Prefix + "TurnRateDeg", &K.TurnRateDeg});
				Keys.push_back({Prefix + "MinTelegraph", &K.MinTelegraph});
				Keys.push_back({Prefix + "DodgeCooldown", &K.DodgeCooldown});
				Keys.push_back({Prefix + "RetreatHealth", &K.RetreatHealth});
				Keys.push_back({Prefix + "SightRange", &K.SightRange});
				Keys.push_back({Prefix + "MemorySeconds", &K.MemorySeconds});
				Keys.push_back({Prefix + "AttackIntervalMult", &K.AttackIntervalMult});
			}
			Keys.push_back({"Difficulty", &Target.Difficulty});
			Keys.push_back({"ReactionPerChapter", &Target.ReactionPerChapter});
			Keys.push_back({"ReactionFloor", &Target.ReactionFloor});
			Keys.push_back({"ExtraAttackers", &Target.ExtraAttackers});
			Keys.push_back({"TokenTurnSeconds", &Target.TokenTurnSeconds});
			Keys.push_back({"FlankFrontCone", &Target.FlankFrontCone});
			Keys.push_back({"HoldExtra", &Target.HoldExtra});
			Keys.push_back({"OutrangedMargin", &Target.OutrangedMargin});
			Keys.push_back({"MaxShooters", &Target.MaxShooters});
			Keys.push_back({"ShotStagger", &Target.ShotStagger});
			Keys.push_back({"ShotSpeed", &Target.ShotSpeed});
			Keys.push_back({"ShotLaneHalfWidth", &Target.ShotLaneHalfWidth});
			Keys.push_back({"ShotLead", &Target.ShotLead});
			Keys.push_back({"DodgeSideOffset", &Target.DodgeSideOffset});
			Keys.push_back({"SidestepDistance", &Target.SidestepDistance});
			Keys.push_back({"RegroupSeconds", &Target.RegroupSeconds});
			Keys.push_back({"RegroupCooldown", &Target.RegroupCooldown});
			Keys.push_back({"StrainCaution", &Target.StrainCaution});
			Keys.push_back({"FeintExtraDelay", &Target.FeintExtraDelay});
			Keys.push_back({"PunishReach", &Target.PunishReach});
			Keys.push_back({"HabitWindow", &Target.HabitWindow});
			return Keys;
		}

		const char* TierName(ETier Tier)
		{
			switch (Tier)
			{
			case ETier::Regular: return "Regular";
			case ETier::Elite: return "Elite";
			case ETier::Boss: return "Boss";
			default: return "?";
			}
		}

		ETier TierFor(ERole Role)
		{
			if (Role == ERole::Boss)
			{
				return ETier::Boss;
			}
			return Role == ERole::MiniBoss ? ETier::Elite : ETier::Regular;
		}

		FKnobs EffectiveKnobs(ETier Tier, int Chapter)
		{
			const FAiTuning& T = AiTuning();
			const int Index = std::clamp(static_cast<int>(Tier), 0, TierCount - 1);
			FKnobs K = T.Tier[Index];
			const double D = std::max(0.1, T.Difficulty);
			const int Ch = std::max(1, Chapter);
			K.ReactionSeconds = std::max(T.ReactionFloor, K.ReactionSeconds / D - T.ReactionPerChapter * static_cast<double>(Ch - 1));
			K.Aggression = BrainImpl::Clamp01(K.Aggression * D);
			K.DodgeChance = std::clamp(K.DodgeChance * D, 0.0, 0.85);   // never perfect
			K.BlockChance = std::clamp(K.BlockChance * D, 0.0, 0.85);
			K.PunishChance = std::clamp(K.PunishChance * D, 0.0, 0.95);
			K.FeintChance = std::clamp(K.FeintChance * D, 0.0, 0.8);
			K.AdaptRate = std::max(0.0, K.AdaptRate * D);
			K.AttackIntervalMult = std::max(0.3, K.AttackIntervalMult / D);
			K.TurnRateDeg = std::min(720.0, K.TurnRateDeg * D);
			K.MinTelegraph = std::max(BrainImpl::TelegraphFloor, K.MinTelegraph);
			K.DodgeCooldown = std::max(0.5, K.DodgeCooldown);
			return K;
		}

		// ------------------------------------------------------------------ timeline
		void FPlayerTimeline::Record(const FPlayerView& View)
		{
			if (!Views.empty() && View.Time <= Views.back().Time)
			{
				if (View.Time == Views.back().Time)
				{
					Views.back() = View;
					return;
				}
				Views.clear();   // time went backwards (new stage / reload)
			}
			Views.push_back(View);
			std::size_t Drop = 0;
			while (Drop + 1 < Views.size() && Views[Drop + 1].Time < View.Time - BrainImpl::TimelineSeconds)
			{
				++Drop;
			}
			if (Drop > 0)
			{
				Views.erase(Views.begin(), Views.begin() + static_cast<std::ptrdiff_t>(Drop));
			}
		}

		FPlayerView FPlayerTimeline::At(double Time) const
		{
			if (Views.empty())
			{
				return {};
			}
			for (auto It = Views.rbegin(); It != Views.rend(); ++It)
			{
				if (It->Time <= Time)
				{
					return *It;
				}
			}
			return Views.front();
		}

		bool FPlayerTimeline::Healing(double Time, double Window) const
		{
			if (Views.empty())
			{
				return false;
			}
			const FPlayerView Now = At(Time);
			double Lowest = Now.HealthFraction;
			for (const FPlayerView& View : Views)
			{
				if (View.Time >= Time - Window && View.Time <= Time)
				{
					Lowest = std::min(Lowest, View.HealthFraction);
				}
			}
			return Now.HealthFraction - Lowest > 0.02;
		}

		// ------------------------------------------------------------------ habits
		void FHabits::Observe(const FPlayerView& Before, const FPlayerView& After)
		{
			if (After.Time <= Before.Time)
			{
				return;
			}
			if (After.bDodging && !Before.bDodging)
			{
				const ESide Side = BrainImpl::SideOf(After.Vel, After.Facing);
				Push(EAction::Dodge, Side, After.Time);
				// Was it early? Compare with the nearest pending telegraph.
				const FTelegraph* Nearest = nullptr;
				for (const FTelegraph& Telegraph : Telegraphs)
				{
					if (After.Time >= Telegraph.StartedAt && After.Time <= Telegraph.HitAt + 0.1 && (!Nearest || Telegraph.HitAt < Nearest->HitAt))
					{
						Nearest = &Telegraph;
					}
				}
				if (Nearest)
				{
					if (Nearest->HitAt - After.Time > 0.25)
					{
						++EarlyDodges;
					}
					else
					{
						++TimedDodges;
					}
					if (EarlyDodges + TimedDodges > 20)
					{
						EarlyDodges /= 2;   // recent dodges matter most
						TimedDodges /= 2;
					}
				}
			}
			if (After.bAttacking && After.ActionStartedAt > Before.ActionStartedAt + 1e-6)
			{
				Push(After.bLongCast ? EAction::Cast : (After.bHeavyWindup ? EAction::Heavy : EAction::Basic), ESide::Count, After.Time);
			}
			if (After.bBlocking && !Before.bBlocking)
			{
				Push(EAction::Block, ESide::Count, After.Time);
			}
			if (After.bAirborne && !Before.bAirborne)
			{
				Push(EAction::Jump, ESide::Count, After.Time);
			}
			if (After.SecondsSinceSwitch + 1e-6 < Before.SecondsSinceSwitch - (After.Time - Before.Time) && After.SecondsSinceSwitch < 0.5)
			{
				Push(EAction::Switch, ESide::Count, After.Time);
			}
		}

		void FHabits::NoteTelegraph(double StartedAt, double HitAt)
		{
			Telegraphs.push_back({StartedAt, HitAt});
			if (Telegraphs.size() > 16)
			{
				Telegraphs.erase(Telegraphs.begin());
			}
		}

		void FHabits::Push(EAction Action, ESide Side, double Time)
		{
			History.push_back({Action, Side, Time, Time - LastActionAt > 2.0});
			LastActionAt = Time;
			const std::size_t Window = static_cast<std::size_t>(std::max(3.0, AiTuning().HabitWindow));
			while (History.size() > Window)
			{
				History.erase(History.begin());
			}
		}

		void FHabits::Reset()
		{
			History.clear();
			Telegraphs.clear();
			EarlyDodges = 0;
			TimedDodges = 0;
			LastActionAt = -100.0;
		}

		int FHabits::Count(EAction Action) const
		{
			int Total = 0;
			for (const FEntry& Entry : History)
			{
				Total += Entry.Action == Action ? 1 : 0;
			}
			return Total;
		}

		ESide FHabits::FavouriteDodge(double& Confidence) const
		{
			int Sides[static_cast<int>(ESide::Count)] = {};
			int Total = 0;
			for (const FEntry& Entry : History)
			{
				if (Entry.Action == EAction::Dodge && Entry.Side != ESide::Count)
				{
					++Sides[static_cast<int>(Entry.Side)];
					++Total;
				}
			}
			Confidence = 0.0;
			if (Total == 0)
			{
				return ESide::Count;
			}
			int Best = 0;
			for (int Index = 1; Index < static_cast<int>(ESide::Count); ++Index)
			{
				Best = Sides[Index] > Sides[Best] ? Index : Best;
			}
			Confidence = static_cast<double>(Sides[Best]) / static_cast<double>(Total) * std::min(1.0, static_cast<double>(Total) / 4.0);
			return static_cast<ESide>(Best);
		}

		EAction FHabits::FavouriteOpener(double& Confidence) const
		{
			int Actions[static_cast<int>(EAction::Count)] = {};
			int Total = 0;
			for (const FEntry& Entry : History)
			{
				if (Entry.bOpener)
				{
					++Actions[static_cast<int>(Entry.Action)];
					++Total;
				}
			}
			Confidence = 0.0;
			if (Total == 0)
			{
				return EAction::Count;
			}
			int Best = 0;
			for (int Index = 1; Index < static_cast<int>(EAction::Count); ++Index)
			{
				Best = Actions[Index] > Actions[Best] ? Index : Best;
			}
			Confidence = static_cast<double>(Actions[Best]) / static_cast<double>(Total) * std::min(1.0, static_cast<double>(Total) / 3.0);
			return static_cast<EAction>(Best);
		}

		double FHabits::SpamLevel() const
		{
			int InRuns = 0;
			int Run = 0;
			for (const FEntry& Entry : History)
			{
				if (Entry.Action == EAction::Basic)
				{
					++Run;
					InRuns += Run == 3 ? 3 : (Run > 3 ? 1 : 0);
				}
				else
				{
					Run = 0;
				}
			}
			return BrainImpl::Clamp01(static_cast<double>(InRuns) / std::max(6.0, static_cast<double>(History.size())));
		}

		double FHabits::EarlyDodgeRate() const
		{
			const int Total = EarlyDodges + TimedDodges;
			return Total < 3 ? 0.0 : static_cast<double>(EarlyDodges) / static_cast<double>(Total);
		}

		// ------------------------------------------------------------------ crowd
		void FCrowd::Update(double Now, const FPlayerView& Commander, const std::vector<FCrowdAgent>& Agents, double Adaptation, double Aggression)
		{
			const FAiTuning& T = AiTuning();
			// Forget the dead.
			for (auto It = Memory.begin(); It != Memory.end();)
			{
				const bool bAlive = std::any_of(Agents.begin(), Agents.end(), [&It](const FCrowdAgent& Agent) { return Agent.Id == It->first; });
				if (bAlive)
				{
					++It;
				}
				else
				{
					Slots.erase(It->first);
					It = Memory.erase(It);
				}
			}

			int MeleeCount = 0;
			for (const FCrowdAgent& Agent : Agents)
			{
				MeleeCount += BrainImpl::IsRangedRole(Agent.Role) ? 0 : 1;
				FMemory& Mem = Memory[Agent.Id];
				if (Agent.bSees)
				{
					Mem.LastSeen = Now;
				}
			}
			const int Cap = std::min(MeleeCount, MaxAttackers(MeleeCount, Adaptation) + static_cast<int>(std::lround(std::max(0.0, T.ExtraAttackers))));

			// Tokens: committed angels keep theirs; an idle holder gives it back after a while so pressure rotates.
			int Held = 0;
			for (const FCrowdAgent& Agent : Agents)
			{
				FMemory& Mem = Memory[Agent.Id];
				if (BrainImpl::IsRangedRole(Agent.Role))
				{
					Mem.bToken = false;
					continue;
				}
				if (BrainImpl::IsBossRole(Agent.Role))
				{
					Mem.bToken = true;   // the boss always presses; it does not take a grunt's turn
					continue;
				}
				if (Mem.bToken && !Agent.bCommitted && Now - Mem.TokenSince > 3.0)
				{
					Mem.bToken = false;
					Mem.bBorrowed = false;
					Mem.FreeAt = Now + 0.5 * T.TokenTurnSeconds;
				}
				if (Mem.bToken && !Mem.bBorrowed)
				{
					++Held;
				}
			}

			const FV2 Face = Normal(Commander.Facing);
			const double ConeCos = std::cos(T.FlankFrontCone * BrainImpl::Pi / 180.0);
			struct FCandidate
			{
				int Id;
				double Score;
			};
			std::vector<FCandidate> Candidates;
			for (const FCrowdAgent& Agent : Agents)
			{
				const FMemory& Mem = Memory[Agent.Id];
				if (BrainImpl::IsRangedRole(Agent.Role) || BrainImpl::IsBossRole(Agent.Role) || Mem.bToken || Now < Mem.FreeAt || !Agent.bSees)
				{
					continue;
				}
				const FV2 Offset = Agent.Pos - Commander.Pos;
				const double Dist = Length(Offset);
				const bool bOutsideFront = Dot(Normal(Offset), Face) < ConeCos;
				const double Waited = std::min(6.0, Now - Mem.LastAttackAt);
				Candidates.push_back({Agent.Id, -Dist / 400.0 + Waited / 4.0 + (bOutsideFront ? 0.8 * (0.5 + Adaptation) : 0.0)});
			}
			std::sort(Candidates.begin(), Candidates.end(), [](const FCandidate& A, const FCandidate& B)
				{ return A.Score != B.Score ? A.Score > B.Score : A.Id < B.Id; });
			for (const FCandidate& Candidate : Candidates)
			{
				if (Held >= Cap)
				{
					break;
				}
				FMemory& Mem = Memory[Candidate.Id];
				Mem.bToken = true;
				Mem.bBorrowed = false;
				Mem.TokenSince = Now;
				++Held;
			}

			// Hold points: waiting melee angels spread on a ring; once adapted they avoid the commander's front.
			struct FWaiter
			{
				int Id;
				double U;   // angle around the commander from its facing, 0..2pi
				double Reach;
			};
			std::vector<FWaiter> Waiters;
			const double Cone = Adaptation >= 0.3 ? T.FlankFrontCone * BrainImpl::Pi / 180.0 : 0.0;
			for (const FCrowdAgent& Agent : Agents)
			{
				if (BrainImpl::IsRangedRole(Agent.Role) || Memory[Agent.Id].bToken)
				{
					continue;
				}
				double U = BrainImpl::AngleBetween(Face.X == 0.0 && Face.Y == 0.0 ? FV2{1.0, 0.0} : Face, Agent.Pos - Commander.Pos);
				U = U < 0.0 ? U + 2.0 * BrainImpl::Pi : U;
				U = std::clamp(U, Cone, 2.0 * BrainImpl::Pi - Cone);
				Waiters.push_back({Agent.Id, U, Agent.Reach});
			}
			std::sort(Waiters.begin(), Waiters.end(), [](const FWaiter& A, const FWaiter& B) { return A.U != B.U ? A.U < B.U : A.Id < B.Id; });
			const double Span = 2.0 * BrainImpl::Pi - 2.0 * Cone;
			const FV2 Base = Face.X == 0.0 && Face.Y == 0.0 ? FV2{1.0, 0.0} : Face;
			for (std::size_t Index = 0; Index < Waiters.size(); ++Index)
			{
				const FWaiter& Waiter = Waiters[Index];
				double U = Waiter.U;
				if (Waiters.size() > 1 || Cone > 0.0)
				{
					// Evenly spaced in the allowed arc, keeping the angels' order (nobody crosses through the commander).
					const double Even = Cone + (static_cast<double>(Index) + 0.5) * Span / static_cast<double>(Waiters.size());
					U = Waiters.size() > 1 ? 0.5 * (Waiter.U + Even) : Waiter.U;
				}
				const double Radius = std::max(Waiter.Reach + T.HoldExtra, Commander.ThreatReach + T.OutrangedMargin);
				FCrowdSlot& Slot = Slots[Waiter.Id];
				Slot.bToken = false;
				Slot.bMayShoot = false;
				Slot.HoldPoint = Commander.Pos + Rotate(Base, U) * Radius;
				const double Relative = std::fabs(U > BrainImpl::Pi ? U - 2.0 * BrainImpl::Pi : U);
				Slot.bFlanker = Relative > std::max(Cone, BrainImpl::Pi / 3.0);
			}
			for (const FCrowdAgent& Agent : Agents)
			{
				if (BrainImpl::IsRangedRole(Agent.Role) || !Memory[Agent.Id].bToken)
				{
					continue;
				}
				FCrowdSlot& Slot = Slots[Agent.Id];
				Slot.bToken = true;
				Slot.bMayShoot = false;
				Slot.HoldPoint = Commander.Pos;
				Slot.bFlanker = Dot(Normal(Agent.Pos - Commander.Pos), Face) < ConeCos;
			}

			// Ranged turns: at most MaxShooters at once, starts staggered, one new shooter per frame.
			Shooting = 0;
			for (const FCrowdAgent& Agent : Agents)
			{
				Shooting += BrainImpl::IsRangedRole(Agent.Role) && Agent.bCommitted ? 1 : 0;
			}
			int Best = -1;
			double BestWait = -1.0;
			const bool bTurnOpen = Shooting < static_cast<int>(std::lround(T.MaxShooters)) && Now - LastShotAt >= T.ShotStagger;
			for (const FCrowdAgent& Agent : Agents)
			{
				if (!BrainImpl::IsRangedRole(Agent.Role))
				{
					continue;
				}
				FCrowdSlot& Slot = Slots[Agent.Id];
				Slot.bToken = false;
				Slot.bMayShoot = false;
				Slot.HoldPoint = Agent.Pos;
				Slot.bFlanker = false;
				const double Waited = Now - Memory[Agent.Id].LastAttackAt;
				if (bTurnOpen && Agent.bSees && !Agent.bCommitted && Waited > BestWait)
				{
					BestWait = Waited;
					Best = Agent.Id;
				}
			}
			if (Best >= 0)
			{
				Slots[Best].bMayShoot = true;
			}
			(void)Aggression;
		}

		const FCrowdSlot& FCrowd::Slot(int Id) const
		{
			const auto It = Slots.find(Id);
			return It != Slots.end() ? It->second : Empty;
		}

		void FCrowd::Release(int Id, double Now, double Aggression)
		{
			FMemory& Mem = Memory[Id];
			Mem.bToken = false;
			Mem.bBorrowed = false;
			Mem.LastAttackAt = Now;
			Mem.FreeAt = Now + AiTuning().TokenTurnSeconds / std::max(0.3, Aggression);
			Slots[Id].bToken = false;
		}

		bool FCrowd::BorrowToken(int Id, double Now)
		{
			FMemory& Mem = Memory[Id];
			if (Mem.bToken)
			{
				return true;
			}
			for (const auto& Entry : Memory)
			{
				if (Entry.second.bBorrowed && Entry.second.bToken)
				{
					return false;   // one punish over the cap at a time
				}
			}
			Mem.bToken = true;
			Mem.bBorrowed = true;
			Mem.TokenSince = Now;
			Slots[Id].bToken = true;
			return true;
		}

		void FCrowd::NoteShot(int Id, double Now)
		{
			LastShotAt = Now;
			Memory[Id].LastAttackAt = Now;
			Slots[Id].bMayShoot = false;
		}

		void FCrowd::Reset()
		{
			Memory.clear();
			Slots.clear();
			LastShotAt = -100.0;
			Shooting = 0;
		}

		int FCrowd::TokensHeld() const
		{
			int Total = 0;
			for (const auto& Entry : Memory)
			{
				Total += Entry.second.bToken ? 1 : 0;
			}
			return Total;
		}

		// ------------------------------------------------------------------ decisions
		const char* BehaviorName(EBehavior Behavior)
		{
			switch (Behavior)
			{
			case EBehavior::Spacing: return "Spacing";
			case EBehavior::Punish: return "Punish";
			case EBehavior::Defense: return "Defense";
			case EBehavior::Feint: return "Feint";
			case EBehavior::Adapt: return "Adapt";
			case EBehavior::Coordination: return "Coordination";
			case EBehavior::SelfPreservation: return "SelfPreservation";
			case EBehavior::Kit: return "Kit";
			default: return "?";
			}
		}

		const char* IntentName(EIntent Intent)
		{
			switch (Intent)
			{
			case EIntent::Hold: return "Hold";
			case EIntent::Approach: return "Approach";
			case EIntent::Strafe: return "Strafe";
			case EIntent::Retreat: return "Retreat";
			case EIntent::Reposition: return "Reposition";
			case EIntent::Sidestep: return "Sidestep";
			case EIntent::Backstep: return "Backstep";
			case EIntent::Guard: return "Guard";
			case EIntent::Attack: return "Attack";
			default: return "?";
			}
		}

		void FAction::Tag(EBehavior Behavior)
		{
			if (Has(Behavior))
			{
				return;
			}
			for (EBehavior& Slot : Tags)
			{
				if (Slot == EBehavior::Count)
				{
					Slot = Behavior;
					return;
				}
			}
		}

		bool FAction::Has(EBehavior Behavior) const
		{
			for (EBehavior Slot : Tags)
			{
				if (Slot == Behavior)
				{
					return true;
				}
			}
			return false;
		}

		double FRandom::Next()
		{
			// xorshift32
			uint32_t X = State == 0u ? 0x9E3779B9u : State;
			X ^= X << 13;
			X ^= X >> 17;
			X ^= X << 5;
			State = X;
			return static_cast<double>(X) / 4294967296.0;
		}

		double MoveCooldown(EMove Move)
		{
			switch (Move)
			{
			case EMove::Feint: return 4.0;
			case EMove::Sweep: return 5.0;
			case EMove::Unblockable: return 5.0;
			case EMove::Blink: return 6.0;
			case EMove::DashStrike: return 6.0;
			case EMove::AntiAir: return 2.5;
			case EMove::Nova: return 7.0;
			case EMove::Volley: return 5.0;
			case EMove::Charge: return 7.0;
			case EMove::Lunge: return 4.5;
			case EMove::Guard: return 3.0;
			default: return 0.0;
			}
		}

		FAction Think(const FThink& In, const FMind& Mind, const FHabits& Habits, FCrowd& Crowd, FBrainState& State)
		{
			using namespace BrainImpl;
			const FAiTuning& T = AiTuning();
			const FKnobs& K = In.Knobs;
			const FPlayerView& V = In.View;
			const bool bBoss = IsBossRole(In.Role);
			const bool bRanged = IsRangedRole(In.Role);
			const FCrowdSlot& Slot = Crowd.Slot(In.Id);
			const double Habit = HabitWeight(K);
			FAction Out;
			Out.Goal = In.Pos;

			if (In.Now >= State.NextStrafeFlip)
			{
				State.StrafeSign = State.Random.Next() < 0.5 ? -1.0 : 1.0;
				State.NextStrafeFlip = In.Now + 1.5 + 2.0 * State.Random.Next();
			}
			Out.StrafeSign = State.StrafeSign;

			// ---------------------------------------------------------- perception and memory
			if (In.bSees)
			{
				State.LastSeenAt = In.Now;
				State.LastKnown = V.Pos;
			}
			else
			{
				if (In.Now - State.LastSeenAt > K.MemorySeconds)
				{
					Out.Intent = EIntent::Hold;   // lost the commander: look around where it stands
					return Finish(In, State, Out);
				}
				Out.Goal = State.LastKnown;
				if (bRanged)
				{
					Out.Intent = EIntent::Reposition;   // walk around the cover to get a line of sight
					Out.Tag(EBehavior::Coordination);
				}
				else
				{
					Out.Intent = EIntent::Approach;     // hunt the last known position
					Out.Speed = 0.9 + 0.3 * K.Aggression;
					Out.Tag(EBehavior::Spacing);
				}
				return Finish(In, State, Out);
			}

			const FV2 ToCommander = V.Pos - In.Pos;
			const double Dist = Length(ToCommander);
			const FV2 Toward = Length(ToCommander) > 1e-6 ? Normal(ToCommander) : Normal(In.Facing);
			const bool bAimedAtMe = Dot(Normal(V.Facing), Toward * -1.0) > std::cos(60.0 * Pi / 180.0);
			const bool bCautious = In.StrainFraction >= T.StrainCaution || In.HealthFraction < K.RetreatHealth;

			// ---------------------------------------------------------- defence (one roll per commander action)
			if (V.bAttacking && bAimedAtMe && Dist <= V.ThreatReach + 80.0 && V.ActionStartedAt != State.HandledThreat)
			{
				State.HandledThreat = V.ActionStartedAt;
				const double Skill = 0.75 + 0.25 * Mind.Adaptation();
				const double Roll = State.Random.Next();
				if ((V.bHeavyWindup || V.bLongCast) && In.Now >= State.DodgeReadyAt)
				{
					const double Chance = std::min(0.85, K.DodgeChance * Skill * (In.Role == ERole::Brute ? 0.5 : 1.0) * (bCautious ? 1.25 : 1.0));
					if (Roll < Chance)
					{
						State.DodgeReadyAt = In.Now + K.DodgeCooldown;
						Out.Tag(EBehavior::Defense);
						if (bRanged || In.Role == ERole::Brute)
						{
							Out.Intent = EIntent::Backstep;
							Out.Goal = Toward * -1.0;
						}
						else
						{
							// Sidestep away from the allies' side (keeps the group from stacking in the swing).
							const FV2 Side = Perp(Toward);
							const double Sign = In.NearbyAllies > 0 ? (Dot(In.AllyCentre - In.Pos, Side) > 0.0 ? -1.0 : 1.0) : State.StrafeSign;
							Out.Intent = EIntent::Sidestep;
							Out.Goal = Side * Sign;
						}
						Out.Speed = 1.0;   // the Unreal layer moves SidestepDistance along Goal
						return Finish(In, State, Out);
					}
				}
				else if (!bRanged && !V.bHeavyWindup && In.Now >= State.MoveReadyAt[static_cast<int>(EMove::Guard)])
				{
					// Light swings: guard, more readily against a commander who spams basics.
					const double Spam = Habits.SpamLevel() * Habit;
					const double Chance = std::min(0.85, K.BlockChance * (0.5 + Mind.Expertise(ECounter::Guard)) * Skill + 0.4 * Spam);
					if (Roll < Chance)
					{
						State.MoveReadyAt[static_cast<int>(EMove::Guard)] = In.Now + MoveCooldown(EMove::Guard);
						Out.Intent = EIntent::Guard;
						Out.Move = EMove::Guard;
						Out.Tag(EBehavior::Defense);
						if (Spam > 0.2)
						{
							Out.Tag(EBehavior::Adapt);
						}
						return Finish(In, State, Out);
					}
				}
			}

			// ---------------------------------------------------------- self-preservation
			if (State.RetreatUntil > In.Now)
			{
				Out.Intent = EIntent::Retreat;
				Out.Goal = State.RetreatGoal;
				Out.Speed = 1.25;
				Out.Tag(EBehavior::SelfPreservation);
				return Finish(In, State, Out);
			}
			const bool bOpening = (V.bRecovery && V.bWhiffed) || V.bLongCast || In.bHealing;
			if (bCautious && !bBoss && In.Now >= State.RegroupReadyAt && !bOpening)
			{
				State.RegroupReadyAt = In.Now + T.RegroupCooldown;
				State.RetreatUntil = In.Now + T.RegroupSeconds;
				const double Away = std::max(In.Reach + T.HoldExtra, V.ThreatReach + T.OutrangedMargin) + 300.0;
				State.RetreatGoal = In.NearbyAllies > 0 ? In.AllyCentre + Normal(In.AllyCentre - V.Pos) * 150.0 : V.Pos + Toward * -Away;
				if (Length(State.RetreatGoal - V.Pos) < Away * 0.6)
				{
					State.RetreatGoal = V.Pos + Toward * -Away;   // the group is on top of the commander: back off instead
				}
				Out.Intent = EIntent::Retreat;
				Out.Goal = State.RetreatGoal;
				Out.Speed = 1.25;
				Out.Tag(EBehavior::SelfPreservation);
				return Finish(In, State, Out);
			}

			const bool bReady = In.Now >= State.AttackReadyAt;
			const bool bFacingIt = Dot(Normal(In.Facing), Toward) > std::cos(100.0 * Pi / 180.0);
			const double Interval = In.Interval * K.AttackIntervalMult * (bCautious ? 1.3 : 1.0);

			auto Commit = [&](FAction& Action, EMove Move, double Windup)
			{
				Action.Intent = EIntent::Attack;
				Action.Move = Move;
				Action.Windup = std::max(K.MinTelegraph, Windup);
				Action.Tag(EBehavior::Kit);
				State.MoveReadyAt[static_cast<int>(Move)] = In.Now + MoveCooldown(Move);
				Remember(State, Move);
				State.AttackReadyAt = In.Now + Interval;
				if (bRanged)
				{
					Crowd.NoteShot(In.Id, In.Now);
					if (In.NearbyAllies > 0)
					{
						Action.Tag(EBehavior::Coordination);
					}
				}
				else if (Slot.bFlanker || (bBoss && In.NearbyAllies > 0 && Crowd.TokensHeld() > 1))
				{
					Action.Tag(EBehavior::Coordination);   // from the side / back, or pressing together with its escorts
				}
			};
			auto AimFor = [&](FAction& Action)
			{
				// Lean toward the commander's favourite dodge side (the game adds the lead from the movement the angel sees).
				Action.AimOffset = {};
				double Confidence = 0.0;
				const ESide Side = Habits.FavouriteDodge(Confidence);
				if ((Side == ESide::Left || Side == ESide::Right) && Confidence * Habit > 0.25)
				{
					Action.AimOffset = SideVector(Side, V.Facing) * (T.DodgeSideOffset * std::min(1.0, Confidence));
					Action.Tag(EBehavior::Adapt);
				}
			};

			// ---------------------------------------------------------- punish openings
			const bool bNewOpening = bOpening && (In.bHealing ? V.Time - State.HandledHealAt > 1.0 : V.ActionStartedAt != State.HandledOpening);
			if (bNewOpening)
			{
				if (In.bHealing)
				{
					State.HandledHealAt = V.Time;
				}
				else
				{
					State.HandledOpening = V.ActionStartedAt;
				}
				if (State.Random.Next() < K.PunishChance * (0.7 + 0.3 * Mind.Adaptation()))
				{
					State.PunishUntil = In.Now + 1.0;
				}
			}
			if (State.PunishUntil > In.Now && bOpening && In.Now >= State.AttackReadyAt - 0.5 * Interval)
			{
				if (bRanged)
				{
					if (Slot.bMayShoot || V.bLongCast)
					{
						Commit(Out, EMove::Strike, In.BaseWindup * 0.85);
						AimFor(Out);
						Out.Tag(EBehavior::Punish);
						State.PunishUntil = -1.0;
						return Finish(In, State, Out);
					}
				}
				else if (Dist <= In.Reach + 40.0 && bFacingIt)
				{
					if (Slot.bToken || Crowd.BorrowToken(In.Id, In.Now))
					{
						Out.bBorrowToken = !Slot.bToken;
						Commit(Out, EMove::Strike, In.BaseWindup * 0.8);
						Out.DamageMult = 1.2;
						Out.Tag(EBehavior::Punish);
						State.PunishUntil = -1.0;
						return Finish(In, State, Out);
					}
				}
				else if (Dist <= In.Reach + T.PunishReach)
				{
					const EMove Closer = In.Role == ERole::Assassin ? EMove::Lunge : (bBoss || In.Role == ERole::Melee ? EMove::DashStrike : EMove::Count);
					if (Closer != EMove::Count && In.Now >= State.MoveReadyAt[static_cast<int>(Closer)] && Dist > In.Reach + 150.0 && bFacingIt &&
						(Slot.bToken || Crowd.BorrowToken(In.Id, In.Now)))
					{
						Out.bBorrowToken = !Slot.bToken;
						Commit(Out, Closer, In.BaseWindup * WindupMult(Closer));
						Out.Tag(EBehavior::Punish);
						State.PunishUntil = -1.0;
						return Finish(In, State, Out);
					}
					Out.Intent = EIntent::Approach;
					Out.Goal = In.CommanderNow;
					Out.Speed = 1.5;
					Out.Tag(EBehavior::Punish);
					return Finish(In, State, Out);
				}
			}

			// ---------------------------------------------------------- offence: the best move of the kit by score
			const bool bMayAttack = bReady && bFacingIt && (bRanged ? Slot.bMayShoot && !In.bLineBlockedByAlly : (Slot.bToken || bBoss));
			if (bMayAttack)
			{
				double DodgeConfidence = 0.0;
				const ESide DodgeSide = Habits.FavouriteDodge(DodgeConfidence);
				const bool bSideDodger = (DodgeSide == ESide::Left || DodgeSide == ESide::Right) && DodgeConfidence * Habit > 0.3;
				const double EarlyRate = Habits.EarlyDodgeRate() * Habit;
				double OpenerConfidence = 0.0;
				const EAction Opener = Habits.FavouriteOpener(OpenerConfidence);
				const bool bBlocker = V.bBlocking || (Habits.Count(EAction::Block) >= 3 && Habit > 0.4);
				const double Kiting = Clamp01(Mind.Rate(ETrait::Kiting) / 10.0);
				const bool bJustSwitched = V.SecondsSinceSwitch < 2.0;
				const double Reach = In.Reach + 40.0;

				EMove Best = EMove::Count;
				double BestScore = -1e9;
				bool bBestHabit = false;
				for (int Index = 0; Index < MoveCount; ++Index)
				{
					const EMove Move = static_cast<EMove>(Index);
					if (!InKit(In.Role, Move) || In.Now < State.MoveReadyAt[Index])
					{
						continue;
					}
					double Near = 0.0;
					double Far = 0.0;
					const bool bGap = GapBand(Move, Near, Far);
					const bool bFromRange = IsRangedMove(In.Role, Move);
					if (bGap)
					{
						if (Dist < In.Reach + Near || Dist > In.Reach + Far)
						{
							continue;
						}
					}
					else if (Move == EMove::Nova)
					{
						if (Dist > 600.0)
						{
							continue;
						}
					}
					else if (bFromRange)
					{
						if (Dist > K.SightRange || (Move == EMove::AntiAir && !V.bAirborne))
						{
							continue;
						}
					}
					else if (Dist > Reach)
					{
						continue;
					}

					double Score = 0.0;
					bool bHabit = false;
					switch (Move)
					{
					case EMove::Strike:
						Score = 1.0;
						if (bJustSwitched)
						{
							Score += 0.3 + 1.2 * Mind.Expertise(ECounter::Ambush);
						}
						break;
					case EMove::Combo:
						Score = 0.8 + 0.6 * K.Aggression + (V.bRecovery ? 0.5 : 0.0) - (bCautious ? 0.6 : 0.0);
						break;
					case EMove::Feint:
						Score = 0.4 + K.FeintChance + Mind.Expertise(ECounter::Feint) + 1.5 * EarlyRate - (bCautious ? 0.6 : 0.0);
						if ((Opener == EAction::Dodge || Opener == EAction::Basic) && OpenerConfidence * Habit > 0.4)
						{
							Score += 0.5;   // bait the opener they like
							bHabit = true;
						}
						bHabit = bHabit || EarlyRate > 0.2;
						break;
					case EMove::Sweep:
						Score = 0.5 + (bSideDodger ? 1.2 * DodgeConfidence : 0.0);
						bHabit = bSideDodger;
						break;
					case EMove::Unblockable:
						Score = 0.3 + Mind.Expertise(ECounter::Unblockable) + (bBlocker ? 1.5 : 0.0);
						bHabit = bBlocker && !V.bBlocking;
						break;
					case EMove::Blink:
						Score = 0.6 + Mind.Expertise(ECounter::Blink) + 0.8 * Kiting;
						bHabit = Kiting > 0.3;
						break;
					case EMove::DashStrike:
						Score = 0.5 + Mind.Expertise(ECounter::MirrorDash) + 0.4 * K.Aggression;
						break;
					case EMove::AntiAir:
						Score = 2.0 + Mind.Expertise(ECounter::AntiAir);
						break;
					case EMove::Nova:
						Score = (bRanged ? 1.4 : 0.6) + Mind.Expertise(ECounter::MirrorNova) + (In.bPhaseTwo ? 0.5 : 0.0) + (bCautious ? 0.5 : 0.0);
						break;
					case EMove::Volley:
						Score = 0.6 + Mind.Expertise(ECounter::MirrorVolley) + (Dist > 600.0 ? 0.4 : 0.0) + (bCautious ? 0.4 : 0.0);
						break;
					case EMove::Charge:
						Score = 0.9 + 0.5 * K.Aggression;
						break;
					case EMove::Lunge:
						Score = 1.0 + 0.5 * K.Aggression;
						break;
					default:
						break;
					}
					// Variety: a move used lately scores lower; a tiny jitter breaks ties (never the reason for a pick).
					Score -= Recently(State, Move) ? 0.6 : 0.0;
					Score += 0.25 * State.Random.Next();
					if (Score > BestScore)
					{
						BestScore = Score;
						Best = Move;
						bBestHabit = bHabit;
					}
				}

				if (Best != EMove::Count)
				{
					const double PhaseMult = In.bPhaseTwo ? 0.9 : 1.0;
					const double AmbushMult = Best == EMove::Strike && bJustSwitched && Mind.Knows(ECounter::Ambush) ? 0.75 : 1.0;
					Commit(Out, Best, In.BaseWindup * WindupMult(Best) * PhaseMult * AmbushMult);
					if (bBestHabit)
					{
						Out.Tag(EBehavior::Adapt);
					}
					switch (Best)
					{
					case EMove::Feint:
						Out.FeintDelay = std::max(K.MinTelegraph, T.FeintExtraDelay * (1.0 + 0.5 * EarlyRate));
						Out.Tag(EBehavior::Feint);
						break;
					case EMove::Unblockable:
						Out.bUnblockable = true;
						break;
					case EMove::Charge:
						Out.bSuperArmor = true;
						Out.DamageMult = 1.3;
						break;
					case EMove::Nova:
						Out.bSuperArmor = bBoss || In.Role == ERole::Brute;
						break;
					case EMove::Sweep:
						Out.DamageMult = 1.1;
						break;
					case EMove::Combo:
					{
						const int Extra = (K.Aggression > 0.6 ? 1 : 0) + (bBoss ? 1 : 0) + (In.bPhaseTwo ? 1 : 0);
						Out.FollowUps = std::clamp(1 + Extra, 1, 3);
						for (int Step = 0; Step < Out.FollowUps; ++Step)
						{
							Out.Enders[Step] = EEnder::Quick;
						}
						// The ender reads the commander: early dodgers get a held strike, side dodgers a sweep, blockers an unblockable.
						EEnder Ender = EEnder::Quick;
						if (EarlyRate > 0.25)
						{
							Ender = EEnder::Delayed;
							Out.Tag(EBehavior::Adapt);
						}
						else if (bSideDodger)
						{
							Ender = EEnder::Sweep;
							Out.Tag(EBehavior::Adapt);
						}
						else if (bBlocker)
						{
							Ender = EEnder::Unblockable;
							Out.Tag(EBehavior::Adapt);
						}
						else
						{
							const double Roll = State.Random.Next();
							Ender = Roll < K.FeintChance ? EEnder::Delayed : (Roll < K.FeintChance + 0.3 ? EEnder::Sweep : EEnder::Quick);
						}
						Out.Enders[Out.FollowUps - 1] = Ender;
						if (Ender == EEnder::Delayed)
						{
							Out.Tag(EBehavior::Feint);
						}
						break;
					}
					default:
						break;
					}
					if (IsRangedMove(In.Role, Best))
					{
						AimFor(Out);
						// Ranged mixup: a held shot (the aim holds, then flashes again) catches dodges thrown on the first flash.
						if (bRanged && (Best == EMove::Strike || Best == EMove::Volley) && State.Random.Next() < K.FeintChance * 0.5 + EarlyRate)
						{
							Out.FeintDelay = std::max(K.MinTelegraph, T.FeintExtraDelay);
							Out.Tag(EBehavior::Feint);
							if (EarlyRate > 0.2)
							{
								Out.Tag(EBehavior::Adapt);
							}
						}
					}
					if (bBoss && bCautious && (Best == EMove::Nova || Best == EMove::Volley))
					{
						Out.Tag(EBehavior::SelfPreservation);   // close to BREAK / low: the boss fights from range
					}
					return Finish(In, State, Out);
				}
			}

			// ---------------------------------------------------------- spacing
			Out.Tag(EBehavior::Spacing);
			if (bRanged)
			{
				const double Min = In.KeepMin > 0.0 ? In.KeepMin : 700.0;
				const double Max = In.KeepMax > Min ? In.KeepMax : Min + 400.0;
				if (In.bLineBlockedByAlly)
				{
					Out.Intent = EIntent::Reposition;   // an ally is in the line: step sideways for a clear shot
					Out.Goal = V.Pos;
					Out.Tag(EBehavior::Coordination);
				}
				else if (Dist < Min || (Dist < Min + 150.0 && Dot(V.Vel, Toward * -1.0) > 150.0))
				{
					Out.Intent = EIntent::Approach;   // outranged / pressed: back off to the band
					Out.Goal = V.Pos + Toward * -(0.5 * (Min + Max));
					Out.Speed = 1.15;
				}
				else if (Dist > Max)
				{
					Out.Intent = EIntent::Approach;
					Out.Goal = V.Pos + Toward * -(0.5 * (Min + Max));
				}
				else
				{
					Out.Intent = EIntent::Strafe;
					Out.Goal = V.Pos;
					Out.Speed = 0.6;
				}
				return Finish(In, State, Out);
			}

			if (Slot.bToken || bBoss)
			{
				if (Dist > In.Reach)
				{
					Out.Intent = EIntent::Approach;
					Out.Goal = In.CommanderNow;
					// Close in hard on a commander stuck in a recovery.
					Out.Speed = V.bRecovery ? 1.4 : 1.0 + 0.3 * K.Aggression;
				}
				else if (V.bAttacking && Dist < V.ThreatReach && bCautious)
				{
					Out.Intent = EIntent::Approach;   // cautious: step out of the swing
					Out.Goal = V.Pos + Toward * -(V.ThreatReach + T.OutrangedMargin);
					Out.Speed = 1.2;
					Out.Tag(EBehavior::SelfPreservation);
				}
				else
				{
					Out.Intent = EIntent::Strafe;     // in reach, waiting for the interval: circle tight
					Out.Goal = V.Pos;
					Out.Speed = 0.5;
				}
				return Finish(In, State, Out);
			}

			// Waiting its turn: hold the ring point (sides / back when adapted), out of the commander's reach.
			Out.Tag(EBehavior::Coordination);
			FV2 Hold = Slot.HoldPoint;
			if (Length(Hold - V.Pos) < 1.0)
			{
				Hold = V.Pos + Toward * -(In.Reach + T.HoldExtra);
			}
			if (V.bRecovery && Dist < In.Reach + T.PunishReach)
			{
				Hold = V.Pos + Normal(Hold - V.Pos) * (In.Reach + 60.0);   // tighten the ring on a recovering commander
			}
			const bool bOutranged = Dist < V.ThreatReach + T.OutrangedMargin && V.bAttacking;
			if (Length(Hold - In.Pos) > 80.0 || bOutranged)
			{
				Out.Intent = EIntent::Approach;
				Out.Goal = bOutranged ? V.Pos + Toward * -(V.ThreatReach + T.OutrangedMargin + 40.0) : Hold;
				Out.Speed = bOutranged ? 1.2 : 0.9;
			}
			else
			{
				Out.Intent = EIntent::Strafe;
				Out.Goal = V.Pos;
				Out.Speed = 0.4;
			}
			return Finish(In, State, Out);
		}

		// ------------------------------------------------------------------ stats
		FAiStats& AiStats() { return BrainImpl::LiveStats(); }
		void ResetAiStats() { BrainImpl::LiveStats() = FAiStats(); }

		void CountAction(ERole Role, const FAction& Action)
		{
			FAiStats& Stats = AiStats();
			const int Row = BrainImpl::RoleIndex(Role);
			for (EBehavior Tag : Action.Tags)
			{
				if (Tag != EBehavior::Count)
				{
					++Stats.Behaviors[Row][static_cast<int>(Tag)];
				}
			}
			if (Action.Intent == EIntent::Attack)
			{
				++Stats.Moves[Row][static_cast<int>(Action.Move)];
				Stats.Punishes += Action.Has(EBehavior::Punish) ? 1 : 0;
			}
			if ((Action.Intent == EIntent::Sidestep || Action.Intent == EIntent::Backstep) && Action.Has(EBehavior::Defense))
			{
				++Stats.Evasions;
			}
		}
	}
}
