#include "BIBGame/BIBMotionEvents.h"

#include "BIBGame/BIBMotion.h"
#include "BIBGame/BIBEnemy.h"
#include "BIBRules/BIBKitStatus.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "BIBRules/BIBArena.h"
#include "BIBRules/BIBCore.h"
#include "BIBRules/BIBPages.h"

#include "HAL/PlatformTime.h"
#include "Internationalization/Text.h"

namespace
{
	using BIBMotion::FPlay;

	FString Num(int64 Value)
	{
		return FText::AsNumber(Value).ToString();
	}

	FPlay Make(const FString& Piece, const FString& Clip, float Speed = 1.f, const FString& Tag = FString())
	{
		FPlay Play;
		Play.Piece = Piece;
		Play.Clip = Clip;
		Play.Speed = Speed;
		Play.Tag = Tag.IsEmpty() ? Piece : Tag;
		return Play;
	}

	FPlay AtCursor(const FString& Piece, const FString& Clip, float Scale = 0.5f)
	{
		FPlay Play = Make(Piece, Clip, 1.4f, Piece + TEXT(":") + Clip);
		Play.bAnchored = true;
		Play.Anchor = BIBMotion::CursorPosition();
		Play.Scale = Scale;
		return Play;
	}

	// A small cooldown per key so the same piece does not stutter when an event repeats every frame.
	bool Ready(const FString& Key, double Seconds)
	{
		static TMap<FString, double> Last;
		const double Now = FPlatformTime::Seconds();
		double& At = Last.FindOrAdd(Key, -1000.0);
		if (Now - At < Seconds)
		{
			return false;
		}
		At = Now;
		return true;
	}

	int64 Cur(const BIBMotionEvents::FSnapshot& S, BIB::ECurrency C)
	{
		return S.Currency[static_cast<int32>(C)];
	}
}

BIBMotionEvents::FSnapshot BIBMotionEvents::Snap(const BIB::FProfile& Profile, const BIB::Pages::FUiState& State)
{
	FSnapshot S;
	for (int32 Index = 0; Index < static_cast<int32>(BIB::ECurrency::Count) && Index < 16; ++Index)
	{
		S.Currency[Index] = Profile.Get(static_cast<BIB::ECurrency>(Index));
	}
	for (const BIB::FOwnedOperator& Op : Profile.Operators)
	{
		if (Op.bOwned)
		{
			S.LevelSum += Op.Level;
			S.TopLevel = FMath::Max(S.TopLevel, Op.Level);
		}
	}
	S.SummonCount = State.SummonCount;
	for (const BIB::FMail& Mail : Profile.Meta.Mail)
	{
		S.MailClaimed += Mail.bClaimed ? 1 : 0;
	}
	S.Equipment = static_cast<int32>(Profile.Inventory.size());
	S.Tab = State.Tab;
	S.Page = UTF8_TO_TCHAR(State.Page.c_str());
	return S;
}

void BIBMotionEvents::MenuOpened()
{
	BIBMotion::Play(TEXT("tr3"), TEXT("play"), 2.4f);
	FPlay Nav = Make(TEXT("nav-bar"), TEXT("open"), 1.3f);
	Nav.Delay = 0.15f;
	BIBMotion::Play(Nav);
}

void BIBMotionEvents::StationOpened(const FString& Title, const FString& Code, const FString& StationId)
{
	BIBMotion::Play(TEXT("tr2"), TEXT("play"), 2.2f);
	FPlay Header = Make(TEXT("page-header"), TEXT("open"), 1.2f);
	Header.Values.Add(TEXT("title"), Title.ToUpper());
	Header.Values.Add(TEXT("code"), Code.ToUpper());
	Header.Delay = 0.2f;
	Header.Opacity = 0.95f;
	BIBMotion::Play(Header);
	if (StationId == TEXT("factory"))
	{
		FPlay Banners = Make(TEXT("banner-select"), TEXT("open"), 1.5f);
		Banners.Delay = 0.35f;
		Banners.Opacity = 0.9f;
		BIBMotion::Play(Banners);
	}
	else if (StationId == TEXT("taskroom"))
	{
		// The campaign map flourish is played by PageOpened with the real chapter.
	}
}

void BIBMotionEvents::PageOpened(const FString& Title)
{
	if (Ready(TEXT("tr1"), 0.25))
	{
		BIBMotion::Play(TEXT("tr1"), TEXT("play"), 2.6f);
	}
	(void)Title;
}

void BIBMotionEvents::BackPressed()
{
	BIBMotion::Play(AtCursor(TEXT("btn-secondary"), TEXT("back")));
}

void BIBMotionEvents::ClosePressed()
{
	BIBMotion::Play(AtCursor(TEXT("btn-secondary"), TEXT("close")));
}

void BIBMotionEvents::AfterAction(const FSnapshot& Before, const BIB::FProfile& Profile, const BIB::Pages::FUiState& State, const FString& ActionId,
	const FString& Message)
{
	const FSnapshot After = Snap(Profile, State);
	using BIB::ECurrency;
	auto Delta = [&](ECurrency C) { return Cur(After, C) - Cur(Before, C); };

	// ---- summons: ticket consume -> charge -> the best rarity's reveal (and the x10 card backs)
	if (After.SummonCount != Before.SummonCount && !State.SummonStars.empty())
	{
		float At = 0.f;
		const int64 TicketsSpent = -(Delta(ECurrency::RegularTicket) + Delta(ECurrency::SpecialTicket));
		if (TicketsSpent > 0)
		{
			FPlay Ticket = Make(TEXT("ticket-use"), TEXT("use"), 2.f);
			const ECurrency Kind = Delta(ECurrency::SpecialTicket) < 0 ? ECurrency::SpecialTicket : ECurrency::RegularTicket;
			Ticket.Values.Add(TEXT("count"), TEXT("x") + Num(Cur(After, Kind)));
			BIBMotion::Play(Ticket);
			At += 1.1f;
		}
		else
		{
			FPlay Modal = Make(TEXT("modal"), TEXT("confirm"), 3.f);
			Modal.Values.Add(TEXT("question"), FString::Printf(TEXT("SUMMON X%d?"), static_cast<int32>(State.SummonStars.size())));
			Modal.Values.Add(TEXT("owned"), FString::Printf(TEXT("DIAMONDS: %s  >  %s"), *Num(Cur(Before, ECurrency::Diamonds)), *Num(Cur(After, ECurrency::Diamonds))));
			BIBMotion::Play(Modal);
			At += 1.3f;
		}
		FPlay Charge = Make(TEXT("summon-charge"), TEXT("charge"), 1.8f);
		Charge.Delay = At;
		BIBMotion::Play(Charge);
		At += 1.5f;
		int32 Best = 3;
		for (int32 Stars : State.SummonStars)
		{
			Best = FMath::Max(Best, Stars);
		}
		static const TCHAR* const Reveal[] = {TEXT("reveal-rare"), TEXT("reveal-epic"), TEXT("reveal-legendary"), TEXT("reveal-secret")};
		FPlay Show = Make(Reveal[FMath::Clamp(Best - 3, 0, 3)], TEXT("reveal"), 1.2f);
		Show.Delay = At;
		BIBMotion::Play(Show);
		if (State.SummonStars.size() >= 10)
		{
			FPlay Cards = Make(TEXT("summon-x10"), TEXT("cards"), 1.f);
			Cards.Delay = At + 2.f;
			BIBMotion::Play(Cards);
		}
		return;
	}
	// ---- a level up
	if (After.TopLevel > Before.TopLevel || After.LevelSum > Before.LevelSum)
	{
		FPlay Up = Make(TEXT("level-up"), TEXT("up"), 1.3f);
		Up.Values.Add(TEXT("level"), FString::Printf(TEXT("LV %d"), After.TopLevel));
		Up.Values.Add(TEXT("stats"), FString::Printf(TEXT("+%d LEVEL%s"), After.LevelSum - Before.LevelSum, After.LevelSum - Before.LevelSum == 1 ? TEXT("") : TEXT("S")));
		BIBMotion::Play(Up);
		return;
	}
	const bool bGained = Delta(ECurrency::Gold) > 0 || Delta(ECurrency::Bytes) > 0 || Delta(ECurrency::AngelHearts) > 0 || Delta(ECurrency::Diamonds) > 0
		|| Delta(ECurrency::RegularTicket) > 0 || Delta(ECurrency::SpecialTicket) > 0;
	const bool bSpent = Delta(ECurrency::Gold) < 0 || Delta(ECurrency::Bytes) < 0 || Delta(ECurrency::Diamonds) < 0;
	// ---- a purchase (something bought with Gold, Bytes or Diamonds)
	if (bSpent && (After.Equipment > Before.Equipment || Before.Page.Contains(TEXT("shop")) || Before.Tab == BIB::Pages::TabShop))
	{
		FPlay Card = Make(TEXT("shop-card"), TEXT("buy"), 1.4f);
		Card.Values.Add(TEXT("name"), Message.Left(28).ToUpper());
		const int64 Price = FMath::Max3(-Delta(ECurrency::Gold), -Delta(ECurrency::Bytes), -Delta(ECurrency::Diamonds));
		Card.Values.Add(TEXT("price"), Num(Price));
		Card.Values.Add(TEXT("sub"), Delta(ECurrency::Diamonds) < 0 ? TEXT("DIAMONDS") : (Delta(ECurrency::Bytes) < 0 ? TEXT("BYTES") : TEXT("GOLD")));
		BIBMotion::Play(Card);
		return;
	}
	// ---- rewards claimed (quests, mail, events, pass ...)
	if (bGained)
	{
		const bool bQuest = Before.Page == TEXT("quests") || ActionId.Contains(TEXT("quest"));
		const bool bMail = After.MailClaimed > Before.MailClaimed || Before.Page == TEXT("mail");
		if (bQuest)
		{
			FPlay Quest = Make(TEXT("quest-complete"), TEXT("claim"), 1.4f);
			Quest.Values.Add(TEXT("q0"), Message.IsEmpty() ? FString(TEXT("QUEST CLAIMED")) : Message.Left(34).ToUpper());
			BIBMotion::Play(Quest);
		}
		else if (bMail)
		{
			FPlay Mail = Make(TEXT("mail-inbox"), TEXT("open"), 1.4f);
			int32 Shown = 0;
			for (int32 Index = static_cast<int32>(Profile.Meta.Mail.size()) - 1; Index >= 0 && Shown < 3; --Index)
			{
				const BIB::FMail& Item = Profile.Meta.Mail[static_cast<size_t>(Index)];
				Mail.Values.Add(FString::Printf(TEXT("kind%d"), Shown), Item.From.empty() ? FString(TEXT("SYSTEM")) : FString(UTF8_TO_TCHAR(Item.From.c_str())).ToUpper());
				Mail.Values.Add(FString::Printf(TEXT("subject%d"), Shown), FString(UTF8_TO_TCHAR(Item.Title.c_str())).Left(30).ToUpper());
				++Shown;
			}
			int32 Unclaimed = 0;
			for (const BIB::FMail& Item : Profile.Meta.Mail)
			{
				Unclaimed += Item.bClaimed ? 0 : 1;
			}
			Mail.Values.Add(TEXT("badge"), FString::FromInt(Unclaimed));
			BIBMotion::Play(Mail);
		}
		FPlay Rewards = Make(TEXT("reward-popup"), TEXT("rewards"), 1.5f);
		const ECurrency Order[5] = {ECurrency::Gold, ECurrency::Bytes, ECurrency::AngelHearts, ECurrency::RegularTicket, ECurrency::Diamonds};
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const int64 D = Delta(Order[Index]) + (Index == 3 ? Delta(ECurrency::SpecialTicket) : 0);
			Rewards.Values.Add(FString::Printf(TEXT("r%d"), Index), D > 0 ? TEXT("x") + Num(D) : FString(TEXT("-")));
		}
		Rewards.Delay = (bQuest || bMail) ? 1.4f : 0.f;
		BIBMotion::Play(Rewards);
		FPlay Gain = Make(TEXT("resource-gain"), TEXT("gain"), 1.3f);
		Gain.Delay = Rewards.Delay + 1.8f;
		Gain.Values.Add(TEXT("v0"), Num(Cur(After, ECurrency::Gold)));
		Gain.Values.Add(TEXT("v1"), Num(Cur(After, ECurrency::Bytes)));
		Gain.Values.Add(TEXT("v2"), Num(Cur(After, ECurrency::AngelHearts)));
		Gain.Values.Add(TEXT("g0"), Delta(ECurrency::Gold) > 0 ? TEXT("+") + Num(Delta(ECurrency::Gold)) + TEXT(" GOLD") : FString());
		Gain.Values.Add(TEXT("g1"), Delta(ECurrency::Bytes) > 0 ? TEXT("+") + Num(Delta(ECurrency::Bytes)) + TEXT(" BYTES") : FString());
		Gain.Values.Add(TEXT("g2"), Delta(ECurrency::AngelHearts) > 0 ? TEXT("+") + Num(Delta(ECurrency::AngelHearts)) + TEXT(" ANGEL HEARTS") : FString());
		BIBMotion::Play(Gain);
		return;
	}
	// ---- navigation inside the panel: a new page gets its piece (or the quick transition)
	if (After.Tab != Before.Tab || After.Page != Before.Page)
	{
		if (After.Page == TEXT("settings"))
		{
			BIBMotion::Play(TEXT("settings-controls"), TEXT("open"), 1.3f);
		}
		else if (After.Page == TEXT("pass"))
		{
			BIBMotion::Play(TEXT("season-pass"), TEXT("open"), 1.2f);
		}
		else if (After.Page == TEXT("mail"))
		{
			FPlay Mail = Make(TEXT("mail-inbox"), TEXT("open"), 1.4f);
			int32 Shown = 0;
			int32 Unclaimed = 0;
			for (int32 Index = static_cast<int32>(Profile.Meta.Mail.size()) - 1; Index >= 0; --Index)
			{
				const BIB::FMail& Item = Profile.Meta.Mail[static_cast<size_t>(Index)];
				Unclaimed += Item.bClaimed ? 0 : 1;
				if (Shown < 3)
				{
					Mail.Values.Add(FString::Printf(TEXT("kind%d"), Shown), Item.From.empty() ? FString(TEXT("SYSTEM")) : FString(UTF8_TO_TCHAR(Item.From.c_str())).ToUpper());
					Mail.Values.Add(FString::Printf(TEXT("subject%d"), Shown), FString(UTF8_TO_TCHAR(Item.Title.c_str())).Left(30).ToUpper());
					++Shown;
				}
			}
			Mail.Values.Add(TEXT("badge"), FString::FromInt(Unclaimed));
			BIBMotion::Play(Mail);
		}
		else if (After.Tab == BIB::Pages::TabCampaign && Before.Tab != BIB::Pages::TabCampaign)
		{
			FPlay Map = Make(TEXT("campaign-map"), TEXT("open"), 1.3f);
			const int32 Cleared = Profile.StagesCleared;
			const int32 Chapter = FMath::Clamp(Cleared / 8 + 1, 1, 13);
			Map.Values.Add(TEXT("title"), FString::Printf(TEXT("CHAPTER %02d"), Chapter));
			Map.Values.Add(TEXT("sub"), FString::Printf(TEXT("STAGE %d-1 TO %d-8 // MINI-BOSS EVERY STAGE // BOSS AT THE END"), Chapter, Chapter));
			Map.Values.Add(TEXT("current"), FString::Printf(TEXT("CURRENT // STAGE %d-%d"), Chapter, FMath::Clamp(Cleared % 8 + 1, 1, 8)));
			BIBMotion::Play(Map);
		}
		else if (After.Tab == BIB::Pages::TabTeam && Before.Tab != BIB::Pages::TabTeam)
		{
			FPlay Power = Make(TEXT("battle-power"), TEXT("power"), 1.4f);
			Power.Values.Add(TEXT("power"), Num(BIB::TeamBattlePower(Profile)));
			Power.Values.Add(TEXT("delta"), TEXT("SQUAD BATTLE POWER"));
			BIBMotion::Play(Power);
		}
		else
		{
			PageOpened(FString());
		}
		return;
	}
	// ---- anything else that was clicked: the primary button press under the cursor
	if (ActionId != TEXT("back"))
	{
		BIBMotion::Play(AtCursor(TEXT("btn-primary"), TEXT("press"), 0.35f));
	}
}

void BIBMotionEvents::Travel(bool bOn, const FString& Label)
{
	if (!bOn)
	{
		BIBMotion::Stop(TEXT("loading"));
		return;
	}
	if (BIBMotion::IsPlaying(TEXT("loading")))
	{
		return;
	}
	FPlay Loading = Make(TEXT("loading"), TEXT("loop"), 1.f, TEXT("loading"));
	Loading.bLoop = true;
	Loading.Values.Add(TEXT("label"), TEXT("LOADING // ") + Label.ToUpper());
	Loading.Values.Add(TEXT("tip"), TEXT("TIP: SWITCH OPERATORS ON A BROKEN ANGEL FOR A RELAY STRIKE"));
	BIBMotion::Play(Loading);
}

void BIBMotionEvents::ArkToasts(const TArray<FString>& Lines)
{
	if (Lines.Num() == 0)
	{
		return;
	}
	static const TCHAR* const Kinds[] = {TEXT("NOTICE"), TEXT("ARK"), TEXT("SYSTEM")};
	FPlay Toast = Make(TEXT("notice-toast"), TEXT("toasts"), 1.f);
	for (int32 Index = 0; Index < FMath::Min(3, Lines.Num()); ++Index)
	{
		Toast.Values.Add(FString::Printf(TEXT("kind%d"), Index), Kinds[Index]);
		Toast.Values.Add(FString::Printf(TEXT("text%d"), Index), Lines[Lines.Num() - 1 - Index].Left(34).ToUpper());
	}
	BIBMotion::Play(Toast);
}

void BIBMotionEvents::StageStarted(bool bBossLevel, bool bTouch, const FString& Title)
{
	BIBMotion::Stop(TEXT("loading"));
	BIBMotion::Play(bBossLevel ? TEXT("tr5") : TEXT("tr4"), TEXT("play"), bBossLevel ? 1.3f : 1.6f);
	FPlay Skills = Make(bTouch ? TEXT("mobile-actions") : TEXT("skill-buttons"), TEXT("intro"), 1.f);
	Skills.Delay = 0.9f;
	Skills.Opacity = 0.85f;
	BIBMotion::Play(Skills);
	if (bTouch)
	{
		TouchTutorial();
	}
	(void)Title;
}

void BIBMotionEvents::Victory(const FString& Rewards)
{
	FPlay Win = Make(TEXT("victory"), TEXT("win"), 1.f);
	Win.Values.Add(TEXT("rewards"), Rewards.ToUpper());
	BIBMotion::Play(Win);
}

void BIBMotionEvents::Defeat()
{
	BIBMotion::Play(TEXT("defeat"), TEXT("lose"), 1.f);
}

void BIBMotionEvents::BossIntro(const FString& Chapter)
{
	FPlay Boss = Make(TEXT("boss-intro"), TEXT("intro"), 1.2f);
	Boss.Values.Add(TEXT("chapter"), Chapter.ToUpper());
	BIBMotion::Play(Boss);
}

void BIBMotionEvents::MiniBoss()
{
	BIBMotion::Play(TEXT("miniboss-warning"), TEXT("alert"), 1.2f);
}

void BIBMotionEvents::ComboMilestone(int32 Hits)
{
	FPlay Combo = Make(TEXT("combo-counter"), TEXT("burst"), 1.f);
	Combo.Values.Add(TEXT("count"), FString::FromInt(Hits));
	BIBMotion::Play(Combo);
}

void BIBMotionEvents::BigHit(const FVector2D& Screen, float Value)
{
	if (!Ready(TEXT("crit"), 0.5))
	{
		return;
	}
	FPlay Crit = Make(TEXT("damage-numbers"), TEXT("crit"), 1.2f);
	Crit.bAnchored = true;
	Crit.Anchor = Screen;
	Crit.Scale = 0.55f;
	Crit.Values.Add(TEXT("value"), Num(FMath::RoundToInt64(Value)));
	BIBMotion::Play(Crit);
}

void BIBMotionEvents::UltimateReady()
{
	FPlay Ult = Make(TEXT("ultimate-ready"), TEXT("ready"), 1.3f);
	Ult.Opacity = 0.9f;
	BIBMotion::Play(Ult);
}

void BIBMotionEvents::Switched(bool bTouch)
{
	FPlay Squad = Make(TEXT("squad-switch"), TEXT("switch"), 1.2f);
	Squad.Opacity = 0.85f;
	BIBMotion::Play(Squad);
	(void)bTouch;
}

void BIBMotionEvents::Defense(int32 Kind)
{
	static const TCHAR* const Clips[] = {TEXT("block"), TEXT("parry"), TEXT("dodge")};
	if (!Ready(TEXT("defense"), Kind == 0 ? 2.0 : 0.4))
	{
		return;
	}
	FPlay Prompt = Make(TEXT("defense-prompts"), Clips[FMath::Clamp(Kind, 0, 2)], 1.5f, TEXT("defense"));
	BIBMotion::Play(Prompt);
}

void BIBMotionEvents::LowHealth()
{
	if (Ready(TEXT("lowhp"), 8.0))
	{
		FPlay Low = Make(TEXT("hp-bar"), TEXT("low"), 1.f);
		Low.Opacity = 0.8f;
		BIBMotion::Play(Low);
	}
}

void BIBMotionEvents::TargetLock(const FVector2D& Screen, float DistanceM)
{
	if (!Ready(TEXT("lock"), 1.2))
	{
		return;
	}
	FPlay Lock = Make(TEXT("target-lock"), TEXT("lock"), 1.6f);
	Lock.bAnchored = true;
	Lock.Anchor = Screen;
	Lock.Scale = 0.45f;
	Lock.Values.Add(TEXT("dist"), FString::Printf(TEXT("DIST %.1f M"), DistanceM));
	BIBMotion::Play(Lock);
}

void BIBMotionEvents::TowerFloor(int32 Floor)
{
	FPlay Tower = Make(TEXT("ark-floor"), TEXT("floor"), 1.3f);
	Tower.Values.Add(TEXT("floor"), FString::FromInt(Floor));
	BIBMotion::Play(Tower);
}

void BIBMotionEvents::BossRushRound(int32 Round)
{
	FPlay Rush = Make(TEXT("boss-rush"), TEXT("round"), 1.f);
	Rush.Values.Add(TEXT("round"), FString::FromInt(Round));
	BIBMotion::Play(Rush);
}

void BIBMotionEvents::ArcadeStart(int32 Kind)
{
	BIBMotion::Play(Kind == 1 ? TEXT("rhythm-lanes") : TEXT("night-shift"), TEXT("intro"), 1.2f);
}

void BIBMotionEvents::ArenaRank(const FString& Rank, const FString& Score)
{
	FPlay Arena = Make(TEXT("arena-rank"), TEXT("rank"), 1.6f);
	Arena.Values.Add(TEXT("rank"), Rank.ToUpper());
	Arena.Values.Add(TEXT("score"), Score.ToUpper());
	BIBMotion::Play(Arena);
}

void BIBMotionEvents::TouchPress(const FString& Clip, const FVector2D& Screen)
{
	FPlay Press = Make(TEXT("mobile-switch-dodge"), Clip, 1.4f, TEXT("touch:") + Clip);
	Press.bAnchored = true;
	Press.Anchor = Screen;
	Press.Scale = 0.5f;
	BIBMotion::Play(Press);
}

void BIBMotionEvents::TouchTutorial()
{
	static bool bShown = false;
	if (!bShown)
	{
		bShown = true;
		FPlay Tutorial = Make(TEXT("mobile-hud-layout"), TEXT("tutorial"), 1.f);
		Tutorial.Delay = 1.6f;
		BIBMotion::Play(Tutorial);
	}
}

// ============================================================================================ Set 2 world VFX
namespace
{
	FString WhoKey(const FString& Operator)
	{
		static const TMap<FString, FString> Keys = {
			{TEXT("Luna"), TEXT("luna")}, {TEXT("Judge"), TEXT("judge")}, {TEXT("Narrator"), TEXT("narr")}, {TEXT("Alchemist"), TEXT("alch")},
			{TEXT("Arita"), TEXT("arita")}, {TEXT("Adapt"), TEXT("adapt")}, {TEXT("Fujita"), TEXT("fujita")}, {TEXT("Ishii"), TEXT("ishii")},
			{TEXT("Myrcia"), TEXT("myrcia")}, {TEXT("Shiraishi"), TEXT("shira")}, {TEXT("Kuu"), TEXT("kuu")}, {TEXT("Plack"), TEXT("plack")},
			{TEXT("Blood"), TEXT("blood")}, {TEXT("Gambler"), TEXT("gambler")}, {TEXT("Stargazer"), TEXT("star")}, {TEXT("Weather"), TEXT("weather")},
			{TEXT("Death"), TEXT("death")}, {TEXT("Jester"), TEXT("jester")}, {TEXT("Media"), TEXT("media")}, {TEXT("Timekeeper"), TEXT("timek")}};
		const FString* Key = Keys.Find(Operator);
		return Key ? *Key : Operator.ToLower();
	}

	bool PlayOn(const FString& Piece, AActor* On, bool bFlip, const FString& Tag, bool bLoop, const FString& Clip = FString())
	{
		if (Piece.IsEmpty() || !On)
		{
			return false;
		}
		BIBMotion::FPlay Play;
		Play.Piece = Piece;
		Play.Clip = Clip.IsEmpty() ? (bLoop ? TEXT("loop") : TEXT("play")) : Clip;
		Play.Follow = On;
		Play.bFlip = bFlip;
		Play.bLoop = bLoop;
		Play.Tag = Tag;
		Play.Opacity = 0.95f;
		if (!BIBMotion::Play(Play) && bLoop)
		{
			// a one-shot piece used as a held overlay (guard stance, verdict markers): loop its play clip
			Play.Clip = TEXT("play");
			return BIBMotion::Play(Play);
		}
		return true;
	}

	TMap<uint32, TSet<FString>> GEnemyLoops;
}

bool BIBMotionEvents::FacesLeft(AActor* Caster)
{
	APlayerController* PC = Caster ? UGameplayStatics::GetPlayerController(Caster, 0) : nullptr;
	FVector2D A, B;
	if (!PC || !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Caster->GetActorLocation(), A, false)
		|| !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Caster->GetActorLocation() + Caster->GetActorForwardVector() * 150.0, B, false))
	{
		return false;
	}
	return B.X < A.X;
}

bool BIBMotionEvents::Ability(AActor* Caster, const FString& Operator, const FString& SlotKey, AActor* Target)
{
	const FString Piece = BIBMotion::AbilityPiece(WhoKey(Operator), SlotKey);
	if (Piece.IsEmpty())
	{
		return false;
	}
	// Abilities that land on the angel (the piece is drawn around the enemy) follow the target when there is one.
	const bool bOnEnemy = BIBMotion::PieceMode(Piece) == TEXT("enemy") && Target;
	return PlayOn(Piece, bOnEnemy ? Target : Caster, !bOnEnemy && FacesLeft(Caster), TEXT("ab:") + Piece, false);
}

bool BIBMotionEvents::Shared(const FString& Slug, AActor* On, bool bFlip, const FString& Tag, bool bLoop)
{
	return PlayOn(BIBMotion::FindFx(Slug), On, bFlip, Tag, bLoop);
}

void BIBMotionEvents::SyncEnemy(ABIBEnemy* Enemy, const FString& ActiveOperator, bool bJudgeVerdicts)
{
	if (!Enemy || !Enemy->IsAlive())
	{
		return;
	}
	using BIB::Kits::EStatus;
	TSet<FString> Want;
	const BIB::Kits::FStatusBook& Book = Enemy->GetStatusBook();
	const FString Op = WhoKey(ActiveOperator);
	auto Has = [&Book](EStatus S) { return Book.Has(S); };
	if (Has(EStatus::Burn)) { Want.Add(TEXT("burn")); }
	if (Has(EStatus::Bleed)) { Want.Add(TEXT("bleed")); }
	if (Has(EStatus::Poison)) { Want.Add(TEXT("poison")); }
	if (Has(EStatus::Shock)) { Want.Add(TEXT("shock")); }
	if (Has(EStatus::Wet)) { Want.Add(TEXT("wet")); }
	if (Has(EStatus::Root)) { Want.Add(TEXT("root")); }
	if (Has(EStatus::Weaken)) { Want.Add(TEXT("weaken")); }
	if (Has(EStatus::Vulnerable)) { Want.Add(TEXT("vulnerable")); }
	if (Has(EStatus::Doom)) { Want.Add(TEXT("doom")); }
	if (Has(EStatus::Infection)) { Want.Add(TEXT("infection")); }
	if (Has(EStatus::Mutated)) { Want.Add(TEXT("mutation")); }
	if (Has(EStatus::Immune)) { Want.Add(TEXT("immunity")); }
	if (Has(EStatus::Illusion)) { Want.Add(TEXT("illusion")); }
	if (Has(EStatus::Fear)) { Want.Add(Op == TEXT("jester") ? TEXT("taunt") : TEXT("fear")); }
	// the time and cold looks belong to the operators that cause them
	if (Has(EStatus::Freeze)) { Want.Add(Op == TEXT("timek") ? TEXT("time-frozen") : TEXT("freeze")); }
	if (Has(EStatus::Slow)) { Want.Add(Op == TEXT("timek") ? TEXT("time-slowed") : ((Op == TEXT("weather") || Op == TEXT("ishii")) ? TEXT("chill") : TEXT("slow"))); }
	if (Has(EStatus::Mark)) { Want.Add(Op == TEXT("star") ? TEXT("seeded") : (Op == TEXT("arita") ? TEXT("soul-link") : (Op == TEXT("jester") ? TEXT("taunt") : TEXT("vulnerable")))); }
	if (Has(EStatus::Stun) || Enemy->IsDazeStunned()) { Want.Add(TEXT("stunned-halo")); }
	if (Enemy->IsGuarding()) { Want.Add(TEXT("guard-counter-stance")); }
	// a boss winding up an attack does not flinch: the poise shimmer says so
	if (Enemy->IsBoss() && Enemy->IsCommitted()) { Want.Add(TEXT("poise-shield-shimmer")); }
	if (bJudgeVerdicts && Enemy->JudgeState != EBIBJudgeState::Innocent) { Want.Add(TEXT("verdict-markers")); }

	const uint32 Id = Enemy->GetUniqueID();
	TSet<FString>& Have = GEnemyLoops.FindOrAdd(Id);
	const FString Prefix = FString::Printf(TEXT("en:%u:"), Id);
	for (const FString& Slug : Have)
	{
		if (!Want.Contains(Slug))
		{
			BIBMotion::Stop(Prefix + Slug);
		}
	}
	for (const FString& Slug : Want)
	{
		if (!Have.Contains(Slug))
		{
			PlayOn(BIBMotion::FindFx(Slug), Enemy, false, Prefix + Slug, true);
		}
	}
	Have = MoveTemp(Want);
}

void BIBMotionEvents::EnemyDied(ABIBEnemy* Enemy)
{
	if (!Enemy)
	{
		return;
	}
	const uint32 Id = Enemy->GetUniqueID();
	BIBMotion::StopPrefix(FString::Printf(TEXT("en:%u:"), Id));
	GEnemyLoops.Remove(Id);
}

void BIBMotionEvents::Dialogue(bool bOpen, const FString& Speaker, const FString& Line)
{
	if (!bOpen)
	{
		BIBMotion::Stop(TEXT("dialogue"));
		BIBMotion::Stop(TEXT("dialogue:hold"));
		return;
	}
	static FString LastLine;
	const bool bNew = !BIBMotion::IsPlaying(TEXT("dialogue:hold")) && !BIBMotion::IsPlaying(TEXT("dialogue"));
	if (!bNew && Line == LastLine)
	{
		return;
	}
	LastLine = Line;
	BIBMotion::FPlay Hold = Make(TEXT("story-dialogue"), TEXT("hold"), 1.f, TEXT("dialogue:hold"));
	Hold.bLoop = true;
	Hold.Values.Add(TEXT("name"), Speaker.ToUpper());
	Hold.Values.Add(TEXT("line"), Line);
	if (bNew)
	{
		BIBMotion::FPlay Open = Make(TEXT("story-dialogue"), TEXT("open"), 1.f, TEXT("dialogue"));
		Open.Values = Hold.Values;
		BIBMotion::Play(Open);
		Hold.Delay = 0.55f;
	}
	BIBMotion::Play(Hold);
}

void BIBMotionEvents::Section(bool bClear, int32 Index, int32 Count, const FString& Objective)
{
	BIBMotion::FPlay Tracker = Make(TEXT("objective-tracker"), bClear ? TEXT("clear") : TEXT("show"), 1.f, TEXT("objective"));
	Tracker.Values.Add(TEXT("count"), FString::Printf(TEXT("%d / %d"), Index, Count));
	Tracker.Values.Add(TEXT("objective"), Objective.ToUpper());
	BIBMotion::Play(Tracker);
}

void BIBMotionEvents::Gauges(const FString& Operator)
{
	static const TMap<FString, FString> Sheet = {
		{TEXT("blood"), TEXT("gauges-a")}, {TEXT("death"), TEXT("gauges-a")}, {TEXT("kuu"), TEXT("gauges-a")}, {TEXT("media"), TEXT("gauges-a")},
		{TEXT("shira"), TEXT("gauges-b")}, {TEXT("plack"), TEXT("gauges-b")}, {TEXT("star"), TEXT("gauges-b")}, {TEXT("narr"), TEXT("gauges-b")}};
	if (const FString* Piece = Sheet.Find(WhoKey(Operator)))
	{
		BIBMotion::FPlay Gauge = Make(*Piece, TEXT("show"), 1.4f, TEXT("gauges"));
		Gauge.Opacity = 0.9f;
		BIBMotion::Play(Gauge);
	}
}

void BIBMotionEvents::JudgeMode(int32 Mode, bool bLocked)
{
	static const TCHAR* const Clips[] = {TEXT("witness"), TEXT("lawyer"), TEXT("judge")};
	static const TCHAR* const Names[] = {TEXT("WITNESS"), TEXT("LAWYER"), TEXT("JUDGE")};
	const int32 Index = FMath::Clamp(Mode, 0, 2);
	BIBMotion::FPlay Wheel = Make(TEXT("judge-mode-wheel"), bLocked ? TEXT("locked") : Clips[Index], 1.3f, TEXT("judgewheel"));
	Wheel.Values.Add(TEXT("mode"), FString::Printf(TEXT("MODE: %s%s"), Names[Index], bLocked ? TEXT(" (LOCKED)") : TEXT("")));
	Wheel.Opacity = 0.9f;
	BIBMotion::Play(Wheel);
}

void BIBMotionEvents::Exchange(bool bOpen)
{
	if (bOpen)
	{
		BIBMotion::Play(TEXT("global-exchange-panel"), TEXT("open"), 1.4f, TEXT("exchange"));
	}
}
