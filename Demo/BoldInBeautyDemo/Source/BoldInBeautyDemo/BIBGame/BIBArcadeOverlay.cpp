#include "BIBGame/BIBArcadeOverlay.h"
#include "BIBGame/BIBFonts.h"
#include "BIBGame/BIBAnimeStyle.h"

#include "BIBGame/BIBGameInstance.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBRules/BIBArcade.h"
#include "BIBRules/BIBArena.h"
#include "BIBRules/BIBCore.h"
#include "BIBRules/BIBMetaOps.h"
#include "BIBRules/BIBMiniGames.h"
#include "BIBRules/BIBRally.h"
#include "BIBRules/BIBSynth.h"
#include "BIBRules/BIBUcnBots.h"
#include "BIBRules/BIBUltimateNight.h"

#include "Components/AudioComponent.h"
#include "Containers/Ticker.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Sound/SoundWaveProcedural.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogBIBArcade, Log, All);

namespace BIBOverlayInternal
{
	// Boldinbeauty brand colours (website/css/style.css): void, panel, text, halo cream, Bold pink.
	const FLinearColor Void = FLinearColor::FromSRGBColor(FColor(0x0c, 0x0a, 0x14));
	const FLinearColor Panel = FLinearColor::FromSRGBColor(FColor(0x1b, 0x17, 0x29));
	const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(0xec, 0xe8, 0xf4));
	const FLinearColor Cream = FLinearColor::FromSRGBColor(FColor(0xff, 0xf1, 0xd6));
	const FLinearColor Pink = FLinearColor::FromSRGBColor(FColor(0xff, 0x2e, 0x74));
	const FLinearColor Cyan(0.20f, 0.80f, 0.95f, 1.f);
	const FLinearColor Gold(1.f, 0.78f, 0.15f, 1.f);
	const FLinearColor Purple(0.62f, 0.38f, 1.f, 1.f);
	const FLinearColor Dim(1.f, 1.f, 1.f, 0.35f);
	const FLinearColor Danger(0.95f, 0.18f, 0.22f, 1.f);

	FString ToF(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	std::string ToStd(const FString& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text));
	}

	FString KeyOf(const BIB::FSettings& Settings, int32 Index)
	{
		return ToF(Settings.KeyBinds[Index].empty() ? std::string(BIB::DefaultKeyName(Index)) : Settings.KeyBinds[Index]);
	}

	FString KeyLabel(const BIB::FSettings& Settings, int32 Index)
	{
		return ToF(BIB::KeyDisplayName(ToStd(KeyOf(Settings, Index))));
	}

	// Everything the games draw goes through this small painter (boxes, text, images, lines in local units).
	struct FPainter
	{
		const FGeometry& Geometry;
		FSlateWindowElementList& Out;
		int32 Layer;
		float TextScale;

		const FSlateBrush* White() const { return FCoreStyle::Get().GetBrush("GenericWhiteBox"); }

		void Box(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Colour) const
		{
			FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), White(), ESlateDrawEffect::None, Colour);
		}

		void Frame(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Colour, float Thickness = 2.f) const
		{
			Box(Pos, FVector2D(Size.X, Thickness), Colour);
			Box(Pos + FVector2D(0.f, Size.Y - Thickness), FVector2D(Size.X, Thickness), Colour);
			Box(Pos, FVector2D(Thickness, Size.Y), Colour);
			Box(Pos + FVector2D(Size.X - Thickness, 0.f), FVector2D(Thickness, Size.Y), Colour);
		}

		FSlateFontInfo Font(int32 Size, bool bBold = true) const
		{
			return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::Max(6, FMath::RoundToInt(Size * TextScale)));
		}

		FVector2D Measure(const FString& Text, int32 Size, bool bBold = true) const
		{
			return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font(Size, bBold));
		}

		// Align: 0 left, 1 centre, 2 right (around Pos.X).
		void Text(const FString& Value, const FVector2D& Pos, int32 Size, const FLinearColor& Colour, int32 Align = 0, bool bBold = true) const
		{
			const FVector2D Extent = Measure(Value, Size, bBold);
			FVector2D At = Pos;
			if (Align == 1)
			{
				At.X -= Extent.X * 0.5f;
			}
			else if (Align == 2)
			{
				At.X -= Extent.X;
			}
			FSlateDrawElement::MakeText(Out, Layer + 1, Geometry.ToPaintGeometry(Extent, FSlateLayoutTransform(At)), Value, Font(Size, bBold), ESlateDrawEffect::None, Colour);
		}

		void Image(const FSlateBrush* Brush, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Tint = FLinearColor::White) const
		{
			if (Brush)
			{
				FSlateDrawElement::MakeBox(Out, Layer + 1, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), Brush, ESlateDrawEffect::None, Tint);
			}
		}

		void Line(const FVector2D& From, const FVector2D& To, const FLinearColor& Colour, float Thickness = 2.f) const
		{
			TArray<FVector2D> Points;
			Points.Add(From);
			Points.Add(To);
			FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Colour, true, Thickness);
		}
	};

	// Plays a synthesised buffer once. The component and the wave stay referenced until the sound has finished.
	struct FVoice
	{
		TStrongObjectPtr<UAudioComponent> Component;
		TStrongObjectPtr<USoundWaveProcedural> Wave;
		double EndTime = 0.0;
	};
}

using namespace BIBOverlayInternal;
namespace BUcn = BIB::Arcade::Ucn;

// ================================================================================================ the game overlay
// The overlays name keys by FKey name ("Escape", "Enter", "Left" ...). A gamepad press is mapped onto the matching name so the
// overlays can be left with B (back / quit), A (confirm) and the d-pad (move) without their own gamepad tables. Rebound keys
// are unaffected: a controller button that nothing maps keeps its own name.
static FString OverlayKeyName(const FKey& Key)
{
	if (Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right) return TEXT("Escape");
	if (Key == EKeys::Gamepad_FaceButton_Bottom) return TEXT("Enter");
	if (Key == EKeys::Gamepad_DPad_Left) return TEXT("Left");
	if (Key == EKeys::Gamepad_DPad_Right) return TEXT("Right");
	if (Key == EKeys::Gamepad_DPad_Up) return TEXT("Up");
	if (Key == EKeys::Gamepad_DPad_Down) return TEXT("Down");
	return Key.GetFName().ToString();
}

class SBIBGameOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBIBGameOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UWorld* InWorld, UBIBGameInstance* InInstance, EBIBOverlayKind InKind, int32 InA, int32 InB)
	{
		World = InWorld;
		Instance = InInstance;
		Kind = InKind;
		A = InA;
		B = InB;
		const BIB::FSettings& Settings = Instance.IsValid() ? Instance->GetProfile().Meta.Settings : BIB::FSettings();
		TextScale = FMath::Clamp(Settings.HudScale, 75, 125) / 100.f;
		bCaptions = Settings.bSoundCaptions;
		bReduceMotion = Settings.bReduceMotion;
		SfxVolume = FMath::Clamp(Settings.SfxVolume, 0, 100) / 100.f;
		// owner 2026-10-09: music 60% quieter at baseline (the slider still scales it)
		MusicVolume = FMath::Clamp(Settings.MusicVolume, 0, 100) / 100.f * 0.4f;
		OffsetSeconds = FMath::Clamp(Settings.RhythmOffsetMs, -200, 200) / 1000.0;
		switch (Kind)
		{
		case EBIBOverlayKind::NightShift:
			Night.Start(static_cast<BIB::Arcade::EDifficulty>(FMath::Clamp(A, 0, BIB::Arcade::DifficultyCount - 1)), static_cast<uint32_t>(FDateTime::UtcNow().GetTicks() & 0xFFFFFFFF));
			Night.SelectCamera(0);
			break;
		case EBIBOverlayKind::UltimateNight:
			Ucn.Start(UcnMode(), static_cast<uint32_t>(FDateTime::UtcNow().GetTicks() & 0xFFFFFFFF));
			Ucn.SelectCamera(0);
			break;
		case EBIBOverlayKind::Rhythm:
			Song.Start(A, static_cast<BIB::Arcade::EDifficulty>(FMath::Clamp(B, 0, BIB::Arcade::DifficultyCount - 1)));
			// The calibration offset: positive = the sound arrives late, so the notes wait for it.
			SimDelay = FMath::Max(0.0, OffsetSeconds);
			MusicDelay = FMath::Max(0.0, -OffsetSeconds);
			break;
		case EBIBOverlayKind::HaloFall:
			Halo.Start(MiniDifficulty(), static_cast<uint32_t>(FDateTime::UtcNow().GetTicks() & 0xFFFFFFFF));
			break;
		case EBIBOverlayKind::NeonMemory:
			Memory.Start(MiniDifficulty(), static_cast<uint32_t>(FDateTime::UtcNow().GetTicks() & 0xFFFFFFFF));
			break;
		case EBIBOverlayKind::FirewallBreach:
			Breach.Start(MiniDifficulty(), static_cast<uint32_t>(FDateTime::UtcNow().GetTicks() & 0xFFFFFFFF));
			break;
		case EBIBOverlayKind::ArkRally:
		{
			const uint32_t Seed = static_cast<uint32_t>(FDateTime::UtcNow().GetTicks() & 0xFFFFFFFF);
			std::string Error;
			const BIB::Arcade::ERallyVehicle Vehicle = Instance.IsValid() && Instance->GetProfile().Meta.ArcadeRecords.RallyVehicle == 1
				? BIB::Arcade::ERallyVehicle::Buggy : BIB::Arcade::ERallyVehicle::DirtBike;
			if (B != 1 || !Instance.IsValid() || !Rally.StartCustom(Instance->GetProfile().Meta.ArcadeRecords.RallyTrack, Seed, Error, Vehicle))
			{
				Rally.Start(MiniDifficulty(), Seed, Vehicle);
			}
			RallyCamera = FVector2D(Rally.Chassis().Pos.X, Rally.Chassis().Pos.Y);
			break;
		}
		case EBIBOverlayKind::ArenaBattle:
			if (Instance.IsValid())
			{
				Battle = Instance->UiState.LastBattle;
				Attack = Instance->UiState.LastAttack;
				Defence = Instance->UiState.LastDefence;
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const BIB::Arena::FLineup& Lineup = Side == 0 ? Attack : Defence;
					for (const BIB::Arena::FFighter& Fighter : Lineup.Team)
					{
						MaxHp[Side].Add(static_cast<float>(Fighter.Hp));
						Hp[Side].Add(static_cast<float>(Fighter.Hp));
					}
				}
			}
			break;
		}
		ChildSlot[SNullWidget::NullWidget];
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SBIBGameOverlay::Advance));
	}

	virtual ~SBIBGameOverlay() override
	{
		StopAllSounds();
	}

	TFunction<void(const FString&)> OnDone;
	TFunction<void()> RequestClose;

	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override
	{
		if (KeyEvent.IsRepeat())
		{
			return FReply::Handled();
		}
		HandleKey(OverlayKeyName(KeyEvent.GetKey()), true);
		return FReply::Handled();
	}

	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override
	{
		HandleKey(OverlayKeyName(KeyEvent.GetKey()), false);
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		for (int32 Index = Hits.Num() - 1; Index >= 0; --Index)
		{
			if (Hits[Index].Key.ContainsPoint(Local))
			{
				Click(Hits[Index].Value, true);
				return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
			}
		}
		if (bShowResult)
		{
			Finish();
		}
		return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (Kind == EBIBOverlayKind::ArkRally)
		{
			for (bool& bHeld : bRallyTouch)
			{
				bHeld = false;
			}
			ApplyRallyInput();
		}
		if (Kind == EBIBOverlayKind::NightShift)
		{
			Night.SetLight(BIB::Arcade::ESide::Left, bKeyLight[0]);
			Night.SetLight(BIB::Arcade::ESide::Right, bKeyLight[1]);
		}
		return FReply::Handled();
	}

	// ------------------------------------------------------------------------------------------ test hooks
	FString DebugState() const
	{
		switch (Kind)
		{
		case EBIBOverlayKind::NightShift:
			return FString::Printf(TEXT("night %s t=%.1f hour=%d power=%.1f left=%s%s right=%s%s camera=%s cam=%d over=%d won=%d result=%s"),
				*ToF(BIB::Arcade::DifficultyName(Night.GetDifficulty())), Night.Time(), Night.Hour(), Night.Power(),
				Night.IsDoorClosed(BIB::Arcade::ESide::Left) ? TEXT("closed") : TEXT("open"), Night.IsDoorBroken(BIB::Arcade::ESide::Left) ? TEXT("+broken") : TEXT(""),
				Night.IsDoorClosed(BIB::Arcade::ESide::Right) ? TEXT("closed") : TEXT("open"), Night.IsDoorBroken(BIB::Arcade::ESide::Right) ? TEXT("+broken") : TEXT(""),
				Night.IsCameraUp() ? TEXT("up") : TEXT("down"), Night.SelectedCamera(), Night.IsOver() ? 1 : 0, Night.IsWon() ? 1 : 0, *ResultText);
		case EBIBOverlayKind::Rhythm:
			return FString::Printf(TEXT("rhythm song=%d t=%.2f notes=%d score=%lld combo=%d meter=%.2f over=%d passed=%d result=%s"), Song.SongIndex(), Song.Time(),
				static_cast<int32>(Song.Notes().size()), Song.Score(), Song.Combo(), Song.Meter(), Song.IsOver() ? 1 : 0, Song.IsPassed() ? 1 : 0, *ResultText);
		case EBIBOverlayKind::ArenaBattle:
			return FString::Printf(TEXT("arena t=%.1f events=%d/%d won=%d over=%d"), ReplayTime, NextEvent, static_cast<int32>(Battle.Events.size()),
				Battle.bAttackerWon ? 1 : 0, bShowResult ? 1 : 0);
		case EBIBOverlayKind::HaloFall:
			return FString::Printf(TEXT("halo %s t=%.1f wave=%d cleared=%d angels=%d lives=%d x=%.2f score=%lld over=%d won=%d result=%s"),
				*ToF(BIB::Arcade::DifficultyName(Halo.GetDifficulty())), Halo.Time(), Halo.Wave(), Halo.WavesCleared(), Halo.AliveAngels(), Halo.Lives(), Halo.PlayerX(),
				Halo.Score(), Halo.IsOver() ? 1 : 0, Halo.IsWon() ? 1 : 0, *ResultText);
		case EBIBOverlayKind::NeonMemory:
			return FString::Printf(TEXT("memory %s t=%.1f cards=%d matched=%d/%d moves=%d preview=%d score=%lld over=%d won=%d result=%s"),
				*ToF(BIB::Arcade::DifficultyName(Memory.GetDifficulty())), Memory.Time(), Memory.CardCount(), Memory.Matched(), Memory.Pairs(), Memory.Moves(),
				Memory.InPreview() ? 1 : 0, Memory.Score(), Memory.IsOver() ? 1 : 0, Memory.IsWon() ? 1 : 0, *ResultText);
		case EBIBOverlayKind::UltimateNight:
		{
			int32 OpenDuties = 0;
			for (const BUcn::FDuty& Duty : Ucn.Duties())
			{
				OpenDuties += Duty.bOpen ? 1 : 0;
			}
			return FString::Printf(TEXT("ucn %s t=%.1f hour=%d power=%.1f left=%s right=%s window=%s blood=%d camera=%s cam=%d shocks=%d cams=%d duties=%d ad=%d over=%d won=%d killer=%d result=%s"),
				*ToF(BUcn::ModeName(Ucn.GetMode())), Ucn.Time(), Ucn.Hour(), Ucn.Power(),
				Ucn.IsDoorClosed(BIB::Arcade::ESide::Left) ? TEXT("closed") : TEXT("open"), Ucn.IsDoorClosed(BIB::Arcade::ESide::Right) ? TEXT("closed") : TEXT("open"),
				Ucn.IsWindowClosed() ? TEXT("closed") : TEXT("open"), Ucn.BloodPhaseNow(), Ucn.IsCameraUp() ? TEXT("up") : TEXT("down"), Ucn.SelectedCamera(), Ucn.ShocksTotal(), Ucn.CamerasWorking(), OpenDuties, Ucn.IsAdOpen() ? 1 : 0,
				Ucn.IsOver() ? 1 : 0, Ucn.IsWon() ? 1 : 0, Ucn.Killer(), *ResultText);
		}
		case EBIBOverlayKind::FirewallBreach:
			return FString::Printf(TEXT("breach %s t=%.1f layer=%d/%d integrity=%d cursor=%.2f port=%.2f score=%lld over=%d won=%d result=%s"),
				*ToF(BIB::Arcade::DifficultyName(Breach.GetDifficulty())), Breach.Time(), Breach.Layer(), Breach.Rules().Layers, Breach.Integrity(), Breach.Cursor(),
				Breach.PortCentre(), Breach.Score(), Breach.IsOver() ? 1 : 0, Breach.IsWon() ? 1 : 0, *ResultText);
		case EBIBOverlayKind::ArkRally:
			return FString::Printf(TEXT("rally %s t=%.1f x=%.1f/%.1f speed=%.1f coins=%d crashes=%d place=%d ghosts=%d vehicle=%s nitro=%.2f flips=%d score=%lld over=%d won=%d result=%s"),
				Rally.IsCustom() ? TEXT("Custom") : *ToF(BIB::Arcade::DifficultyName(Rally.GetDifficulty())), Rally.Time(), Rally.Chassis().Pos.X, Rally.Track().FinishX,
				Rally.Speed(), Rally.Coins(), Rally.Crashes(), Rally.Place(), Rally.HasGhosts() ? 1 : 0,
				Rally.Vehicle() == BIB::Arcade::ERallyVehicle::Buggy ? TEXT("buggy") : TEXT("bike"), Rally.Nitro(), Rally.Flips(), Rally.Score(), Rally.IsOver() ? 1 : 0, Rally.IsWon() ? 1 : 0, *ResultText);
		}
		return FString();
	}

	bool DebugCommand(const FString& Command)
	{
		FString Head;
		FString Rest;
		if (!Command.Split(TEXT(":"), &Head, &Rest))
		{
			Head = Command;
		}
		if (Head == TEXT("key") || Head == TEXT("keyup"))
		{
			HandleKey(Rest, Head == TEXT("key"));
			return true;
		}
		if (Head == TEXT("advance"))
		{
			const double Seconds = FCString::Atod(*Rest);
			for (double Done = 0.0; Done < Seconds; Done += 0.05)
			{
				Step(0.05);
			}
			return true;
		}
		if (Head == TEXT("night") && Kind == EBIBOverlayKind::NightShift)
		{
			if (Rest == TEXT("media:left") || Rest == TEXT("media:right"))
			{
				Night.ForceMediaTeleport(Rest.EndsWith(TEXT("left")) ? BIB::Arcade::ESide::Left : BIB::Arcade::ESide::Right);
				return true;
			}
			if (Rest == TEXT("luna:door"))
			{
				Night.ForceRoom(BIB::Arcade::ENightChar::Luna, BIB::Arcade::ERoom::LeftDoor);
				return true;
			}
			if (Rest == TEXT("win"))
			{
				// Keep everyone home until 6 AM (the verify script checks the reward path).
				for (int32 Guard = 0; Guard < 20000 && !Night.IsOver(); ++Guard)
				{
					for (BIB::Arcade::ENightChar Char : {BIB::Arcade::ENightChar::Luna, BIB::Arcade::ENightChar::Plack, BIB::Arcade::ENightChar::Narrator})
					{
						Night.ForceRoom(Char, BIB::Arcade::ERoom::Stage);
					}
					Night.ForceRoom(BIB::Arcade::ENightChar::Kuu, BIB::Arcade::ERoom::Backroom);
					if (Night.IsMediaAtDoor(BIB::Arcade::ESide::Left) || Night.IsMediaAtDoor(BIB::Arcade::ESide::Right))
					{
						Night.SetDoor(Night.IsMediaAtDoor(BIB::Arcade::ESide::Left) ? BIB::Arcade::ESide::Left : BIB::Arcade::ESide::Right, true);
					}
					Step(0.25);
					Night.SetDoor(BIB::Arcade::ESide::Left, false);
					Night.SetDoor(BIB::Arcade::ESide::Right, false);
				}
				return true;
			}
		}
		if (Head == TEXT("ucn") && Kind == EBIBOverlayKind::UltimateNight)
		{
			if (Rest == TEXT("autoplay"))
			{
				// The perfect bot of the beatability suite plays through the real overlay (the verify script checks the reward path).
				UcnBot = MakeUnique<BUcn::FBot>(BUcn::EBotKind::Perfect, 1u, Ucn.Tuning());
				return true;
			}
			if (Rest.StartsWith(TEXT("do:")))
			{
				Click(TEXT("ucn:") + Rest.RightChop(3), true);   // e.g. ucn:do:door:0, ucn:do:cam, ucn:do:sel:4, ucn:do:shock
				return true;
			}
			return false;
		}
		if (Head == TEXT("rhythm") && Rest == TEXT("autoplay") && Kind == EBIBOverlayKind::Rhythm)
		{
			bAutoplay = true;
			return true;
		}
		if (Head == TEXT("halo") && Rest == TEXT("win") && Kind == EBIBOverlayKind::HaloFall)
		{
			// Shoot down every wave (the verify script checks the reward path).
			for (int32 Guard = 0; Guard < 100 && !Halo.IsOver(); ++Guard)
			{
				Halo.ForceClearWave();
				for (int32 Frame = 0; Frame < 150 && !Halo.IsOver(); ++Frame)
				{
					Step(1.0 / 60.0);
					if (Halo.AliveAngels() > 0 && Frame > 2)
					{
						break;   // the next wave has dropped in
					}
				}
			}
			return true;
		}
		if (Head == TEXT("memory") && Rest == TEXT("solve") && Kind == EBIBOverlayKind::NeonMemory)
		{
			Memory.Tick(FMath::Max(0.0, Memory.Rules().Preview - Memory.Time()) + 0.01);
			for (int32 Card = 0; Card < Memory.CardCount() && !Memory.IsOver(); ++Card)
			{
				if (!Memory.IsMatched(Card))
				{
					Memory.Flip(Card);
					Memory.Flip(Memory.PartnerOf(Card));
				}
			}
			return true;
		}
		if (Head == TEXT("breach") && Rest == TEXT("autoplay") && Kind == EBIBOverlayKind::FirewallBreach)
		{
			bAutoplay = true;
			return true;
		}
		if (Head == TEXT("rally") && Kind == EBIBOverlayKind::ArkRally && (Rest == TEXT("autoplay") || Rest == TEXT("respawn")))
		{
			if (Rest == TEXT("respawn"))
			{
				Rally.Respawn();
			}
			else
			{
				bAutoplay = true;
			}
			return true;
		}
		if (Head == TEXT("close"))
		{
			Finish();
			return true;
		}
		return false;
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		Hits.Reset();
		const FPainter Paint{AllottedGeometry, OutDrawElements, LayerId, TextScale};
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		Paint.Box(FVector2D::ZeroVector, Size, Void);
		switch (Kind)
		{
		case EBIBOverlayKind::NightShift: PaintNight(Paint, Size); break;
		case EBIBOverlayKind::UltimateNight: PaintUcn(Paint, Size); break;
		case EBIBOverlayKind::Rhythm: PaintRhythm(Paint, Size); break;
		case EBIBOverlayKind::ArenaBattle: PaintArena(Paint, Size); break;
		case EBIBOverlayKind::HaloFall: PaintHalo(Paint, Size); break;
		case EBIBOverlayKind::NeonMemory: PaintMemory(Paint, Size); break;
		case EBIBOverlayKind::FirewallBreach: PaintBreach(Paint, Size); break;
		case EBIBOverlayKind::ArkRally: PaintRally(Paint, Size); break;
		}
		if (bShowResult)
		{
			const FVector2D PanelSize(Size.X * 0.6f, Size.Y * 0.28f);
			const FVector2D PanelPos((Size.X - PanelSize.X) * 0.5f, Size.Y * 0.36f);
			FPainter Top{AllottedGeometry, OutDrawElements, LayerId + 10, TextScale};
			Top.Box(PanelPos, PanelSize, FLinearColor(Panel.R, Panel.G, Panel.B, 0.97f));
			Top.Frame(PanelPos, PanelSize, ResultGood ? Gold : Pink, 3.f);
			Top.Text(ResultTitle, FVector2D(Size.X * 0.5f, PanelPos.Y + 18.f), 30, ResultGood ? Gold : Pink, 1);
			WrapText(Top, ResultText, FVector2D(PanelPos.X + 24.f, PanelPos.Y + 74.f), PanelSize.X - 48.f, 15, Ink);
			Top.Text(TEXT("Enter or click to continue"), FVector2D(Size.X * 0.5f, PanelPos.Y + PanelSize.Y - 34.f), 13, Dim, 1, false);
		}
		return LayerId + 12;
	}

private:
	// ------------------------------------------------------------------------------------------ state
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UBIBGameInstance> Instance;
	EBIBOverlayKind Kind = EBIBOverlayKind::NightShift;
	int32 A = 0;
	int32 B = 0;
	float TextScale = 1.f;
	bool bCaptions = false;
	bool bReduceMotion = false;
	float SfxVolume = 0.8f;
	float MusicVolume = 0.28f;
	double Clock = 0.0;

	BIB::Arcade::FNightShift Night;
	bool bKeyLight[2] = {false, false};
	bool bQuitArmed = false;
	double JumpscareUntil = -1.0;
	double CaptionUntil[2] = {-1.0, -1.0};
	FString Caption[2];
	double NextThump = 0.0;

	// Ultimate Custom Night (2026-10-14).
	BUcn::FUltimateNight Ucn;
	TUniquePtr<BUcn::FBot> UcnBot;   // debug autoplay only
	FString UcnNote;                 // centre line: what just happened (always shown: part of the visual telegraph)
	double UcnNoteUntil = -1.0;
	FLinearColor UcnNoteColour = FLinearColor::White;
	double UcnShockFlash = -10.0;
	double UcnStarSmashAt = -10.0;   // Stargazer smashing the front window

	BIB::Arcade::FRhythm Song;
	double OffsetSeconds = 0.0;
	double SimDelay = 0.0;
	double MusicDelay = 0.0;
	bool bMusicStarted = false;
	bool bAutoplay = false;
	size_t AutoNext = 0;
	bool bLaneDown[BIB::Arcade::RhythmMaxLanes] = {};
	double LaneFlash[BIB::Arcade::RhythmMaxLanes] = {-1.0, -1.0, -1.0, -1.0, -1.0};

	// The 2026-10-07 mini-games (BIBRules/BIBMiniGames.h).
	BIB::Arcade::FHaloFall Halo;
	bool bHaloLeft = false;
	bool bHaloRight = false;
	BIB::Arcade::FNeonMemory Memory;
	int32 MemoryCursor = 0;
	double MemoryFlash = -10.0;
	BIB::Arcade::FFirewallBreach Breach;
	BIB::Arcade::FArkRally Rally;      // 2026-10-15 (BIBRules/BIBRally.h)
	bool bRallyKey[5] = {};            // held keys: gas, brake, lean back, lean forward, nitro
	bool bRallyTouch[5] = {};          // the same five on-screen buttons (mouse / touch)
	FVector2D RallyCamera = FVector2D::ZeroVector;
	FString RallyFlash;                // a short line in the middle of the screen (CHECKPOINT, CRASH, BOOST...)
	double RallyFlashAt = -10.0;
	double MiniEndAt = -1.0;   // a finished game stays on screen until then, then the result panel opens
	struct FSpark
	{
		FVector2D Where;   // field units (0..1)
		double Born = 0.0;
		bool bGold = false;
	};
	TArray<FSpark> Sparks;

	BIB::Arena::FBattle Battle;
	BIB::Arena::FLineup Attack;
	BIB::Arena::FLineup Defence;
	TArray<float> Hp[2];
	TArray<float> MaxHp[2];
	double ReplayTime = 0.0;
	int32 NextEvent = 0;
	struct FFloat
	{
		FVector2D Where;
		FString Text;
		double Born = 0.0;
		bool bBig = false;
	};
	TArray<FFloat> Floats;
	TArray<int32> FloatSlot[2];

	bool bResolved = false;
	bool bShowResult = false;
	bool ResultGood = false;
	FString ResultTitle;
	FString ResultText;
	bool bClosed = false;

	TArray<FVoice> Voices;
	mutable TArray<TPair<FSlateRect, FString>> Hits;
	mutable TMap<FString, TSharedPtr<FSlateBrush>> Brushes;
	mutable TMap<FString, TStrongObjectPtr<UTexture2D>> Textures;

	// ------------------------------------------------------------------------------------------ helpers
	BIB::Arcade::EDifficulty MiniDifficulty() const
	{
		return static_cast<BIB::Arcade::EDifficulty>(FMath::Clamp(A, 0, BIB::Arcade::DifficultyCount - 1));
	}

	BUcn::EMode UcnMode() const
	{
		return static_cast<BUcn::EMode>(FMath::Clamp(A, 0, BUcn::ModeCount - 1));
	}

	bool IsMini() const
	{
		return Kind == EBIBOverlayKind::HaloFall || Kind == EBIBOverlayKind::NeonMemory || Kind == EBIBOverlayKind::FirewallBreach || Kind == EBIBOverlayKind::ArkRally;
	}

	/** True when the key is battle binding Index (0 forward, 1 back, 2 left, 3 right, 4 jump = the mini-games' action key). */
	bool IsBattleKey(const FString& Key, int32 Index) const
	{
		return Instance.IsValid() && KeyOf(Instance->GetProfile().Meta.Settings, Index) == Key;
	}

	FString OperatorFace(int32 RosterIndex) const
	{
		return RosterIndex >= 0 && RosterIndex < static_cast<int32>(BIB::Roster().size()) ? ToF(BIB::Roster()[static_cast<size_t>(RosterIndex)].Name).ToLower() : FString();
	}

	FString OperatorName(int32 RosterIndex) const
	{
		return RosterIndex >= 0 && RosterIndex < static_cast<int32>(BIB::Roster().size()) ? ToF(BIB::Roster()[static_cast<size_t>(RosterIndex)].Name) : FString();
	}

	const FSlateBrush* Portrait(const FString& Name) const
	{
		if (const TSharedPtr<FSlateBrush>* Found = Brushes.Find(Name))
		{
			return Found->Get();
		}
		const FString Path = FString::Printf(TEXT("/Game/BIB/UI/Portraits/T_%s_head.T_%s_head"), *Name, *Name);
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		TSharedPtr<FSlateBrush> Brush;
		if (Texture)
		{
			Textures.Add(Name, TStrongObjectPtr<UTexture2D>(Texture));
			Brush = MakeShared<FSlateBrush>();
			Brush->SetResourceObject(Texture);
			Brush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
			Brush->DrawAs = ESlateBrushDrawType::Image;
		}
		Brushes.Add(Name, Brush);
		return Brush.Get();
	}

	void Hit(const FVector2D& Pos, const FVector2D& Size, const FString& Action) const
	{
		Hits.Add(TPair<FSlateRect, FString>(FSlateRect(Pos.X, Pos.Y, Pos.X + Size.X, Pos.Y + Size.Y), Action));
	}

	void Button(const FPainter& Paint, const FVector2D& Pos, const FVector2D& Size, const FString& Label, const FString& Sub, const FString& Action, bool bActive,
		const FLinearColor& Accent) const
	{
		Paint.Box(Pos, Size, bActive ? FLinearColor(Accent.R * 0.55f, Accent.G * 0.55f, Accent.B * 0.55f, 1.f) : Panel);
		Paint.Frame(Pos, Size, bActive ? Accent : FLinearColor(1.f, 1.f, 1.f, 0.18f), 2.f);
		Paint.Text(Label, FVector2D(Pos.X + Size.X * 0.5f, Pos.Y + Size.Y * 0.5f - 16.f), 16, bActive ? Cream : Ink, 1);
		if (!Sub.IsEmpty())
		{
			Paint.Text(Sub, FVector2D(Pos.X + Size.X * 0.5f, Pos.Y + Size.Y * 0.5f + 6.f), 11, Dim, 1, false);
		}
		Hit(Pos, Size, Action);
	}

	void WrapText(const FPainter& Paint, const FString& Text, const FVector2D& Pos, float Width, int32 Size, const FLinearColor& Colour) const
	{
		TArray<FString> Words;
		Text.ParseIntoArray(Words, TEXT(" "));
		FString Line;
		float Y = Pos.Y;
		for (const FString& Word : Words)
		{
			const FString Next = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			if (!Line.IsEmpty() && Paint.Measure(Next, Size, false).X > Width)
			{
				Paint.Text(Line, FVector2D(Pos.X, Y), Size, Colour, 0, false);
				Y += Size * TextScale * 1.5f;
				Line = Word;
			}
			else
			{
				Line = Next;
			}
		}
		if (!Line.IsEmpty())
		{
			Paint.Text(Line, FVector2D(Pos.X, Y), Size, Colour, 0, false);
		}
	}

	void Play(const BIB::Synth::FBuffer& Buffer, float Volume)
	{
		UWorld* Target = World.Get();
		if (!Target || Buffer.Samples.empty() || Volume <= 0.f)
		{
			return;
		}
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>();
		Wave->SetSampleRate(static_cast<uint32>(Buffer.Rate));
		Wave->NumChannels = Buffer.Channels;
		Wave->Duration = static_cast<float>(Buffer.Seconds());
		Wave->SoundGroup = SOUNDGROUP_Default;
		Wave->bLooping = false;
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Buffer.Samples.data()), static_cast<int32>(Buffer.Samples.size() * sizeof(int16)));
		UAudioComponent* Component = UGameplayStatics::CreateSound2D(Target, Wave, Volume, 1.f, 0.f, nullptr, false, true);
		if (!Component)
		{
			return;
		}
		Component->Play();
		FVoice Voice;
		Voice.Component = TStrongObjectPtr<UAudioComponent>(Component);
		Voice.Wave = TStrongObjectPtr<USoundWaveProcedural>(Wave);
		Voice.EndTime = Clock + Buffer.Seconds() + 0.25;
		Voices.Add(MoveTemp(Voice));
	}

	void StopAllSounds()
	{
		for (FVoice& Voice : Voices)
		{
			if (Voice.Component.IsValid())
			{
				Voice.Component->Stop();
			}
		}
		Voices.Reset();
	}

	void ReapSounds()
	{
		for (int32 Index = Voices.Num() - 1; Index >= 0; --Index)
		{
			if (Clock >= Voices[Index].EndTime)
			{
				if (Voices[Index].Component.IsValid())
				{
					Voices[Index].Component->Stop();
				}
				Voices.RemoveAtSwap(Index);
			}
		}
	}

	float SideOf(int32 Side) const
	{
		return Side == 0 ? -1.f : 1.f;
	}

	// ------------------------------------------------------------------------------------------ input
	int32 BindIndexFor(const FString& Key, int32 First, int32 Count) const
	{
		if (!Instance.IsValid())
		{
			return -1;
		}
		const BIB::FSettings& Settings = Instance->GetProfile().Meta.Settings;
		for (int32 Index = First; Index < First + Count; ++Index)
		{
			if (KeyOf(Settings, Index) == Key)
			{
				return Index - First;
			}
		}
		return -1;
	}

	void HandleKey(const FString& Key, bool bDown)
	{
		if (bShowResult)
		{
			if (bDown && (Key == TEXT("Enter") || Key == TEXT("SpaceBar") || Key == TEXT("Escape")))
			{
				Finish();
			}
			return;
		}
		if (Kind == EBIBOverlayKind::NightShift)
		{
			const int32 Bind = BindIndexFor(Key, BIB::NightBindFirst, BIB::NightBindCount);
			if (Bind == 2 || Bind == 3)
			{
				bKeyLight[Bind - 2] = bDown;
				Night.SetLight(Bind == 2 ? BIB::Arcade::ESide::Left : BIB::Arcade::ESide::Right, bDown);
				return;
			}
			if (!bDown)
			{
				return;
			}
			if (Bind == 0 || Bind == 1)
			{
				Click(Bind == 0 ? TEXT("door:0") : TEXT("door:1"), true);
				return;
			}
			if (Bind == 4)
			{
				Click(TEXT("cam"), true);
				return;
			}
			static const TCHAR* Digits[10] = {TEXT("One"), TEXT("Two"), TEXT("Three"), TEXT("Four"), TEXT("Five"), TEXT("Six"), TEXT("Seven"), TEXT("Eight"), TEXT("Nine"), TEXT("Zero")};
			for (int32 Index = 0; Index < 10; ++Index)
			{
				if (Key == Digits[Index])
				{
					Click(FString::Printf(TEXT("sel:%d"), Index), true);
					return;
				}
			}
			if (Key == TEXT("Escape"))
			{
				Click(TEXT("quit"), true);
			}
			return;
		}
		if (Kind == EBIBOverlayKind::UltimateNight)
		{
			if (!bDown)
			{
				return;
			}
			// Night Shift's door and camera bindings; the extra tools have fixed keys (shown on the mode page).
			const int32 Bind = BindIndexFor(Key, BIB::NightBindFirst, BIB::NightBindCount);
			if (Bind == 0 || Bind == 1)
			{
				Click(Bind == 0 ? TEXT("ucn:door:0") : TEXT("ucn:door:1"), true);
				return;
			}
			if (Bind == 4)
			{
				Click(TEXT("ucn:cam"), true);
				return;
			}
			static const TCHAR* UcnDigits[10] = {TEXT("One"), TEXT("Two"), TEXT("Three"), TEXT("Four"), TEXT("Five"), TEXT("Six"), TEXT("Seven"), TEXT("Eight"), TEXT("Nine"), TEXT("Zero")};
			for (int32 Index = 0; Index < 10; ++Index)
			{
				if (Key == UcnDigits[Index])
				{
					Click(FString::Printf(TEXT("ucn:sel:%d"), Index), true);
					return;
				}
			}
			if (Key == TEXT("F")) Click(TEXT("ucn:shock"), true);   // F = flash / shock (Space is Night Shift's camera key)
			else if (Key == TEXT("W")) Click(TEXT("ucn:window"), true);
			else if (Key == TEXT("V")) Click(TEXT("ucn:vaccine"), true);
			else if (Key == TEXT("X")) Click(TEXT("ucn:ad"), true);
			else if (Key == TEXT("C")) Click(TEXT("ucn:urgent"), true);
			else if (Key == TEXT("Escape")) Click(TEXT("quit"), true);
			return;
		}
		if (Kind == EBIBOverlayKind::Rhythm)
		{
			const int32 Lane = BindIndexFor(Key, BIB::RhythmBindFirst, Song.Lanes());
			if (Lane >= 0)
			{
				bLaneDown[Lane] = bDown;
				if (bDown)
				{
					Click(FString::Printf(TEXT("lane:%d"), Lane), true);
				}
				return;
			}
			if (bDown && Key == TEXT("Escape"))
			{
				Click(TEXT("quit"), true);
			}
			return;
		}
		if (Kind == EBIBOverlayKind::HaloFall)
		{
			if (Key == TEXT("Left") || IsBattleKey(Key, 2))
			{
				bHaloLeft = bDown;
			}
			else if (Key == TEXT("Right") || IsBattleKey(Key, 3))
			{
				bHaloRight = bDown;
			}
			else if (Key == TEXT("Up") || IsBattleKey(Key, 4))
			{
				Halo.SetFiring(bDown);
			}
			else if (bDown && Key == TEXT("Escape"))
			{
				Click(TEXT("quit"), true);
			}
			Halo.SetMove((bHaloRight ? 1 : 0) - (bHaloLeft ? 1 : 0));
			return;
		}
		if (Kind == EBIBOverlayKind::NeonMemory)
		{
			if (!bDown)
			{
				return;
			}
			const int32 Cols = FMath::Max(1, Memory.Cols());
			const int32 Count = FMath::Max(1, Memory.CardCount());
			if (Key == TEXT("Left") || IsBattleKey(Key, 2)) MemoryCursor = (MemoryCursor + Count - 1) % Count;
			else if (Key == TEXT("Right") || IsBattleKey(Key, 3)) MemoryCursor = (MemoryCursor + 1) % Count;
			else if (Key == TEXT("Up") || IsBattleKey(Key, 0)) MemoryCursor = (MemoryCursor + Count - Cols) % Count;
			else if (Key == TEXT("Down") || IsBattleKey(Key, 1)) MemoryCursor = (MemoryCursor + Cols) % Count;
			else if (Key == TEXT("Enter") || IsBattleKey(Key, 4)) Click(FString::Printf(TEXT("card:%d"), MemoryCursor), true);
			else if (Key == TEXT("Escape")) Click(TEXT("quit"), true);
			return;
		}
		if (Kind == EBIBOverlayKind::ArkRally)
		{
			// Gas / brake on one hand, lean on the other (the battle keys or the arrows); Space is gas too.
			int32 Control = -1;
			if (Key == TEXT("SpaceBar") || Key == TEXT("LeftShift") || Key == TEXT("RightShift") || IsBattleKey(Key, 4)) Control = 4;   // nitro
			else if (Key == TEXT("Up") || IsBattleKey(Key, 0)) Control = 0;
			else if (Key == TEXT("Down") || IsBattleKey(Key, 1)) Control = 1;
			else if (Key == TEXT("Left") || IsBattleKey(Key, 2)) Control = 2;
			else if (Key == TEXT("Right") || IsBattleKey(Key, 3)) Control = 3;
			if (Control >= 0)
			{
				bRallyKey[Control] = bDown;
				ApplyRallyInput();
			}
			else if (bDown && Key == TEXT("R"))
			{
				Click(TEXT("respawn"), true);
			}
			else if (bDown && Key == TEXT("Escape"))
			{
				Click(TEXT("quit"), true);
			}
			return;
		}
		if (Kind == EBIBOverlayKind::FirewallBreach)
		{
			if (bDown && (Key == TEXT("Enter") || Key == TEXT("SpaceBar") || IsBattleKey(Key, 4)))
			{
				Click(TEXT("press"), true);
			}
			else if (bDown && Key == TEXT("Escape"))
			{
				Click(TEXT("quit"), true);
			}
			return;
		}
		if (bDown && (Key == TEXT("SpaceBar") || Key == TEXT("Enter") || Key == TEXT("Escape")))
		{
			Click(TEXT("skip"), true);
		}
	}

	void Click(const FString& Action, bool bDown)
	{
		if (Action == TEXT("close"))
		{
			Finish();
			return;
		}
		if (bShowResult)
		{
			return;
		}
		if (Kind == EBIBOverlayKind::NightShift && !Night.IsOver())
		{
			if (Action == TEXT("quit"))
			{
				if (bQuitArmed)
				{
					// Giving up ends the night as a loss (the hours survived still pay a little).
					ResolveNight(TEXT("You left your post at ") + ToF(Night.ClockText()) + TEXT("."));
				}
				bQuitArmed = true;
				return;
			}
			bQuitArmed = false;
			if (Action.StartsWith(TEXT("door:")))
			{
				const BIB::Arcade::ESide Side = Action.EndsWith(TEXT("0")) ? BIB::Arcade::ESide::Left : BIB::Arcade::ESide::Right;
				if (Night.ToggleDoor(Side))
				{
					Play(BIB::Synth::DoorSlam(Side == BIB::Arcade::ESide::Left ? -0.8 : 0.8), SfxVolume);
				}
				else if (Night.IsDoorBroken(Side))
				{
					Play(BIB::Synth::MissBuzz(), SfxVolume);
				}
				return;
			}
			if (Action.StartsWith(TEXT("light:")))
			{
				const int32 Side = Action.EndsWith(TEXT("0")) ? 0 : 1;
				Night.SetLight(Side == 0 ? BIB::Arcade::ESide::Left : BIB::Arcade::ESide::Right, bDown);
				return;
			}
			if (Action == TEXT("cam"))
			{
				Night.ToggleCamera();
				Play(BIB::Synth::CameraBlip(), SfxVolume * 0.6f);
				return;
			}
			if (Action.StartsWith(TEXT("sel:")))
			{
				Night.SelectCamera(FCString::Atoi(*Action.RightChop(4)));
				if (Night.IsCameraUp())
				{
					Play(BIB::Synth::CameraBlip(), SfxVolume * 0.4f);
				}
				return;
			}
			return;
		}
		if (Kind == EBIBOverlayKind::UltimateNight && !Ucn.IsOver())
		{
			ClickUcn(Action);
			return;
		}
		if (Kind == EBIBOverlayKind::Rhythm && !Song.IsOver())
		{
			if (Action == TEXT("quit"))
			{
				if (bQuitArmed)
				{
					ResolveRhythm(true);
				}
				bQuitArmed = true;
				return;
			}
			if (Action.StartsWith(TEXT("lane:")) && SimDelay <= 0.0)
			{
				const int32 Lane = FCString::Atoi(*Action.RightChop(5));
				const BIB::Arcade::EJudge Judge = Song.Press(Lane);
				LaneFlash[FMath::Clamp(Lane, 0, BIB::Arcade::RhythmMaxLanes - 1)] = Clock;
				const BIB::Arcade::FSong& Info = BIB::Arcade::Song(Song.SongIndex());
				if (Judge == BIB::Arcade::EJudge::Miss)
				{
					Play(BIB::Synth::MissBuzz(), SfxVolume * 0.5f);
				}
				else
				{
					const float Pan = Song.Lanes() > 1 ? -0.6f + 1.2f * Lane / (Song.Lanes() - 1) : 0.f;
					Play(BIB::Synth::LaneTone(Info.LaneHz[FMath::Clamp(Lane, 0, BIB::Arcade::RhythmMaxLanes - 1)], Pan), SfxVolume);
				}
			}
			return;
		}
		if (IsMini() && !MiniOver())
		{
			if (Action == TEXT("quit"))
			{
				if (bQuitArmed)
				{
					ResolveMini(true);
				}
				bQuitArmed = true;
				return;
			}
			bQuitArmed = false;
			if (Kind == EBIBOverlayKind::NeonMemory && Action.StartsWith(TEXT("card:")))
			{
				const int32 Card = FCString::Atoi(*Action.RightChop(5));
				const int32 Before = Memory.Matched();
				MemoryCursor = FMath::Clamp(Card, 0, FMath::Max(0, Memory.CardCount() - 1));
				if (Memory.Flip(Card))
				{
					const int32 Cols = FMath::Max(1, Memory.Cols());
					const float Pan = Cols > 1 ? -0.6f + 1.2f * static_cast<float>(Card % Cols) / static_cast<float>(Cols - 1) : 0.f;
					if (Memory.Matched() > Before)
					{
						MemoryFlash = Clock;
						Play(BIB::Synth::LaneTone(523.25 * FMath::Pow(1.122, static_cast<double>(FMath::Min(Memory.Combo(), 8))), Pan), SfxVolume);
					}
					else
					{
						Play(BIB::Synth::CameraBlip(), SfxVolume * 0.5f);
					}
				}
				return;
			}
			if (Kind == EBIBOverlayKind::FirewallBreach && Action == TEXT("press"))
			{
				PlayBreachJudge(Breach.Press());
				return;
			}
			if (Kind == EBIBOverlayKind::ArkRally && Action.StartsWith(TEXT("rhold:")))
			{
				bRallyTouch[FMath::Clamp(FCString::Atoi(*Action.RightChop(6)), 0, 4)] = bDown;
				ApplyRallyInput();
				return;
			}
			if (Kind == EBIBOverlayKind::ArkRally && Action == TEXT("respawn"))
			{
				Rally.Respawn();
				return;
			}
			return;
		}
		if (Kind == EBIBOverlayKind::ArenaBattle && Action == TEXT("skip"))
		{
			while (NextEvent < static_cast<int32>(Battle.Events.size()))
			{
				ApplyBattleEvent(Battle.Events[static_cast<size_t>(NextEvent++)], false);
			}
			ResolveArena();
		}
	}

	// ------------------------------------------------------------------------------------------ the clock
	EActiveTimerReturnType Advance(double InCurrentTime, float InDeltaTime)
	{
		if (bClosed)
		{
			return EActiveTimerReturnType::Stop;
		}
		Step(FMath::Clamp(static_cast<double>(InDeltaTime), 0.0, 0.1));
		Invalidate(EInvalidateWidgetReason::Paint);
		return EActiveTimerReturnType::Continue;
	}

	void Step(double Dt)
	{
		Clock += Dt;
		ReapSounds();
		switch (Kind)
		{
		case EBIBOverlayKind::NightShift: StepNight(Dt); break;
		case EBIBOverlayKind::UltimateNight: StepUcn(Dt); break;
		case EBIBOverlayKind::Rhythm: StepRhythm(Dt); break;
		case EBIBOverlayKind::ArenaBattle: StepArena(Dt); break;
		case EBIBOverlayKind::HaloFall: StepHalo(Dt); break;
		case EBIBOverlayKind::NeonMemory: StepMemory(Dt); break;
		case EBIBOverlayKind::FirewallBreach: StepBreach(Dt); break;
		case EBIBOverlayKind::ArkRally: StepRally(Dt); break;
		}
	}

	bool MiniOver() const
	{
		switch (Kind)
		{
		case EBIBOverlayKind::HaloFall: return Halo.IsOver();
		case EBIBOverlayKind::NeonMemory: return Memory.IsOver();
		case EBIBOverlayKind::FirewallBreach: return Breach.IsOver();
		case EBIBOverlayKind::ArkRally: return Rally.IsOver();
		default: return false;
		}
	}

	/** A finished mini-game stays on screen for a moment, then the result panel opens. */
	void CheckMiniEnd(double Linger)
	{
		if (bResolved || !MiniOver())
		{
			return;
		}
		if (MiniEndAt < 0.0)
		{
			MiniEndAt = Clock + Linger;
		}
		else if (Clock >= MiniEndAt)
		{
			ResolveMini(false);
		}
	}

	void StepHalo(double Dt)
	{
		if (!Halo.IsOver())
		{
			Halo.Tick(Dt);
			const double Pan = (Halo.PlayerX() - 0.5) * 1.6;
			for (const BIB::Arcade::FHaloEvent& Event : Halo.PopEvents())
			{
				switch (Event.Kind)
				{
				case BIB::Arcade::EHaloEventKind::Shot:
					Play(BIB::Synth::Tone(1320.0, 0.05, Pan, 0.25, BIB::Synth::EWave::Square), SfxVolume * 0.4f);
					break;
				case BIB::Arcade::EHaloEventKind::AngelDown:
					Play(BIB::Synth::HitSound(Pan, false), SfxVolume * 0.6f);
					Sparks.Add({FVector2D(Halo.PlayerX(), 0.45), Clock, false});
					break;
				case BIB::Arcade::EHaloEventKind::SeraphDown:
					Play(BIB::Synth::Chime(), SfxVolume);
					Sparks.Add({FVector2D(Halo.SeraphX(), 0.05), Clock, true});
					break;
				case BIB::Arcade::EHaloEventKind::PlayerHit:
					Play(BIB::Synth::Punch(Pan, true), SfxVolume);
					break;
				case BIB::Arcade::EHaloEventKind::WaveCleared:
					Play(BIB::Synth::Chime(), SfxVolume * 0.8f);
					break;
				case BIB::Arcade::EHaloEventKind::Lost:
					Play(BIB::Synth::PowerDown(), SfxVolume);
					break;
				case BIB::Arcade::EHaloEventKind::Won:
					break;
				}
			}
		}
		CheckMiniEnd(1.2);
	}

	void StepMemory(double Dt)
	{
		if (!Memory.IsOver())
		{
			const bool bWasPreview = Memory.InPreview();
			Memory.Tick(Dt);
			if (bWasPreview && !Memory.InPreview())
			{
				Play(BIB::Synth::CameraBlip(), SfxVolume * 0.6f);
			}
		}
		CheckMiniEnd(1.0);
	}

	void PlayBreachJudge(BIB::Arcade::EBreachJudge Judge)
	{
		switch (Judge)
		{
		case BIB::Arcade::EBreachJudge::Perfect: Play(BIB::Synth::LaneTone(987.77, 0.0), SfxVolume); break;
		case BIB::Arcade::EBreachJudge::Good: Play(BIB::Synth::LaneTone(659.25, 0.0), SfxVolume); break;
		case BIB::Arcade::EBreachJudge::Miss: Play(BIB::Synth::MissBuzz(), SfxVolume); break;
		case BIB::Arcade::EBreachJudge::None: break;
		}
	}

	void StepBreach(double Dt)
	{
		if (!Breach.IsOver())
		{
			const int32 MissesBefore = Breach.Misses();
			const double ToCentre = Breach.SecondsToCentre();
			if (bAutoplay && ToCentre <= Dt)
			{
				Breach.Tick(ToCentre);
				PlayBreachJudge(Breach.Press());
			}
			else
			{
				Breach.Tick(Dt);
				if (Breach.Misses() > MissesBefore)
				{
					Play(BIB::Synth::MissBuzz(), SfxVolume);   // the trace ran out
				}
			}
		}
		CheckMiniEnd(0.8);
	}

	void ApplyRallyInput()
	{
		const auto Held = [this](int32 Index) { return bRallyKey[Index] || bRallyTouch[Index]; };
		Rally.SetThrottle((Held(0) ? 1 : 0) - (Held(1) ? 1 : 0));
		Rally.SetLean((Held(2) ? 1 : 0) - (Held(3) ? 1 : 0));
		Rally.SetNitro(Held(4));
	}

	void SetRallyFlash(const FString& Text)
	{
		RallyFlash = Text;
		RallyFlashAt = Clock;
	}

	void StepRally(double Dt)
	{
		if (!Rally.IsOver())
		{
			if (bAutoplay)
			{
				for (double Done = 0.0; Done + 1e-9 < Dt && !Rally.IsOver(); Done += BIB::Arcade::RallyStep)
				{
					Rally.Autopilot();
					Rally.Tick(BIB::Arcade::RallyStep);
				}
			}
			else
			{
				Rally.Tick(Dt);
			}
			for (const BIB::Arcade::FRallyEvent& Event : Rally.PopEvents())
			{
				switch (Event.Kind)
				{
				case BIB::Arcade::ERallyEventKind::Coin:
					Play(BIB::Synth::LaneTone(1046.5 + 40.0 * FMath::Min(Event.A % 8, 7), 0.0), SfxVolume * 0.45f);
					break;
				case BIB::Arcade::ERallyEventKind::Checkpoint:
					Play(BIB::Synth::Chime(), SfxVolume * 0.6f);
					SetRallyFlash(TEXT("CHECKPOINT"));
					break;
				case BIB::Arcade::ERallyEventKind::Crash:
					Play(BIB::Synth::Punch(0.0, true), SfxVolume);
					SetRallyFlash(TEXT("CRASH!"));
					break;
				case BIB::Arcade::ERallyEventKind::Respawn:
					Play(BIB::Synth::CameraBlip(), SfxVolume * 0.6f);
					break;
				case BIB::Arcade::ERallyEventKind::Bomb:
					Play(BIB::Synth::HitSound(0.0, true), SfxVolume);
					SetRallyFlash(TEXT("BOOM"));
					break;
				case BIB::Arcade::ERallyEventKind::Spring:
					Play(BIB::Synth::Tone(330.0, 0.18, 0.0, 0.4, BIB::Synth::EWave::Square), SfxVolume * 0.6f);
					break;
				case BIB::Arcade::ERallyEventKind::Boost:
					Play(BIB::Synth::Tone(880.0, 0.25, 0.0, 0.35, BIB::Synth::EWave::Square), SfxVolume * 0.5f);
					SetRallyFlash(TEXT("BOOST"));
					break;
				case BIB::Arcade::ERallyEventKind::Land:
					Play(BIB::Synth::HitSound(0.0, Event.A > 90), SfxVolume * FMath::Clamp(Event.A / 150.f, 0.2f, 0.7f));
					break;
				case BIB::Arcade::ERallyEventKind::Finished:
					Play(BIB::Synth::Chime(), SfxVolume);
					SetRallyFlash(FString::Printf(TEXT("FINISH - %s"), *PlaceText(Event.A)));
					break;
				case BIB::Arcade::ERallyEventKind::TimeUp:
					Play(BIB::Synth::PowerDown(), SfxVolume);
					break;
				case BIB::Arcade::ERallyEventKind::Flip:
					Play(BIB::Synth::Chime(), SfxVolume * 0.7f);
					SetRallyFlash(FMath::Abs(Event.A) > 1 ? FString::Printf(TEXT("%dx %sFLIP!  +NITRO"), FMath::Abs(Event.A), Event.A < 0 ? TEXT("FRONT ") : TEXT(""))
						: FString::Printf(TEXT("%sFLIP!  +NITRO"), Event.A < 0 ? TEXT("FRONT ") : TEXT("")));
					break;
				case BIB::Arcade::ERallyEventKind::Nitro:
					Play(BIB::Synth::Tone(220.0, 0.35, 0.0, 0.4, BIB::Synth::EWave::Square), SfxVolume * 0.5f);
					break;
				}
			}
		}
		// The camera trails the bike and looks ahead in the direction of travel.
		const BIB::Arcade::FRallyBody& Body = Rally.Chassis();
		const FVector2D Target(Body.Pos.X + FMath::Clamp(Body.Vel.X * 0.35, -3.0, 7.0), Body.Pos.Y + 1.2);
		RallyCamera += (Target - RallyCamera) * FMath::Min(1.0, Dt * (bReduceMotion ? 3.0 : 5.0));
		CheckMiniEnd(1.6);
	}

	static FString PlaceText(int32 Place)
	{
		static const TCHAR* const Names[4] = {TEXT("1ST"), TEXT("2ND"), TEXT("3RD"), TEXT("4TH")};
		return Names[FMath::Clamp(Place, 1, 4) - 1];
	}

	void StepNight(double Dt)
	{
		if (Night.IsOver())
		{
			if (!bResolved && Clock >= JumpscareUntil)
			{
				ResolveNight(FString());
			}
			return;
		}
		Night.Tick(Dt);
		for (const BIB::Arcade::FNightEvent& Event : Night.PopEvents())
		{
			const float Pan = SideOf(Event.A) * 0.95f;
			switch (Event.Kind)
			{
			case BIB::Arcade::ENightEventKind::MediaArrived:
				Play(BIB::Synth::MediaCue(SideOf(Event.A)), SfxVolume);
				SetCaption(Event.A, TEXT("MEDIA!"), 2.0);
				break;
			case BIB::Arcade::ENightEventKind::KuuWindup:
				SetCaption(Event.A, TEXT("thump... thump..."), BIB::Arcade::KuuWindupSeconds);
				NextThump = Clock;
				break;
			case BIB::Arcade::ENightEventKind::KuuPunch:
				Play(BIB::Synth::Punch(Pan, Event.B == 1), SfxVolume);
				SetCaption(Event.A, Event.B == 1 ? TEXT("CRASH - door broken") : TEXT("whoosh - missed"), 2.0);
				break;
			case BIB::Arcade::ENightEventKind::Knock:
				Play(BIB::Synth::Knock(Pan), SfxVolume);
				SetCaption(Event.A, TEXT("knock knock"), 1.5);
				break;
			case BIB::Arcade::ENightEventKind::DoorRepaired:
				SetCaption(Event.A, TEXT("door repaired"), 1.5);
				break;
			case BIB::Arcade::ENightEventKind::PowerOut:
				Play(BIB::Synth::PowerDown(), SfxVolume);
				break;
			case BIB::Arcade::ENightEventKind::Jumpscare:
				Play(BIB::Synth::Jumpscare(), SfxVolume);
				JumpscareUntil = Clock + 1.6;
				break;
			case BIB::Arcade::ENightEventKind::Survived:
				Play(BIB::Synth::Chime(), SfxVolume);
				JumpscareUntil = Clock + 1.0;
				break;
			default:
				break;
			}
		}
		// Kuu's wind-up is heard as heavy thumps from her side.
		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (Night.IsKuuWindingUp(Side == 0 ? BIB::Arcade::ESide::Left : BIB::Arcade::ESide::Right) && Clock >= NextThump)
			{
				Play(BIB::Synth::KuuThump(SideOf(Side) * 0.9f), SfxVolume);
				NextThump = Clock + 1.0;
			}
		}
	}

	void SetCaption(int32 Side, const FString& Text, double Seconds)
	{
		Caption[FMath::Clamp(Side, 0, 1)] = Text;
		CaptionUntil[FMath::Clamp(Side, 0, 1)] = Clock + Seconds;
	}

	void StepRhythm(double Dt)
	{
		if (!bMusicStarted)
		{
			MusicDelay -= Dt;
			if (MusicDelay <= 0.0)
			{
				bMusicStarted = true;
				Play(BIB::Synth::RhythmBacking(Song.SongIndex()), MusicVolume);
			}
		}
		if (SimDelay > 0.0)
		{
			SimDelay -= Dt;
			return;
		}
		if (Song.IsOver())
		{
			if (!bResolved)
			{
				ResolveRhythm(false);
			}
			return;
		}
		Song.Tick(Dt);
		if (bAutoplay)
		{
			while (AutoNext < Song.Notes().size() && Song.Notes()[AutoNext].Time <= Song.Time())
			{
				Song.Press(Song.Notes()[AutoNext].Lane);
				LaneFlash[FMath::Clamp(Song.Notes()[AutoNext].Lane, 0, BIB::Arcade::RhythmMaxLanes - 1)] = Clock;
				++AutoNext;
			}
		}
	}

	void ApplyBattleEvent(const BIB::Arena::FBattleEvent& Event, bool bSound)
	{
		const int32 Foe = 1 - Event.Side;
		if (Hp[Foe].IsValidIndex(Event.Target))
		{
			Hp[Foe][Event.Target] = FMath::Max(0.f, Hp[Foe][Event.Target] - static_cast<float>(Event.Damage));
			FFloat Float;
			Float.Where = FVector2D(Foe, Event.Target);
			Float.Text = FString::Printf(TEXT("%s%s"), Event.Action == BIB::Arena::EAction::Ultimate ? TEXT("ULT ") : (Event.Action == BIB::Arena::EAction::Special ? TEXT("SP ") : TEXT("")),
				*ToF(BIB::Meta::Num(static_cast<long long>(Event.Damage))));
			Float.Born = Clock;
			Float.bBig = Event.Action != BIB::Arena::EAction::Attack;
			Floats.Add(Float);
		}
		if (bSound)
		{
			Play(BIB::Synth::HitSound(Event.Side == 0 ? 0.5 : -0.5, Event.Action != BIB::Arena::EAction::Attack), SfxVolume * 0.7f);
		}
	}

	void StepArena(double Dt)
	{
		if (bShowResult)
		{
			return;
		}
		ReplayTime += Dt;
		while (NextEvent < static_cast<int32>(Battle.Events.size()) && Battle.Events[static_cast<size_t>(NextEvent)].Time <= ReplayTime)
		{
			ApplyBattleEvent(Battle.Events[static_cast<size_t>(NextEvent++)], true);
		}
		if (NextEvent >= static_cast<int32>(Battle.Events.size()) && ReplayTime > Battle.Duration + 1.0)
		{
			ResolveArena();
		}
	}

	// ------------------------------------------------------------------------------------------ results
	void ResolveNight(const FString& Forced)
	{
		if (bResolved)
		{
			return;
		}
		bResolved = true;
		std::string Text;
		if (Instance.IsValid())
		{
			BIB::Arcade::FNightShift Copy = Night;
			if (!Forced.IsEmpty() && !Copy.IsOver())
			{
				// A forfeit counts as a loss at the current hour.
				Text = BIB::Arcade::FinishNight(Instance->GetProfile(), Copy);
				const size_t RewardAt = Text.find("Reward:");
				Text = ToStd(Forced) + " " + (RewardAt == std::string::npos ? Text : Text.substr(RewardAt));
			}
			else
			{
				Text = BIB::Arcade::FinishNight(Instance->GetProfile(), Copy);
			}
			Instance->SaveNow();
		}
		ResultGood = Night.IsWon();
		ResultTitle = Night.IsWon() ? TEXT("6 AM - YOU SURVIVED") : TEXT("GAME OVER");
		ResultText = ToF(Text);
		bShowResult = true;
		StopAllSounds();
		if (Night.IsWon())
		{
			Play(BIB::Synth::Chime(), SfxVolume);
		}
	}

	void ResolveRhythm(bool bQuit)
	{
		if (bResolved)
		{
			return;
		}
		bResolved = true;
		BIB::Arcade::FRhythm Copy = Song;
		if (bQuit && !Copy.IsOver())
		{
			// Quitting fails the song: run the clock out without playing.
			while (!Copy.IsOver())
			{
				Copy.Tick(0.5);
			}
		}
		std::string Text;
		if (Instance.IsValid())
		{
			Text = BIB::Arcade::FinishRhythm(Instance->GetProfile(), Copy);
			Instance->SaveNow();
		}
		ResultGood = Copy.IsPassed();
		ResultTitle = Copy.IsPassed() ? FString::Printf(TEXT("CLEARED  %s"), *FString::ChrN(Copy.Stars(), TEXT('*'))) : TEXT("SONG FAILED");
		ResultText = ToF(Text);
		bShowResult = true;
		StopAllSounds();
	}

	void ResolveArena()
	{
		if (bShowResult)
		{
			return;
		}
		ResultGood = Battle.bAttackerWon;
		ResultTitle = Battle.bAttackerWon ? TEXT("VICTORY") : TEXT("DEFEAT");
		ResultText = Instance.IsValid() ? ToF(Instance->UiState.Status) : FString();
		bShowResult = true;
	}

	void ResolveMini(bool bQuit)
	{
		if (bResolved)
		{
			return;
		}
		bResolved = true;
		std::string Text;
		bool bWon = false;
		int32 Stars = 0;
		// Quitting ends the game as a loss: the copy's clock runs out without playing.
		switch (Kind)
		{
		case EBIBOverlayKind::HaloFall:
		{
			BIB::Arcade::FHaloFall Copy = Halo;
			Copy.SetFiring(false);
			Copy.SetMove(0);
			for (int32 Guard = 0; Guard < 400 && !Copy.IsOver(); ++Guard)
			{
				Copy.Tick(5.0);
			}
			if (Instance.IsValid())
			{
				Text = BIB::Arcade::FinishHalo(Instance->GetProfile(), Copy);
			}
			bWon = Copy.IsWon();
			Stars = Copy.Stars();
			break;
		}
		case EBIBOverlayKind::NeonMemory:
		{
			BIB::Arcade::FNeonMemory Copy = Memory;
			if (bQuit || !Copy.IsOver())
			{
				Copy.Tick(100000.0);
			}
			if (Instance.IsValid())
			{
				Text = BIB::Arcade::FinishMemory(Instance->GetProfile(), Copy);
			}
			bWon = Copy.IsWon();
			Stars = Copy.Stars();
			break;
		}
		case EBIBOverlayKind::FirewallBreach:
		{
			BIB::Arcade::FFirewallBreach Copy = Breach;
			for (int32 Guard = 0; Guard < 100 && !Copy.IsOver(); ++Guard)
			{
				Copy.Tick(30.0);
			}
			if (Instance.IsValid())
			{
				Text = BIB::Arcade::FinishBreach(Instance->GetProfile(), Copy);
			}
			bWon = Copy.IsWon();
			Stars = Copy.Stars();
			break;
		}
		case EBIBOverlayKind::ArkRally:
		{
			BIB::Arcade::FArkRally Copy = Rally;
			Copy.SetThrottle(0);
			Copy.SetLean(0);
			if (bQuit || !Copy.IsOver())
			{
				Copy.Tick(Copy.TimeLimit() + 1.0);   // giving up: the clock runs out
			}
			if (Instance.IsValid())
			{
				Text = BIB::Arcade::FinishRally(Instance->GetProfile(), Copy);
			}
			bWon = Copy.IsWon();
			Stars = Copy.Stars();
			break;
		}
		default:
			break;
		}
		if (Instance.IsValid())
		{
			Instance->SaveNow();
		}
		ResultGood = bWon;
		ResultTitle = bWon ? FString::Printf(TEXT("CLEARED  %s"), *FString::ChrN(Stars, TEXT('*')))
			: (Kind == EBIBOverlayKind::NeonMemory || Kind == EBIBOverlayKind::ArkRally ? FString(TEXT("TIME UP"))
				: (Kind == EBIBOverlayKind::FirewallBreach ? FString(TEXT("TRACED - ACCESS DENIED")) : FString(TEXT("THE CITY FELL"))));
		ResultText = ToF(Text);
		bShowResult = true;
		StopAllSounds();
		if (bWon)
		{
			Play(BIB::Synth::Chime(), SfxVolume);
		}
	}

	void Finish()
	{
		if (bClosed)
		{
			return;
		}
		if (!bResolved && IsMini())
		{
			ResolveMini(true);
		}
		if (!bResolved && Kind == EBIBOverlayKind::NightShift)
		{
			ResolveNight(TEXT("You left your post."));
		}
		if (!bResolved && Kind == EBIBOverlayKind::UltimateNight)
		{
			ResolveUcn(TEXT("You left your post."));
		}
		if (!bResolved && Kind == EBIBOverlayKind::Rhythm)
		{
			ResolveRhythm(true);
		}
		bClosed = true;
		StopAllSounds();
		if (OnDone)
		{
			OnDone(ResultText);
		}
		if (RequestClose)
		{
			RequestClose();
		}
	}

	// ------------------------------------------------------------------------------------------ painting: Night Shift
	void PaintDoor(const FPainter& Paint, const FVector2D& Pos, const FVector2D& Size, int32 Side) const
	{
		const BIB::Arcade::ESide S = Side == 0 ? BIB::Arcade::ESide::Left : BIB::Arcade::ESide::Right;
		FVector2D At = Pos;
		if (!bReduceMotion && Night.IsKuuWindingUp(S))
		{
			At.X += FMath::Sin(static_cast<float>(Clock) * 40.f) * 4.f;   // the door shudders under Kuu's thumps
		}
		const bool bLight = Night.IsLightOn(S);
		Paint.Box(At, Size, bLight ? FLinearColor(0.35f, 0.32f, 0.25f, 1.f) : FLinearColor(0.02f, 0.02f, 0.04f, 1.f));
		if (bLight)
		{
			const std::vector<BIB::Arcade::ENightChar> There = Night.AtDoor(S);
			float Y = At.Y + Size.Y * 0.12f;
			for (BIB::Arcade::ENightChar Char : There)
			{
				const float W = Size.X * 0.8f;
				Paint.Image(Portrait(ToF(BIB::Arcade::NightCharPortrait(Char))), FVector2D(At.X + (Size.X - W) * 0.5f, Y), FVector2D(W, W));
				Paint.Text(ToF(BIB::Arcade::NightCharName(Char)), FVector2D(At.X + Size.X * 0.5f, Y + W + 2.f), 13, Cream, 1);
				Y += W + 26.f;
			}
		}
		if (Night.IsDoorClosed(S))
		{
			Paint.Box(At, Size, FLinearColor(0.30f, 0.32f, 0.38f, 0.97f));
			for (float Stripe = At.Y + 20.f; Stripe < At.Y + Size.Y; Stripe += 40.f)
			{
				Paint.Box(FVector2D(At.X, Stripe), FVector2D(Size.X, 6.f), FLinearColor(0.2f, 0.21f, 0.25f, 1.f));
			}
		}
		if (Night.IsDoorBroken(S))
		{
			Paint.Frame(At, Size, Danger, 4.f);
			Paint.Text(FString::Printf(TEXT("BROKEN %.0fs"), Night.DoorRepairLeft(S)), FVector2D(At.X + Size.X * 0.5f, At.Y + Size.Y * 0.5f), 14, Danger, 1);
		}
		else
		{
			Paint.Frame(At, Size, FLinearColor(1.f, 1.f, 1.f, 0.25f), 2.f);
		}
		Hit(At, Size, FString::Printf(TEXT("door:%d"), Side));
	}

	void PaintNight(const FPainter& Paint, const FVector2D& Size) const
	{
		const BIB::FSettings Settings = Instance.IsValid() ? Instance->GetProfile().Meta.Settings : BIB::FSettings();
		// Top bar: clock, night, power and usage.
		Paint.Text(ToF(Night.ClockText()), FVector2D(28.f, 18.f), 34, Cream);
		Paint.Text(FString::Printf(TEXT("NIGHT SHIFT - %s"), *ToF(BIB::Arcade::DifficultyName(Night.GetDifficulty())).ToUpper()), FVector2D(30.f, 66.f), 13, Pink);
		Paint.Text(FString::Printf(TEXT("Power left: %d%%"), FMath::CeilToInt(Night.Power())), FVector2D(Size.X - 28.f, 20.f), 22,
			Night.Power() < 20.0 ? Danger : Ink, 2);
		const int32 Usage = FMath::Min(5, Night.Usage());
		Paint.Text(TEXT("Usage"), FVector2D(Size.X - 210.f, 56.f), 13, Dim);
		for (int32 Bar = 0; Bar < 5; ++Bar)
		{
			const FLinearColor Colour = Bar < Usage ? (Bar < 2 ? FLinearColor(0.3f, 0.85f, 0.4f, 1.f) : (Bar < 4 ? Gold : Danger)) : FLinearColor(1.f, 1.f, 1.f, 0.1f);
			Paint.Box(FVector2D(Size.X - 150.f + Bar * 24.f, 56.f), FVector2D(18.f, 22.f), Colour);
		}
		if (Night.IsPowerOut())
		{
			Paint.Box(FVector2D(0.f, 100.f), FVector2D(Size.X, Size.Y - 100.f), FLinearColor(0.f, 0.f, 0.f, 0.85f));
			Paint.Text(TEXT("The power is out..."), FVector2D(Size.X * 0.5f, Size.Y * 0.45f), 26, Dim, 1);
		}
		else if (Night.IsCameraUp())
		{
			PaintCameras(Paint, Size);
		}
		else
		{
			const FVector2D DoorSize(Size.X * 0.17f, Size.Y * 0.62f);
			PaintDoor(Paint, FVector2D(Size.X * 0.04f, Size.Y * 0.2f), DoorSize, 0);
			PaintDoor(Paint, FVector2D(Size.X * 0.79f, Size.Y * 0.2f), DoorSize, 1);
			// The desk between the doors.
			Paint.Box(FVector2D(Size.X * 0.25f, Size.Y * 0.62f), FVector2D(Size.X * 0.5f, Size.Y * 0.2f), Panel);
			Paint.Text(TEXT("BOLD"), FVector2D(Size.X * 0.5f - 90.f, Size.Y * 0.3f), 40, Pink);
			Paint.Text(TEXT("in"), FVector2D(Size.X * 0.5f + 18.f, Size.Y * 0.3f + 12.f), 24, Cream);
			Paint.Text(TEXT("BEAUTY"), FVector2D(Size.X * 0.5f - 90.f, Size.Y * 0.3f + 48.f), 40, Ink);
			const FVector2D Btn(Size.X * 0.105f, 64.f);
			const float Y = Size.Y * 0.665f;
			Button(Paint, FVector2D(Size.X * 0.26f, Y), Btn, TEXT("DOOR"), KeyLabel(Settings, BIB::NightBindFirst), TEXT("door:0"), Night.IsDoorClosed(BIB::Arcade::ESide::Left), Pink);
			Button(Paint, FVector2D(Size.X * 0.375f, Y), Btn, TEXT("LIGHT"), TEXT("hold ") + KeyLabel(Settings, BIB::NightBindFirst + 2), TEXT("light:0"),
				Night.IsLightOn(BIB::Arcade::ESide::Left), Cream);
			Button(Paint, FVector2D(Size.X * 0.52f, Y), Btn, TEXT("LIGHT"), TEXT("hold ") + KeyLabel(Settings, BIB::NightBindFirst + 3), TEXT("light:1"),
				Night.IsLightOn(BIB::Arcade::ESide::Right), Cream);
			Button(Paint, FVector2D(Size.X * 0.635f, Y), Btn, TEXT("DOOR"), KeyLabel(Settings, BIB::NightBindFirst + 1), TEXT("door:1"), Night.IsDoorClosed(BIB::Arcade::ESide::Right), Pink);
		}
		if (!Night.IsPowerOut())
		{
			Button(Paint, FVector2D(Size.X * 0.36f, Size.Y - 70.f), FVector2D(Size.X * 0.28f, 54.f), Night.IsCameraUp() ? TEXT("LOWER CAMERA") : TEXT("CAMERA"),
				KeyLabel(Settings, BIB::NightBindFirst + 4), TEXT("cam"), Night.IsCameraUp(), Cyan);
		}
		// Sound captions (Settings > Gameplay), off by default: the sounds are the cue.
		if (bCaptions)
		{
			for (int32 Side = 0; Side < 2; ++Side)
			{
				if (Clock < CaptionUntil[Side])
				{
					Paint.Text((Side == 0 ? TEXT("<< ") : TEXT("")) + Caption[Side] + (Side == 1 ? TEXT(" >>") : TEXT("")),
						FVector2D(Side == 0 ? 40.f : Size.X - 40.f, Size.Y * 0.14f), 18, Gold, Side == 0 ? 0 : 2);
				}
			}
		}
		if (bQuitArmed)
		{
			Paint.Text(TEXT("Press Esc again to give up the night"), FVector2D(Size.X * 0.5f, 92.f), 14, Danger, 1);
		}
		if (Night.IsOver() && !Night.IsWon() && Clock < JumpscareUntil)
		{
			const float Shake = bReduceMotion ? 0.f : FMath::Sin(static_cast<float>(Clock) * 60.f) * 12.f;
			Paint.Box(FVector2D::ZeroVector, Size, FLinearColor(0.35f, 0.f, 0.02f, 0.9f));
			const float Big = FMath::Min(Size.X, Size.Y) * 0.8f;
			Paint.Image(Portrait(ToF(BIB::Arcade::NightCharPortrait(Night.Killer()))), FVector2D((Size.X - Big) * 0.5f + Shake, (Size.Y - Big) * 0.5f), FVector2D(Big, Big));
		}
	}

	void PaintCameras(const FPainter& Paint, const FVector2D& Size) const
	{
		// Left: the feed of the selected camera. Right: the map with the ten cameras.
		const FVector2D FeedPos(Size.X * 0.03f, Size.Y * 0.16f);
		const FVector2D FeedSize(Size.X * 0.55f, Size.Y * 0.66f);
		Paint.Box(FeedPos, FeedSize, FLinearColor(0.05f, 0.07f, 0.06f, 1.f));
		const BIB::Arcade::ERoom Room = static_cast<BIB::Arcade::ERoom>(Night.SelectedCamera());
		const std::vector<BIB::Arcade::ENightChar> There = Night.VisibleInRoom(Room);
		const float Count = FMath::Max(1.f, static_cast<float>(There.size()));
		const float W = FMath::Min(FeedSize.Y * 0.55f, FeedSize.X / Count * 0.8f);
		for (size_t Index = 0; Index < There.size(); ++Index)
		{
			const float X = FeedPos.X + FeedSize.X * (static_cast<float>(Index) + 0.5f) / Count - W * 0.5f;
			Paint.Image(Portrait(ToF(BIB::Arcade::NightCharPortrait(There[Index]))), FVector2D(X, FeedPos.Y + FeedSize.Y * 0.18f), FVector2D(W, W),
				FLinearColor(0.75f, 0.9f, 0.8f, 1.f));
		}
		// Static.
		const int32 Lines = bReduceMotion ? 6 : 26;
		for (int32 Line = 0; Line < Lines; ++Line)
		{
			const float Y = FeedPos.Y + FMath::FRand() * FeedSize.Y;
			Paint.Box(FVector2D(FeedPos.X, Y), FVector2D(FeedSize.X, 1.f + FMath::FRand() * 2.f), FLinearColor(1.f, 1.f, 1.f, 0.05f + FMath::FRand() * 0.08f));
		}
		Paint.Frame(FeedPos, FeedSize, Cyan, 2.f);
		Paint.Text(FString::Printf(TEXT("CAM %d - %s"), Night.SelectedCamera() + 1 == 10 ? 0 : Night.SelectedCamera() + 1, *ToF(BIB::Arcade::RoomName(Room)).ToUpper()),
			FVector2D(FeedPos.X + 14.f, FeedPos.Y + 10.f), 16, Cream);
		if (FMath::Fmod(static_cast<float>(Clock), 1.f) < 0.5f)
		{
			Paint.Box(FVector2D(FeedPos.X + FeedSize.X - 34.f, FeedPos.Y + 14.f), FVector2D(14.f, 14.f), Danger);
		}
		const FVector2D MapPos(Size.X * 0.61f, Size.Y * 0.16f);
		const FVector2D MapSize(Size.X * 0.36f, Size.Y * 0.66f);
		Paint.Box(MapPos, MapSize, Panel);
		auto Where = [&MapPos, &MapSize](BIB::Arcade::ERoom R)
		{
			double X = 0.0;
			double Y = 0.0;
			BIB::Arcade::RoomMapPosition(R, X, Y);
			return FVector2D(MapPos.X + static_cast<float>(X) * MapSize.X, MapPos.Y + static_cast<float>(Y) * MapSize.Y);
		};
		using BIB::Arcade::ERoom;
		const ERoom Links[][2] = {{ERoom::Stage, ERoom::Dining}, {ERoom::Dining, ERoom::Backroom}, {ERoom::Dining, ERoom::Storage}, {ERoom::Dining, ERoom::WestHall},
			{ERoom::Storage, ERoom::WestHall}, {ERoom::WestHall, ERoom::WestCorner}, {ERoom::WestCorner, ERoom::LeftDoor}, {ERoom::Dining, ERoom::Kitchen},
			{ERoom::Dining, ERoom::Restrooms}, {ERoom::Dining, ERoom::EastHall}, {ERoom::Kitchen, ERoom::EastHall}, {ERoom::EastHall, ERoom::EastCorner},
			{ERoom::EastCorner, ERoom::RightDoor}, {ERoom::Backroom, ERoom::WestHall}, {ERoom::Restrooms, ERoom::Kitchen}};
		for (const auto& Link : Links)
		{
			Paint.Line(Where(Link[0]), Where(Link[1]), FLinearColor(1.f, 1.f, 1.f, 0.18f), 3.f);
		}
		Paint.Box(Where(ERoom::Inside) - FVector2D(30.f, 10.f), FVector2D(60.f, 18.f), Pink);
		Paint.Text(TEXT("YOU"), Where(ERoom::Inside) - FVector2D(0.f, 10.f), 11, Void, 1);
		for (int32 Camera = 0; Camera < BIB::Arcade::CameraCount; ++Camera)
		{
			const FVector2D Centre = Where(static_cast<ERoom>(Camera));
			const FVector2D BoxSize(74.f, 34.f);
			const bool bSelected = Camera == Night.SelectedCamera();
			Paint.Box(Centre - BoxSize * 0.5f, BoxSize, bSelected ? Cyan : FLinearColor(0.12f, 0.12f, 0.18f, 1.f));
			Paint.Text(FString::Printf(TEXT("CAM %d"), Camera + 1 == 10 ? 0 : Camera + 1), Centre - FVector2D(0.f, 9.f), 11, bSelected ? Void : Ink, 1);
			Hit(Centre - BoxSize * 0.5f, BoxSize, FString::Printf(TEXT("sel:%d"), Camera));
		}
	}

	// ------------------------------------------------------------------------------------------ Ultimate Custom Night
	// Rules: BIBRules/BIBUltimateNight.h. Placeholder visuals in the Night Shift look (operator heads, boxes, text); every sound is
	// generated with BIBSynth. Every deadly threat also has a visual sign here (edge pulse, door occupant, office object), so a
	// player with the sound off still gets a telegraph.
	FString UcnClockText() const
	{
		const int32 Hour = Ucn.Hour();
		return FString::Printf(TEXT("%d AM"), Hour == 0 ? 12 : Hour);
	}

	void UcnSay(const FString& Text, double Seconds, const FLinearColor& Colour)
	{
		UcnNote = Text;
		UcnNoteUntil = Clock + Seconds;
		UcnNoteColour = Colour;
	}

	static FString UcnName(int32 Id)
	{
		return ToF(BUcn::SubjectInfo(Id).Name);
	}

	void ClickUcn(const FString& Action)
	{
		if (Action == TEXT("quit"))
		{
			if (bQuitArmed)
			{
				ResolveUcn(TEXT("You left your post at ") + UcnClockText() + TEXT("."));
			}
			bQuitArmed = true;
			return;
		}
		bQuitArmed = false;
		if (Action.StartsWith(TEXT("ucn:door:")))
		{
			const BIB::Arcade::ESide Side = Action.EndsWith(TEXT("0")) ? BIB::Arcade::ESide::Left : BIB::Arcade::ESide::Right;
			if (Ucn.ToggleDoor(Side))
			{
				Play(BIB::Synth::DoorSlam(Side == BIB::Arcade::ESide::Left ? -0.8 : 0.8), SfxVolume);
			}
			else
			{
				Play(BIB::Synth::MissBuzz(), SfxVolume * 0.6f);   // broken, or no power
			}
			return;
		}
		if (Action == TEXT("ucn:window"))
		{
			if (Ucn.ToggleWindow())
			{
				Play(BIB::Synth::DoorSlam(0.0), SfxVolume * 0.8f);
			}
			return;
		}
		if (Action == TEXT("ucn:cam"))
		{
			Ucn.ToggleCamera();
			Play(BIB::Synth::CameraBlip(), SfxVolume * 0.6f);
			return;
		}
		if (Action.StartsWith(TEXT("ucn:sel:")))
		{
			Ucn.SelectCamera(FCString::Atoi(*Action.RightChop(8)));
			if (Ucn.IsCameraUp())
			{
				Play(BIB::Synth::CameraBlip(), SfxVolume * 0.4f);
			}
			return;
		}
		if (Action == TEXT("ucn:shock"))
		{
			if (Ucn.Shock())
			{
				UcnShockFlash = Clock;
				Play(BIB::Synth::Tone(1800.0, 0.18, 0.0, 0.5, BIB::Synth::EWave::Noise), SfxVolume);
			}
			else
			{
				Play(BIB::Synth::MissBuzz(), SfxVolume * 0.6f);   // no charge left on this camera, or the camera is down
			}
			return;
		}
		if (Action == TEXT("ucn:vaccine"))
		{
			if (Ucn.ClickVaccine())
			{
				Play(BIB::Synth::Tone(1046.5, 0.25, 0.0, 0.4), SfxVolume);
				UcnSay(TEXT("Vaccine taken: the next infection is blocked"), 2.0, FLinearColor(0.3f, 0.95f, 0.5f, 1.f));
			}
			return;
		}
		if (Action == TEXT("ucn:ad"))
		{
			if (Ucn.CloseAd())
			{
				Play(BIB::Synth::CameraBlip(), SfxVolume * 0.5f);
			}
			return;
		}
		if (Action == TEXT("ucn:urgent"))
		{
			// Keyboard players: click the office object that is due first (lethal ones first).
			int32 Best = -1;
			double BestDeadline = 1e18;
			bool bBestLethal = false;
			for (const BUcn::FDuty& Duty : Ucn.Duties())
			{
				if (!Duty.bOpen || Duty.Kind != BUcn::EDuty::ClickOffice || (Duty.bLethal && Duty.Subject == BUcn::NarratorId))
				{
					continue;
				}
				if ((Duty.bLethal && !bBestLethal) || (Duty.bLethal == bBestLethal && Duty.Deadline < BestDeadline))
				{
					Best = Duty.Id;
					BestDeadline = Duty.Deadline;
					bBestLethal = Duty.bLethal;
				}
			}
			if (Best >= 0)
			{
				Click(FString::Printf(TEXT("ucn:click:%d"), Best), true);
			}
			return;
		}
		if (Action.StartsWith(TEXT("ucn:click:")))
		{
			if (Ucn.ClickOffice(FCString::Atoi(*Action.RightChop(10))))
			{
				Play(BIB::Synth::Tone(740.0, 0.05, 0.0, 0.35, BIB::Synth::EWave::Square), SfxVolume);
			}
			return;
		}
	}

	void StepUcn(double Dt)
	{
		if (Ucn.IsOver())
		{
			if (!bResolved && Clock >= JumpscareUntil)
			{
				ResolveUcn(FString());
			}
			return;
		}
		Ucn.Tick(Dt);
		for (const BUcn::FEvent& Event : Ucn.PopEvents())
		{
			PlayUcnEvent(Event);
		}
		if (UcnBot.IsValid())
		{
			UcnBot->Act(Ucn);
		}
		// Kuu's loud 4-second charge: heavy thumps from her side, faster than Night Shift's.
		const int32 KuuDoor = Ucn.KuuSideInUse();
		if (KuuDoor >= 0 && Clock >= NextThump)
		{
			Play(BIB::Synth::KuuThump(SideOf(KuuDoor) * 0.9f), SfxVolume);
			NextThump = Clock + 0.5;
		}
	}

	void PlayUcnEvent(const BUcn::FEvent& Event)
	{
		using BUcn::EEvent;
		using BIB::Synth::EWave;
		switch (Event.Kind)
		{
		case EEvent::Telegraph:
		{
			const BUcn::EDuty Duty = static_cast<BUcn::EDuty>(Event.B);
			const bool bWindow = Duty == BUcn::EDuty::CloseDoor && Event.C == BUcn::WindowSide;
			const double Pan = bWindow ? 0.0 : SideOf(Event.C) * 0.95;   // the middle window is heard centred (owner: Diano)
			if (Duty == BUcn::EDuty::CloseDoor)
			{
				// Door threats are heard from their side (owner: Media and Myrcia pan to that side).
				switch (Event.A)
				{
				case BUcn::MediaId: Play(BIB::Synth::MediaCue(Pan), SfxVolume); break;
				case BUcn::MyrciaId: Play(BIB::Synth::Tone(196.0, 0.45, Pan, 0.7, EWave::Saw), SfxVolume); break;
				case BUcn::DeathId: Play(BIB::Synth::Tone(82.4, 0.5, Pan, 0.8, EWave::Square), SfxVolume); break;
				case BUcn::FujitaId: Play(BIB::Synth::Tone(329.6, 0.7, Pan, 0.45, EWave::Triangle), SfxVolume); break;
				case BUcn::NarratorId: Play(BIB::Synth::Tone(277.2, 0.4, Pan, 0.75, EWave::Saw), SfxVolume); break;   // Diano
				case BUcn::BloodId: Play(BIB::Synth::Tone(55.0, 0.6, 0.0, 0.8, EWave::Square), SfxVolume); break;
				case BUcn::WeatherId: Play(BIB::Synth::Tone(1760.0, 0.5, 0.0, 0.15, EWave::Triangle), SfxVolume * 0.6f); break;   // a faint shimmer
				default: Play(BIB::Synth::Tone(130.8, 0.35, Pan, 0.3), SfxVolume); break;   // footsteps stop: someone in the doorway
				}
				if (bWindow)
				{
					if (Event.A == BUcn::NarratorId)
					{
						UcnSay(TEXT("Diano at the WINDOW - shut it!"), 1.0, Danger);
					}
				}
				else
				{
					SetCaption(Event.C, (Event.A == BUcn::NarratorId ? FString(TEXT("Diano")) : UcnName(Event.A)) + TEXT(" - close it!"), FMath::Min(3.0, Event.D / 1000.0));
				}
			}
			else if (Duty == BUcn::EDuty::OpenDoor)
			{
				UcnSay(TEXT("Kuu is charging a punch - leave that door OPEN"), 4.0, Gold);
			}
			else if (Duty == BUcn::EDuty::PullCamera)
			{
				Play(BIB::Synth::Tone(1567.98, 0.3, 0.0, 0.6), SfxVolume);
				UcnSay(TEXT("ANGEL is in the office - pull the cameras up!"), 1.5, Danger);
			}
			else if (Duty == BUcn::EDuty::ClickOffice && Event.A == BUcn::DeathId)
			{
				Play(BIB::Synth::Tone(65.4, 0.4, 0.0, 0.9, EWave::Noise), SfxVolume);
				UcnSay(TEXT("DEATH is in the office - click her 4 times!"), 2.0, Danger);
			}
			else if (Duty == BUcn::EDuty::ClickOffice && Event.A == BUcn::NarratorId)
			{
				if (Event.D < 100000)
				{
					Play(BIB::Synth::Tone(880.0, 0.3, 0.0, 0.5, EWave::Square), SfxVolume);
					UcnSay(TEXT("A THIRD camera is coming - clear one now!"), 3.0, Danger);
				}
				else
				{
					Play(BIB::Synth::CameraBlip(), SfxVolume);
				}
			}
			else if (Duty == BUcn::EDuty::ShockGambler)
			{
				Play(BIB::Synth::Tone(987.8, 0.12, 0.0, 0.3, EWave::Triangle), SfxVolume * 0.7f);
			}
			break;
		}
		case EEvent::Knock:
			Play(BIB::Synth::Knock(Event.B == BUcn::WindowSide ? 0.f : SideOf(Event.B) * 0.95f), SfxVolume);
			break;
		case EEvent::BloodPhase:
			if (Event.A > 0)
			{
				Play(BIB::Synth::Tone(110.0 * Event.A, 0.25, 0.0, 0.4 + 0.1 * Event.A, EWave::Sine), SfxVolume);
				UcnSay(Event.A < 4 ? FString::Printf(TEXT("Blood at the window - phase %d/4: keep it OPEN"), Event.A) : FString(TEXT("Blood is closest - SHUT THE WINDOW")),
					Event.A < 4 ? 2.0 : 3.0, Event.A < 4 ? Gold : Danger);
			}
			break;
		case EEvent::Weather:
			break;   // detecting him is the challenge: his aura is the sign, plus a faint shimmer on arrival
		case EEvent::Summoned:
			Play(BIB::Synth::Tone(73.4, 1.5, 0.0, 0.8, EWave::Saw), SfxVolume);
			UcnSay(TEXT("Death has taken Plack's place"), 4.0, Danger);
			break;
		case EEvent::KuuWindup:
			SetCaption(Event.A, TEXT("KUU: charging a punch..."), BUcn::FUcnTuning().KuuWindup);
			NextThump = Clock;
			break;
		case EEvent::KuuPunch:
			Play(BIB::Synth::Punch(SideOf(Event.A) * 0.95f, Event.B == 1), SfxVolume);
			SetCaption(Event.A, Event.B == 1 ? TEXT("CRASH - door broken") : TEXT("whoosh - missed"), 2.0);
			break;
		case EEvent::DoorBroken:
			break;
		case EEvent::DoorRepaired:
			SetCaption(Event.A, TEXT("door repaired"), 1.5);
			break;
		case EEvent::DoorForced:
		case EEvent::CameraForced:
			UcnSay(TEXT("Jester is messing with you"), 1.5, Purple);
			break;
		case EEvent::Infection:
		{
			// A small sound on that camera: louder when you are looking at it.
			const float Near = Ucn.IsViewing(Event.A) ? 1.f : 0.35f;
			Play(BIB::Synth::Tone(220.0, 0.25, 0.0, 0.35, EWave::Noise), SfxVolume * Near);
			if (Event.B == 1)
			{
				UcnSay(FString::Printf(TEXT("Alchemist's leak took CAM %d"), Event.A + 1 == 10 ? 0 : Event.A + 1), 2.5, FLinearColor(0.3f, 0.95f, 0.5f, 1.f));
			}
			break;
		}
		case EEvent::CameraLost:
			Play(BIB::Synth::Tone(150.0, 0.35, 0.0, 0.5, EWave::Square), SfxVolume * 0.7f);
			break;
		case EEvent::Vaccine:
			if (Event.B == 1)
			{
				UcnSay(TEXT("The vaccine stopped an infection"), 2.0, FLinearColor(0.3f, 0.95f, 0.5f, 1.f));
			}
			break;
		case EEvent::CoinFlip:
			// Owner: high pitch heads (find her again), low pitch tails (she leaves).
			Play(Event.A == 1 ? BIB::Synth::Tone(1396.9, 0.3, 0.0, 0.5) : BIB::Synth::Tone(261.6, 0.4, 0.0, 0.5), SfxVolume);
			UcnSay(Event.A == 1 ? TEXT("Heads - find the Gambler again") : TEXT("Tails - the Gambler left"), 2.0, Gold);
			break;
		case EEvent::Stargazer:
			if (Event.A == 0)
			{
				Play(BIB::Synth::Tone(523.25, 1.2, 0.0, 0.45, EWave::Triangle), SfxVolume);
				UcnSay(TEXT("STARGAZER is waking: CAM 1-7-9-5-3-6-4-8 - flash him"), 4.0, Purple);
			}
			else if (Event.A == 1)
			{
				Play(BIB::Synth::Tone(1318.5, 0.06, 0.0, 0.25, EWave::Sine), SfxVolume * 0.6f);
			}
			else if (Event.A == 2)
			{
				// Owner: from CAM 2 or 3 he smashes your front window.
				Play(BIB::Synth::Punch(0.0, true), SfxVolume);
				Play(BIB::Synth::Tone(98.0, 0.9, 0.0, 0.9, EWave::Saw), SfxVolume);
				UcnStarSmashAt = Clock;
				UcnSay(TEXT("STARGAZER IS AT THE FRONT WINDOW"), 1.5, Danger);
			}
			else
			{
				UcnSay(TEXT("Stargazer was sent back"), 1.5, Purple);
			}
			break;
		case EEvent::HourLost:
			Play(BIB::Synth::Tone(392.0, 0.8, 0.0, 0.6, EWave::Saw), SfxVolume);
			UcnSay(TEXT("Time Keeper sent you back an hour"), 3.0, Gold);
			break;
		case EEvent::TimeKeeper:
			if (Event.B == 0)
			{
				Play(BIB::Synth::Tone(2093.0, 0.05, 0.0, 0.3, EWave::Square), SfxVolume * 0.6f);   // tick: she is on some camera
			}
			break;
		case EEvent::MiniCamera:
			Play(BIB::Synth::CameraBlip(), SfxVolume);
			break;
		case EEvent::Ad:
			if (Event.A == 1)
			{
				Play(BIB::Synth::Tone(1174.7, 0.1, 0.0, 0.4, EWave::Square), SfxVolume * 0.6f);
			}
			break;
		case EEvent::Horn:
			Play(BIB::Synth::Tone(233.1, 1.0, 0.0, 0.9, EWave::Saw), SfxVolume);   // owner: a loud air horn over 1 second
			break;
		case EEvent::Judge:
			Play(BIB::Synth::MissBuzz(), SfxVolume);
			break;
		case EEvent::Sacrifice:
			Play(BIB::Synth::Tone(110.0, 1.4, 0.0, 0.6), SfxVolume);
			UcnSay(Event.A == 1 ? TEXT("5 AM - Plack's sacrifice: Death is gone") : TEXT("5 AM - Plack's sacrifice"), 4.0, Gold);
			break;
		case EEvent::PowerOut:
			Play(BIB::Synth::PowerDown(), SfxVolume);
			break;
		case EEvent::Jumpscare:
			Play(BIB::Synth::Jumpscare(), SfxVolume);
			JumpscareUntil = Clock + 1.6;
			break;
		case EEvent::Survived:
			Play(BIB::Synth::Chime(), SfxVolume);
			JumpscareUntil = Clock + 1.0;
			break;
		default:
			break;
		}
	}

	void ResolveUcn(const FString& Forced)
	{
		if (bResolved)
		{
			return;
		}
		bResolved = true;
		const bool bWon = Ucn.IsOver() && Ucn.IsWon();
		std::string Text;
		if (Instance.IsValid())
		{
			if (!Forced.IsEmpty() && !Ucn.IsOver())
			{
				// A forfeit counts as a loss at the current hour: never a reward.
				Text = ToStd(Forced) + " " + BUcn::FinishUcn(Instance->GetProfile(), Ucn.GetMode(), false, Ucn.Hour());
			}
			else
			{
				Text = BUcn::FinishUcn(Instance->GetProfile(), Ucn);
			}
			Instance->SaveNow();
		}
		ResultGood = bWon;
		ResultTitle = bWon ? TEXT("6 AM - YOU SURVIVED") : TEXT("GAME OVER");
		ResultText = ToF(Text);
		bShowResult = true;
		StopAllSounds();
		if (bWon)
		{
			Play(BIB::Synth::Chime(), SfxVolume);
		}
	}

	// ---- painting
	FString UcnFace(int32 Id) const
	{
		if (Id == BUcn::AngelId)
		{
			return FString();   // no portrait yet: drawn as a placeholder halo
		}
		if (Id == BUcn::NarratorId)
		{
			return TEXT("narrator");
		}
		return ToF(BUcn::SubjectInfo(Id).Name).ToLower().Replace(TEXT(" "), TEXT(""));
	}

	void PaintSubject(const FPainter& Paint, int32 Id, const FVector2D& Pos, float W, const FLinearColor& Tint) const
	{
		const FString Face = UcnFace(Id);
		const FSlateBrush* Brush = Face.IsEmpty() ? nullptr : Portrait(Face);
		if (Brush)
		{
			Paint.Image(Brush, Pos, FVector2D(W, W), Tint);
		}
		else
		{
			// PLACEHOLDER visual: a pale halo and the subject number.
			Paint.Box(Pos, FVector2D(W, W), FLinearColor(0.95f, 0.93f, 0.85f, 0.85f));
			Paint.Frame(Pos + FVector2D(W * 0.2f, W * 0.08f), FVector2D(W * 0.6f, W * 0.12f), Gold, 3.f);
			Paint.Text(FString::Printf(TEXT("%03d"), Id), FVector2D(Pos.X + W * 0.5f, Pos.Y + W * 0.42f), 18, Void, 1);
		}
		Paint.Text(UcnName(Id), FVector2D(Pos.X + W * 0.5f, Pos.Y + W + 2.f), 12, Cream, 1);
	}

	// A subject as a camera (or the window) shows it: Weather looks like another operator inside a neon yellow / light blue aura.
	void PaintCamSubject(const FPainter& Paint, int32 Id, const FVector2D& Pos, float W, const FLinearColor& Tint) const
	{
		if (Id == BUcn::WeatherId && Ucn.WeatherLooksLike() >= 0)
		{
			const FLinearColor Aura = Ucn.WeatherAura() == 0 ? FLinearColor(1.f, 0.95f, 0.15f, 0.55f) : FLinearColor(0.55f, 0.85f, 1.f, 0.55f);
			Paint.Box(Pos - FVector2D(8.f, 8.f), FVector2D(W + 16.f, W + 16.f), Aura);
			PaintSubject(Paint, Ucn.WeatherLooksLike(), Pos, W, Tint);
			return;
		}
		PaintSubject(Paint, Id, Pos, W, Tint);
	}

	// The front window: outside it is CAM 3 (owner: you can see Luna or Plack as they come over); Blood comes closer phase by
	// phase; Diano's middle attack; Weather in disguise.
	void PaintUcnWindow(const FPainter& Paint, const FVector2D& Pos, const FVector2D& Size) const
	{
		Paint.Box(Pos, Size, FLinearColor(0.04f, 0.05f, 0.1f, 1.f));
		TArray<int32> Outside;
		for (const int Id : Ucn.SubjectsOnCamera(BUcn::WindowCamera))
		{
			Outside.AddUnique(Id);
		}
		bool bDiano = false;
		for (const BUcn::FDuty& Duty : Ucn.Duties())
		{
			bDiano = bDiano || (Duty.bOpen && Duty.Kind == BUcn::EDuty::CloseDoor && Duty.Side == BUcn::WindowSide && Duty.Subject == BUcn::NarratorId);
		}
		const float Small = Size.Y * 0.42f;
		for (int32 Index = 0; Index < Outside.Num(); ++Index)
		{
			PaintCamSubject(Paint, Outside[Index], FVector2D(Pos.X + 12.f + Index * (Small + 18.f), Pos.Y + Size.Y - Small - 22.f), Small, FLinearColor(0.6f, 0.65f, 0.8f, 1.f));
		}
		const int32 Phase = Ucn.BloodPhaseNow();
		if (Phase > 0)
		{
			// Blood comes closer each phase: bigger and redder; the 4th is right at the glass.
			const float W = Size.Y * (0.3f + 0.15f * Phase);
			PaintSubject(Paint, BUcn::BloodId, FVector2D(Pos.X + (Size.X - W) * 0.5f, Pos.Y + Size.Y - W - 16.f), W, FLinearColor(0.6f + 0.1f * Phase, 0.3f, 0.3f, 1.f));
			Paint.Text(FString::Printf(TEXT("PHASE %d/4"), Phase), FVector2D(Pos.X + Size.X - 10.f, Pos.Y + 8.f), 13, Phase < 4 ? Gold : Danger, 2);
		}
		if (bDiano)
		{
			const float W = Size.Y * 0.8f;
			PaintSubject(Paint, BUcn::NarratorId, FVector2D(Pos.X + (Size.X - W) * 0.5f, Pos.Y + 4.f), W, FLinearColor::White);
		}
		if (Ucn.IsWindowClosed())
		{
			Paint.Box(Pos, Size, FLinearColor(0.30f, 0.32f, 0.38f, 0.95f));
			for (float Stripe = Pos.Y + 12.f; Stripe < Pos.Y + Size.Y; Stripe += 26.f)
			{
				Paint.Box(FVector2D(Pos.X, Stripe), FVector2D(Size.X, 5.f), FLinearColor(0.2f, 0.21f, 0.25f, 1.f));
			}
		}
		if (Clock - UcnStarSmashAt < 1.6)
		{
			// Stargazer through the glass (shut or not): cracks and his face.
			const float W = Size.Y * 0.9f;
			PaintSubject(Paint, BUcn::StargazerId, FVector2D(Pos.X + (Size.X - W) * 0.5f, Pos.Y + 2.f), W, FLinearColor::White);
			Paint.Line(Pos, Pos + Size, Cream, 3.f);
			Paint.Line(FVector2D(Pos.X + Size.X, Pos.Y), FVector2D(Pos.X, Pos.Y + Size.Y), Cream, 3.f);
			Paint.Line(FVector2D(Pos.X + Size.X * 0.5f, Pos.Y), FVector2D(Pos.X + Size.X * 0.35f, Pos.Y + Size.Y), Cream, 2.f);
		}
		Paint.Frame(Pos, Size, Ucn.IsWindowLocked() ? Gold : FLinearColor(1.f, 1.f, 1.f, 0.3f), 3.f);
		Paint.Text(TEXT("FRONT WINDOW (CAM 3 outside)"), FVector2D(Pos.X + 8.f, Pos.Y + 6.f), 11, Dim, 0, false);
		Hit(Pos, Size, TEXT("ucn:window"));
	}

	void PaintUcnDoor(const FPainter& Paint, const FVector2D& Pos, const FVector2D& Size, int32 Side) const
	{
		const BIB::Arcade::ESide S = Side == 0 ? BIB::Arcade::ESide::Left : BIB::Arcade::ESide::Right;
		const bool bKuu = Ucn.KuuSideInUse() == Side;
		FVector2D At = Pos;
		if (!bReduceMotion && bKuu)
		{
			At.X += FMath::Sin(static_cast<float>(Clock) * 40.f) * 5.f;   // the door shudders under the charge
		}
		Paint.Box(At, Size, FLinearColor(0.03f, 0.03f, 0.05f, 1.f));
		// Who is in the doorway (no lights in this mode: an arrival is always visible from the office).
		TArray<int32> There;
		for (const BUcn::FDuty& Duty : Ucn.Duties())
		{
			if (Duty.bOpen && Duty.Kind == BUcn::EDuty::CloseDoor && Duty.Side == Side)
			{
				There.AddUnique(Duty.Subject);
			}
		}
		if (bKuu)
		{
			There.AddUnique(BUcn::KuuId);
		}
		float Y = At.Y + Size.Y * 0.1f;
		for (const int32 Id : There)
		{
			const float W = Size.X * 0.7f;
			if (Id == BUcn::LunaId && !Ucn.IsDoorClosed(S))
			{
				// Owner: Luna's glowing eyes in the doorway.
				Paint.Box(FVector2D(At.X + Size.X * 0.32f, Y + W * 0.35f), FVector2D(Size.X * 0.1f, 6.f), Gold);
				Paint.Box(FVector2D(At.X + Size.X * 0.58f, Y + W * 0.35f), FVector2D(Size.X * 0.1f, 6.f), Gold);
			}
			PaintSubject(Paint, Id, FVector2D(At.X + (Size.X - W) * 0.5f, Y), W, FLinearColor(0.85f, 0.8f, 0.9f, 1.f));
			Y += W + 24.f;
		}
		if (Ucn.IsDoorClosed(S))
		{
			Paint.Box(At, Size, FLinearColor(0.30f, 0.32f, 0.38f, 0.93f));
			for (float Stripe = At.Y + 20.f; Stripe < At.Y + Size.Y; Stripe += 40.f)
			{
				Paint.Box(FVector2D(At.X, Stripe), FVector2D(Size.X, 6.f), FLinearColor(0.2f, 0.21f, 0.25f, 1.f));
			}
			for (int32 Index = 0; Index < There.Num(); ++Index)
			{
				Paint.Text(UcnName(There[Index]), FVector2D(At.X + Size.X * 0.5f, At.Y + 16.f + Index * 18.f), 13, Cream, 1);   // seen through the slats
			}
		}
		if (Ucn.DoorRepairLeft(S) > 0.0)
		{
			Paint.Frame(At, Size, Danger, 4.f);
			Paint.Text(FString::Printf(TEXT("BROKEN %.0fs"), Ucn.DoorRepairLeft(S)), FVector2D(At.X + Size.X * 0.5f, At.Y + Size.Y * 0.5f), 14, Danger, 1);
		}
		else
		{
			Paint.Frame(At, Size, FLinearColor(1.f, 1.f, 1.f, 0.25f), 2.f);
		}
		Hit(At, Size, FString::Printf(TEXT("ucn:door:%d"), Side));
	}

	// The office tray: Narrator's mini cameras and Death, clickable in either view (they are in your room).
	void PaintUcnOffice(const FPainter& Paint, const FVector2D& Size) const
	{
		float X = Size.X * 0.27f;
		const float Y = Size.Y * 0.835f - 66.f;
		for (const BUcn::FDuty& Duty : Ucn.Duties())
		{
			if (!Duty.bOpen || Duty.Kind != BUcn::EDuty::ClickOffice)
			{
				continue;
			}
			if (Duty.Subject == BUcn::NarratorId && Duty.bLethal)
			{
				const double Left = FMath::Max(0.0, Duty.Deadline - Ucn.Time());
				Paint.Text(FString::Printf(TEXT("THIRD CAMERA IN %.1fs"), Left), FVector2D(Size.X * 0.5f, Y - 26.f), 15, Danger, 1);
				continue;
			}
			const bool bDeath = Duty.Subject == BUcn::DeathId;
			const FVector2D Box(bDeath ? 120.f : 74.f, bDeath ? 120.f : 56.f);
			const FVector2D Pos(X, bDeath ? Y - 64.f : Y);
			if (bDeath)
			{
				PaintSubject(Paint, BUcn::DeathId, Pos, Box.X, FLinearColor::White);
				const float Frac = static_cast<float>(FMath::Clamp((Duty.Deadline - Ucn.Time()) / FMath::Max(0.01, Duty.Window), 0.0, 1.0));
				Paint.Box(FVector2D(Pos.X, Pos.Y - 10.f), FVector2D(Box.X * Frac, 6.f), Danger);
			}
			else
			{
				Paint.Box(Pos, Box, FLinearColor(0.12f, 0.12f, 0.18f, 1.f));
				Paint.Box(Pos + FVector2D(Box.X * 0.3f, Box.Y * 0.25f), FVector2D(Box.X * 0.4f, Box.Y * 0.4f), Cyan);   // a mini camera
			}
			Paint.Frame(Pos, Box, bDeath ? Danger : Cyan, 2.f);
			Paint.Text(FString::Printf(TEXT("x%d"), Duty.Clicks), FVector2D(Pos.X + Box.X - 6.f, Pos.Y + 2.f), 14, bDeath ? Danger : Cream, 2);
			Hit(Pos, Box, FString::Printf(TEXT("ucn:click:%d"), Duty.Id));
			X += Box.X + 14.f;
		}
	}

	void PaintUcn(const FPainter& Paint, const FVector2D& Size) const
	{
		const BIB::FSettings Settings = Instance.IsValid() ? Instance->GetProfile().Meta.Settings : BIB::FSettings();
		// Top bar: clock, mode, power, usage, shocks and cameras.
		Paint.Text(UcnClockText(), FVector2D(28.f, 18.f), 34, Cream);
		Paint.Text(FString::Printf(TEXT("ULTIMATE CUSTOM NIGHT - %s"), *ToF(BUcn::ModeName(Ucn.GetMode())).ToUpper()), FVector2D(30.f, 66.f), 13, Pink);
		if (Ucn.JudgeActive())
		{
			Paint.Text(TEXT("JUDGE IS WATCHING"), FVector2D(Size.X * 0.5f, 22.f), 13, Danger, 1);
		}
		Paint.Text(FString::Printf(TEXT("Power left: %d%%"), FMath::CeilToInt(Ucn.Power())), FVector2D(Size.X - 28.f, 20.f), 22, Ucn.Power() < 20.0 ? Danger : Ink, 2);
		Paint.Text(FString::Printf(TEXT("Shocks %d   Cameras %d/10"), Ucn.ShocksTotal(), Ucn.CamerasWorking()), FVector2D(Size.X - 230.f, 56.f), 13, Dim, 2);
		const int32 Usage = FMath::Min(5, Ucn.Usage());
		for (int32 Bar = 0; Bar < 5; ++Bar)
		{
			const FLinearColor Colour = Bar < Usage ? (Bar < 2 ? FLinearColor(0.3f, 0.85f, 0.4f, 1.f) : (Bar < 4 ? Gold : Danger)) : FLinearColor(1.f, 1.f, 1.f, 0.1f);
			Paint.Box(FVector2D(Size.X - 150.f + Bar * 24.f, 56.f), FVector2D(18.f, 22.f), Colour);
		}
		if (Ucn.IsPowerOut())
		{
			Paint.Box(FVector2D(0.f, 100.f), FVector2D(Size.X, Size.Y - 100.f), FLinearColor(0.f, 0.f, 0.f, 0.85f));
			Paint.Text(TEXT("The power is out..."), FVector2D(Size.X * 0.5f, Size.Y * 0.45f), 26, Dim, 1);
		}
		else if (Ucn.IsCameraUp())
		{
			PaintUcnCameras(Paint, Size);
			PaintUcnOffice(Paint, Size);
		}
		else
		{
			const FVector2D DoorSize(Size.X * 0.17f, Size.Y * 0.62f);
			PaintUcnDoor(Paint, FVector2D(Size.X * 0.04f, Size.Y * 0.2f), DoorSize, 0);
			PaintUcnDoor(Paint, FVector2D(Size.X * 0.79f, Size.Y * 0.2f), DoorSize, 1);
			Paint.Box(FVector2D(Size.X * 0.25f, Size.Y * 0.62f), FVector2D(Size.X * 0.5f, Size.Y * 0.2f), Panel);
			Paint.Text(TEXT("BOLD in BEAUTY"), FVector2D(Size.X * 0.5f, Size.Y * 0.765f), 18, FLinearColor(Pink.R, Pink.G, Pink.B, 0.6f), 1);
			PaintUcnWindow(Paint, FVector2D(Size.X * 0.33f, Size.Y * 0.13f), FVector2D(Size.X * 0.34f, Size.Y * 0.3f));
			// Angel in the office: a white figure between the doors.
			for (const BUcn::FDuty& Duty : Ucn.Duties())
			{
				if (Duty.bOpen && Duty.Kind == BUcn::EDuty::PullCamera)
				{
					const float W = Size.Y * 0.26f;
					PaintSubject(Paint, BUcn::AngelId, FVector2D(Size.X * 0.5f - W * 0.5f, Size.Y * 0.36f), W, FLinearColor::White);
					Paint.Text(FString::Printf(TEXT("PULL THE CAMERAS UP  %.1fs"), FMath::Max(0.0, Duty.Deadline - Ucn.Time())),
						FVector2D(Size.X * 0.5f, Size.Y * 0.36f + W + 22.f), 16, Danger, 1);
				}
			}
			const FVector2D Btn(Size.X * 0.105f, 64.f);
			const float Y = Size.Y * 0.665f;
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const BIB::Arcade::ESide S = Side == 0 ? BIB::Arcade::ESide::Left : BIB::Arcade::ESide::Right;
				const FVector2D At(Side == 0 ? Size.X * 0.26f : Size.X * 0.635f, Y);
				Button(Paint, At, Btn, TEXT("DOOR"), KeyLabel(Settings, BIB::NightBindFirst + Side), FString::Printf(TEXT("ucn:door:%d"), Side), Ucn.IsDoorClosed(S), Pink);
				if (!Ucn.IsDoorClosed(S) && Ucn.IsDoorFalseAlarm(S))
				{
					Paint.Text(TEXT("FALSE ALARM"), FVector2D(At.X + Btn.X * 0.5f, At.Y - 18.f), 11, Danger, 1);   // Judge's warning cue
				}
			}
			// The front window (owner 2026-10-14), between the door buttons.
			const FVector2D WinAt(Size.X * 0.4475f, Y);
			Button(Paint, WinAt, Btn, TEXT("WINDOW"), TEXT("W"), TEXT("ucn:window"), Ucn.IsWindowClosed(), Cyan);
			if (Ucn.IsWindowLocked())
			{
				Paint.Text(TEXT("DON'T SHUT YET"), FVector2D(WinAt.X + Btn.X * 0.5f, WinAt.Y - 18.f), 11, Gold, 1);
			}
			else if (!Ucn.IsWindowClosed() && Ucn.IsWindowFalseAlarm())
			{
				Paint.Text(TEXT("FALSE ALARM"), FVector2D(WinAt.X + Btn.X * 0.5f, WinAt.Y - 18.f), 11, Danger, 1);
			}
			PaintUcnOffice(Paint, Size);
		}
		if (!Ucn.IsPowerOut())
		{
			Button(Paint, FVector2D(Size.X * 0.36f, Size.Y - 70.f), FVector2D(Size.X * 0.28f, 54.f), Ucn.IsCameraUp() ? TEXT("LOWER CAMERA") : TEXT("CAMERA"),
				KeyLabel(Settings, BIB::NightBindFirst + 4), TEXT("ucn:cam"), Ucn.IsCameraUp(), Cyan);
		}
		// Visual telegraph in every view: a pulse on the side of each door threat, and the office threats in the centre.
		const float Pulse = bReduceMotion ? 0.7f : 0.45f + 0.35f * FMath::Abs(FMath::Sin(static_cast<float>(Clock) * 9.f));
		for (int32 Side = 0; Side < 2; ++Side)
		{
			FString Names;
			for (const BUcn::FDuty& Duty : Ucn.Duties())
			{
				if (Duty.bOpen && Duty.bLethal && Duty.Kind == BUcn::EDuty::CloseDoor && Duty.Side == Side)
				{
					Names += (Names.IsEmpty() ? TEXT("") : TEXT(" + ")) + UcnName(Duty.Subject);
				}
			}
			if (!Names.IsEmpty())
			{
				const bool bShut = Ucn.IsDoorClosed(Side == 0 ? BIB::Arcade::ESide::Left : BIB::Arcade::ESide::Right);
				const FLinearColor Edge = bShut ? FLinearColor(0.3f, 0.85f, 0.4f, 0.6f) : FLinearColor(Danger.R, Danger.G, Danger.B, Pulse);
				Paint.Box(FVector2D(Side == 0 ? 0.f : Size.X - 14.f, 100.f), FVector2D(14.f, Size.Y - 180.f), Edge);
				Paint.Text(Names, FVector2D(Side == 0 ? 22.f : Size.X - 22.f, 104.f), 13, bShut ? FLinearColor(0.3f, 0.85f, 0.4f, 1.f) : Danger, Side == 0 ? 0 : 2);
			}
		}
		{
			FString Names;
			for (const BUcn::FDuty& Duty : Ucn.Duties())
			{
				if (Duty.bOpen && Duty.bLethal && Duty.Kind == BUcn::EDuty::CloseDoor && Duty.Side == BUcn::WindowSide && Duty.Subject != BUcn::WeatherId)
				{
					Names += (Names.IsEmpty() ? TEXT("") : TEXT(" + ")) + (Duty.Subject == BUcn::NarratorId ? FString(TEXT("Diano")) : UcnName(Duty.Subject));
				}
			}
			if (!Names.IsEmpty())
			{
				// Weather is left out on purpose: spotting his aura is the challenge (he is on CAM 3 and at the window).
				const bool bShut = Ucn.IsWindowClosed();
				Paint.Box(FVector2D(Size.X * 0.3f, 96.f), FVector2D(Size.X * 0.4f, 8.f), bShut ? FLinearColor(0.3f, 0.85f, 0.4f, 0.6f) : FLinearColor(Danger.R, Danger.G, Danger.B, Pulse));
				Paint.Text(TEXT("WINDOW: ") + Names, FVector2D(Size.X * 0.5f, 106.f), 13, bShut ? FLinearColor(0.3f, 0.85f, 0.4f, 1.f) : Danger, 1);
			}
		}
		if (Ucn.IsAdOpen())
		{
			// Jester's advertisement over part of the screen (it covers the monitor: close it to use the cameras).
			const FVector2D AdPos(Size.X * 0.08f, Size.Y * 0.2f);
			const FVector2D AdSize(Size.X * 0.48f, Size.Y * 0.5f);
			Paint.Box(AdPos, AdSize, FLinearColor(0.98f, 0.85f, 0.2f, 1.f));
			Paint.Frame(AdPos, AdSize, Pink, 6.f);
			Paint.Text(TEXT("SPONSOR NIGHT!!"), FVector2D(AdPos.X + AdSize.X * 0.5f, AdPos.Y + AdSize.Y * 0.3f), 34, Pink, 1);
			Paint.Text(TEXT("Jester's deal of the hour - click X to close (key X)"), FVector2D(AdPos.X + AdSize.X * 0.5f, AdPos.Y + AdSize.Y * 0.55f), 14, Void, 1);
			const FVector2D Close(AdPos.X + AdSize.X - 52.f, AdPos.Y + 8.f);
			Paint.Box(Close, FVector2D(44.f, 44.f), Void);
			Paint.Text(TEXT("X"), Close + FVector2D(22.f, 8.f), 22, Cream, 1);
			Hit(AdPos, AdSize, TEXT("ucn:none"));     // the advert swallows clicks...
			Hit(Close, FVector2D(44.f, 44.f), TEXT("ucn:ad"));   // ...except its close box
		}
		if (Ucn.IsHorn())
		{
			Paint.Frame(FVector2D::ZeroVector, Size, Gold, 10.f);   // the air horn
		}
		if (Clock < UcnNoteUntil)
		{
			Paint.Text(UcnNote, FVector2D(Size.X * 0.5f, Size.Y * 0.135f), 18, UcnNoteColour, 1);
		}
		if (bCaptions)
		{
			for (int32 Side = 0; Side < 2; ++Side)
			{
				if (Clock < CaptionUntil[Side])
				{
					Paint.Text((Side == 0 ? TEXT("<< ") : TEXT("")) + Caption[Side] + (Side == 1 ? TEXT(" >>") : TEXT("")),
						FVector2D(Side == 0 ? 40.f : Size.X - 40.f, Size.Y * 0.17f), 16, Gold, Side == 0 ? 0 : 2);
				}
			}
		}
		if (bQuitArmed)
		{
			Paint.Text(TEXT("Press Esc again to give up the night"), FVector2D(Size.X * 0.5f, 92.f), 14, Danger, 1);
		}
		if (Ucn.IsOver() && !Ucn.IsWon() && Clock < JumpscareUntil)
		{
			const float Shake = bReduceMotion ? 0.f : FMath::Sin(static_cast<float>(Clock) * 60.f) * 12.f;
			Paint.Box(FVector2D::ZeroVector, Size, FLinearColor(0.35f, 0.f, 0.02f, 0.9f));
			const float Big = FMath::Min(Size.X, Size.Y) * 0.7f;
			if (Ucn.Killer() >= 0)
			{
				PaintSubject(Paint, Ucn.Killer(), FVector2D((Size.X - Big) * 0.5f + Shake, (Size.Y - Big) * 0.5f - 20.f), Big, FLinearColor::White);
			}
			Paint.Text(ToF(Ucn.LostReason()), FVector2D(Size.X * 0.5f, Size.Y - 60.f), 16, Cream, 1);
		}
	}

	void PaintUcnCameras(const FPainter& Paint, const FVector2D& Size) const
	{
		const int32 Cam = Ucn.SelectedCamera();
		const FVector2D FeedPos(Size.X * 0.03f, Size.Y * 0.16f);
		const FVector2D FeedSize(Size.X * 0.55f, Size.Y * 0.56f);
		Paint.Box(FeedPos, FeedSize, FLinearColor(0.05f, 0.07f, 0.06f, 1.f));
		const BIB::Arcade::ERoom Room = static_cast<BIB::Arcade::ERoom>(Cam);
		if (Ucn.IsCameraDisabled(Cam))
		{
			Paint.Box(FeedPos, FeedSize, FLinearColor(0.1f, 0.1f, 0.1f, 1.f));
			Paint.Text(TEXT("NO SIGNAL"), FeedPos + FeedSize * 0.5f - FVector2D(0.f, 14.f), 30, Dim, 1);
		}
		else
		{
			const std::vector<int> There = Ucn.SubjectsOnCamera(Cam);
			const float Count = FMath::Max(1.f, static_cast<float>(There.size()));
			const float W = FMath::Min(FeedSize.Y * 0.5f, FeedSize.X / Count * 0.8f);
			for (size_t Index = 0; Index < There.size(); ++Index)
			{
				const float X = FeedPos.X + FeedSize.X * (static_cast<float>(Index) + 0.5f) / Count - W * 0.5f;
				PaintCamSubject(Paint, There[Index], FVector2D(X, FeedPos.Y + FeedSize.Y * 0.16f), W, FLinearColor(0.75f, 0.9f, 0.8f, 1.f));
			}
			if (Ucn.IsCameraInfected(Cam))
			{
				Paint.Box(FeedPos, FeedSize, FLinearColor(0.1f, 0.9f, 0.3f, 0.18f));
				Paint.Text(FString::Printf(TEXT("INFECTED %.0fs - SHOCK IT"), Ucn.InfectionLeft(Cam)), FVector2D(FeedPos.X + FeedSize.X * 0.5f, FeedPos.Y + FeedSize.Y - 44.f), 18,
					FLinearColor(0.3f, 0.95f, 0.5f, 1.f), 1);
			}
			if (Ucn.VaccineCamera() == Cam)
			{
				const FVector2D Bottle(FeedPos.X + 24.f, FeedPos.Y + FeedSize.Y - 84.f);
				Paint.Box(Bottle, FVector2D(34.f, 60.f), FLinearColor(0.3f, 0.95f, 0.5f, 1.f));
				Paint.Box(Bottle + FVector2D(10.f, -12.f), FVector2D(14.f, 14.f), Cream);
				Paint.Text(TEXT("VACCINE (V)"), Bottle + FVector2D(44.f, 20.f), 13, Cream);
				Hit(Bottle - FVector2D(6.f, 16.f), FVector2D(48.f, 82.f), TEXT("ucn:vaccine"));
			}
			if (Clock - UcnShockFlash < 0.2)
			{
				Paint.Box(FeedPos, FeedSize, FLinearColor(0.85f, 0.9f, 1.f, 0.6f));   // the flash
			}
			const int32 Lines = bReduceMotion ? 6 : 26;
			for (int32 Line = 0; Line < Lines; ++Line)
			{
				const float Y = FeedPos.Y + FMath::FRand() * FeedSize.Y;
				Paint.Box(FVector2D(FeedPos.X, Y), FVector2D(FeedSize.X, 1.f + FMath::FRand() * 2.f), FLinearColor(1.f, 1.f, 1.f, 0.05f + FMath::FRand() * 0.08f));
			}
		}
		Paint.Frame(FeedPos, FeedSize, Cyan, 2.f);
		const FString RoomLabel = Cam == BUcn::WindowCamera ? FString(TEXT("OUTSIDE THE FRONT WINDOW")) : ToF(BIB::Arcade::RoomName(Room)).ToUpper();
		Paint.Text(FString::Printf(TEXT("CAM %d - %s"), Cam + 1 == 10 ? 0 : Cam + 1, *RoomLabel), FVector2D(FeedPos.X + 14.f, FeedPos.Y + 10.f), 16, Cream);
		// The shock (a very limited number per camera).
		const FVector2D ShockPos(FeedPos.X, FeedPos.Y + FeedSize.Y + 10.f);
		const FVector2D ShockSize(FeedSize.X * 0.4f, 56.f);
		Button(Paint, ShockPos, ShockSize, FString::Printf(TEXT("SHOCK  (%d left here)"), Ucn.ShocksLeft(Cam)), TEXT("F"), TEXT("ucn:shock"), false, Gold);
		if (Ucn.IsShockFalseAlarm())
		{
			Paint.Text(TEXT("FALSE ALARM - nothing to shock"), FVector2D(ShockPos.X + ShockSize.X + 14.f, ShockPos.Y + 18.f), 13, Danger);   // Judge's warning cue
		}
		// The map with the ten cameras (Night Shift's layout).
		const FVector2D MapPos(Size.X * 0.61f, Size.Y * 0.16f);
		const FVector2D MapSize(Size.X * 0.36f, Size.Y * 0.56f);
		Paint.Box(MapPos, MapSize, Panel);
		auto Where = [&MapPos, &MapSize](BIB::Arcade::ERoom R)
		{
			double X = 0.0;
			double Y = 0.0;
			BIB::Arcade::RoomMapPosition(R, X, Y);
			return FVector2D(MapPos.X + static_cast<float>(X) * MapSize.X, MapPos.Y + static_cast<float>(Y) * MapSize.Y);
		};
		Paint.Box(Where(BIB::Arcade::ERoom::Inside) - FVector2D(30.f, 10.f), FVector2D(60.f, 18.f), Pink);
		Paint.Text(TEXT("YOU"), Where(BIB::Arcade::ERoom::Inside) - FVector2D(0.f, 10.f), 11, Void, 1);
		for (int32 Camera = 0; Camera < BIB::Arcade::CameraCount; ++Camera)
		{
			const FVector2D Centre = Where(static_cast<BIB::Arcade::ERoom>(Camera));
			const FVector2D BoxSize(74.f, 38.f);
			const bool bSelected = Camera == Cam;
			const bool bDead = Ucn.IsCameraDisabled(Camera);
			const FLinearColor Fill = bDead ? FLinearColor(0.25f, 0.05f, 0.06f, 1.f) : (bSelected ? Cyan : FLinearColor(0.12f, 0.12f, 0.18f, 1.f));
			Paint.Box(Centre - BoxSize * 0.5f, BoxSize, Fill);
			if (Ucn.IsCameraInfected(Camera))
			{
				Paint.Frame(Centre - BoxSize * 0.5f, BoxSize, FLinearColor(0.3f, 0.95f, 0.5f, 1.f), 3.f);
			}
			Paint.Text(FString::Printf(TEXT("CAM %d"), Camera + 1 == 10 ? 0 : Camera + 1), Centre - FVector2D(0.f, 14.f), 11, bSelected ? Void : Ink, 1);
			Paint.Text(bDead ? FString(TEXT("X")) : FString::Printf(TEXT("%d"), Ucn.ShocksLeft(Camera)), Centre + FVector2D(0.f, 1.f), 10, bSelected ? Void : Dim, 1, false);
			Hit(Centre - BoxSize * 0.5f, BoxSize, FString::Printf(TEXT("ucn:sel:%d"), Camera));
		}
	}

	// ------------------------------------------------------------------------------------------ painting: Rhythm
	void PaintRhythm(const FPainter& Paint, const FVector2D& Size) const
	{
		const BIB::FSettings Settings = Instance.IsValid() ? Instance->GetProfile().Meta.Settings : BIB::FSettings();
		const BIB::Arcade::FSong& Info = BIB::Arcade::Song(Song.SongIndex());
		const int32 Lanes = Song.Lanes();
		const FLinearColor LaneColours[5] = {Pink, Cream, Cyan, Purple, Gold};
		const float LaneW = FMath::Min(110.f, Size.X * 0.09f);
		const float Left = (Size.X - LaneW * Lanes) * 0.5f;
		const float Top = 90.f;
		const float HitY = Size.Y * 0.82f;
		// Wordmark and song.
		Paint.Text(TEXT("BOLD"), FVector2D(28.f, 18.f), 30, Pink);
		Paint.Text(TEXT("in"), FVector2D(120.f, 26.f), 18, Cream);
		Paint.Text(TEXT("BEAUTY"), FVector2D(28.f, 52.f), 30, Ink);
		Paint.Text(FString::Printf(TEXT("%s  -  %d BPM  -  %s"), *ToF(Info.Title), Info.Bpm, *ToF(BIB::Arcade::DifficultyName(Song.GetDifficulty()))), FVector2D(Size.X * 0.5f, 24.f), 16, Ink, 1);
		// Score panel.
		Paint.Text(ToF(BIB::Meta::Num(Song.Score())), FVector2D(Size.X - 28.f, 18.f), 30, Cream, 2);
		Paint.Text(FString::Printf(TEXT("combo %d   x%d"), Song.Combo(), Song.Multiplier()), FVector2D(Size.X - 28.f, 60.f), 16, Song.Multiplier() >= 4 ? Gold : Ink, 2);
		// Meter.
		const FVector2D MeterPos(Size.X - 52.f, Size.Y * 0.3f);
		const FVector2D MeterSize(18.f, Size.Y * 0.4f);
		Paint.Box(MeterPos, MeterSize, Panel);
		const float Fill = static_cast<float>(Song.Meter());
		Paint.Box(FVector2D(MeterPos.X, MeterPos.Y + MeterSize.Y * (1.f - Fill)), FVector2D(MeterSize.X, MeterSize.Y * Fill), Fill > 0.6f ? FLinearColor(0.3f, 0.85f, 0.4f, 1.f) : (Fill > 0.3f ? Gold : Danger));
		// Lanes and the hit line.
		for (int32 Lane = 0; Lane < Lanes; ++Lane)
		{
			const float X = Left + Lane * LaneW;
			const bool bFlash = Clock - LaneFlash[Lane] < 0.12;
			Paint.Box(FVector2D(X + 2.f, Top), FVector2D(LaneW - 4.f, HitY - Top + 40.f), FLinearColor(LaneColours[Lane].R, LaneColours[Lane].G, LaneColours[Lane].B, bFlash || bLaneDown[Lane] ? 0.16f : 0.06f));
			Paint.Box(FVector2D(X + 6.f, HitY - 6.f), FVector2D(LaneW - 12.f, 12.f), bFlash ? LaneColours[Lane] : FLinearColor(1.f, 1.f, 1.f, 0.35f));
			Paint.Text(KeyLabel(Settings, BIB::RhythmBindFirst + Lane), FVector2D(X + LaneW * 0.5f, HitY + 18.f), 15, LaneColours[Lane], 1);
			Hit(FVector2D(X, Top), FVector2D(LaneW, HitY - Top + 60.f), FString::Printf(TEXT("lane:%d"), Lane));
		}
		// Notes: they fall from the top and reach the line at their time.
		const double Now = Song.Time();
		const auto& Notes = Song.Notes();
		for (size_t Index = 0; Index < Notes.size(); ++Index)
		{
			const double Ahead = Notes[Index].Time - Now;
			if (Ahead > BIB::Arcade::RhythmScrollSeconds)
			{
				break;
			}
			if (Ahead < -0.25 || Song.IsHit(Index))
			{
				continue;
			}
			const float Y = HitY - static_cast<float>(Ahead / BIB::Arcade::RhythmScrollSeconds) * (HitY - Top);
			const int32 Lane = FMath::Clamp(Notes[Index].Lane, 0, Lanes - 1);
			const FLinearColor Colour = Song.IsMissed(Index) ? FLinearColor(0.4f, 0.4f, 0.45f, 0.5f) : LaneColours[Lane];
			Paint.Box(FVector2D(Left + Lane * LaneW + 8.f, Y - 11.f), FVector2D(LaneW - 16.f, 22.f), Colour);
			Paint.Box(FVector2D(Left + Lane * LaneW + 14.f, Y - 5.f), FVector2D(LaneW - 28.f, 10.f), FLinearColor(1.f, 1.f, 1.f, 0.35f));
		}
		if (Song.LastJudge() != BIB::Arcade::EJudge::Count && Now - Song.LastJudgeTime() < 0.5)
		{
			const BIB::Arcade::EJudge Judge = Song.LastJudge();
			const FLinearColor Colour = Judge == BIB::Arcade::EJudge::Perfect ? Gold : (Judge == BIB::Arcade::EJudge::Miss ? Danger : Cream);
			Paint.Text(ToF(BIB::Arcade::JudgeName(Judge)), FVector2D(Size.X * 0.5f, HitY - 120.f), 28, Colour, 1);
		}
		if (Now < BIB::Arcade::RhythmLeadIn)
		{
			Paint.Text(FString::Printf(TEXT("%d"), FMath::CeilToInt(BIB::Arcade::RhythmLeadIn - Now)), FVector2D(Size.X * 0.5f, Size.Y * 0.4f), 60, Pink, 1);
		}
		if (bQuitArmed)
		{
			Paint.Text(TEXT("Press Esc again to quit the song"), FVector2D(Size.X * 0.5f, 60.f), 14, Danger, 1);
		}
	}

	// ------------------------------------------------------------------------------------------ painting: mini-games
	void PaintMiniHeader(const FPainter& Paint, const FVector2D& Size, const FString& Title, const FString& Right, const FString& Help) const
	{
		Paint.Text(TEXT("BOLD"), FVector2D(28.f, 18.f), 30, Pink);
		Paint.Text(TEXT("in"), FVector2D(120.f, 26.f), 18, Cream);
		Paint.Text(TEXT("BEAUTY"), FVector2D(28.f, 52.f), 30, Ink);
		Paint.Text(Title, FVector2D(Size.X * 0.5f, 22.f), 20, Cream, 1);
		Paint.Text(Right, FVector2D(Size.X - 28.f, 18.f), 26, Cream, 2);
		Paint.Text(Help, FVector2D(Size.X * 0.5f, Size.Y - 30.f), 12, Dim, 1, false);
		if (bQuitArmed)
		{
			Paint.Text(TEXT("Press Esc again to give up"), FVector2D(Size.X * 0.5f, 56.f), 14, Danger, 1);
		}
	}

	void PaintHalo(const FPainter& Paint, const FVector2D& Size) const
	{
		const BIB::FSettings Settings = Instance.IsValid() ? Instance->GetProfile().Meta.Settings : BIB::FSettings();
		const BIB::Arcade::FHaloRules& Rules = Halo.Rules();
		PaintMiniHeader(Paint, Size, FString::Printf(TEXT("HALO FALL - %s - WAVE %d / %d"), *ToF(BIB::Arcade::DifficultyName(Halo.GetDifficulty())).ToUpper(),
			FMath::Min(Halo.Wave(), Rules.Waves), Rules.Waves), ToF(BIB::Meta::Num(Halo.Score())),
			FString::Printf(TEXT("Move %s / %s or arrows   Fire (hold) %s   Esc: give up"), *KeyLabel(Settings, 2), *KeyLabel(Settings, 3), *KeyLabel(Settings, 4)));
		// The field: a night sky over Neo Tokyo's skyline.
		const FVector2D FieldPos(Size.X * 0.2f, 84.f);
		const FVector2D FieldSize(Size.X * 0.6f, Size.Y - 140.f);
		Paint.Box(FieldPos, FieldSize, FLinearColor(0.03f, 0.02f, 0.07f, 1.f));
		auto At = [&FieldPos, &FieldSize](double X, double Y)
		{
			return FVector2D(FieldPos.X + static_cast<float>(X) * FieldSize.X, FieldPos.Y + static_cast<float>(Y) * FieldSize.Y);
		};
		for (int32 Star = 0; Star < 40; ++Star)
		{
			const float SX = FMath::Frac(Star * 0.6180339f);
			const float SY = FMath::Frac(Star * 0.3819660f + 0.13f) * 0.7f;
			Paint.Box(At(SX, SY), FVector2D(2.f, 2.f), FLinearColor(1.f, 1.f, 1.f, 0.15f + 0.25f * FMath::Frac(Star * 0.37f)));
		}
		for (int32 Tower = 0; Tower < 16; ++Tower)
		{
			const float H = 0.05f + 0.09f * FMath::Frac(Tower * 0.7548776f);
			const FVector2D Base = At(Tower / 16.0, 1.0 - H);
			Paint.Box(Base, FVector2D(FieldSize.X / 16.f - 3.f, H * FieldSize.Y), FLinearColor(0.08f, 0.06f, 0.14f, 1.f));
			Paint.Box(Base, FVector2D(FieldSize.X / 16.f - 3.f, 2.f), Tower % 3 == 0 ? Pink : (Tower % 3 == 1 ? Cyan : Purple));
		}
		Paint.Frame(FieldPos, FieldSize, FLinearColor(1.f, 1.f, 1.f, 0.2f), 2.f);
		// Angels: a halo over a body of light with two wings; the top row (Seraphs) burns pink.
		const float Unit = FieldSize.X / 40.f;
		for (const BIB::Arcade::FHaloAngel& Angel : Halo.Angels())
		{
			if (!Angel.bAlive)
			{
				continue;
			}
			const FVector2D C = At(Angel.X, Angel.Y);
			const float Bob = bReduceMotion ? 0.f : FMath::Sin(static_cast<float>(Clock) * 4.f + static_cast<float>(Angel.X) * 20.f) * 2.f;
			const FLinearColor Body = Angel.Row == 0 ? Pink : Cream;
			Paint.Box(C + FVector2D(-Unit * 0.5f, -Unit * 1.1f + Bob), FVector2D(Unit, Unit * 0.22f), Gold);
			Paint.Box(C + FVector2D(-Unit * 0.3f, -Unit * 0.7f + Bob), FVector2D(Unit * 0.6f, Unit * 1.3f), Body);
			Paint.Line(C + FVector2D(-Unit * 0.3f, -Unit * 0.4f + Bob), C + FVector2D(-Unit * 1.1f, -Unit * 0.9f + Bob), Body, 2.f);
			Paint.Line(C + FVector2D(Unit * 0.3f, -Unit * 0.4f + Bob), C + FVector2D(Unit * 1.1f, -Unit * 0.9f + Bob), Body, 2.f);
		}
		if (Halo.SeraphActive())
		{
			const FVector2D C = At(Halo.SeraphX(), 0.04);
			Paint.Box(C + FVector2D(-Unit, -Unit * 0.9f), FVector2D(Unit * 2.f, Unit * 0.3f), Gold);
			Paint.Box(C + FVector2D(-Unit * 0.45f, -Unit * 0.5f), FVector2D(Unit * 0.9f, Unit * 1.2f), Gold);
			Paint.Text(TEXT("SERAPH"), C + FVector2D(0.f, Unit * 0.8f), 10, Gold, 1);
		}
		for (const BIB::Arcade::FHaloShot& Shot : Halo.Shots())
		{
			const FVector2D C = At(Shot.X, Shot.Y);
			Paint.Box(C - FVector2D(2.f, 7.f), FVector2D(Shot.bEnemy ? 5.f : 3.f, 14.f), Shot.bEnemy ? Gold : Cyan);
		}
		for (const FSpark& Spark : Sparks)
		{
			const double Age = Clock - Spark.Born;
			if (Age < 0.4)
			{
				const float R = static_cast<float>(Age) * 60.f;
				Paint.Frame(At(Spark.Where.X, Spark.Where.Y) - FVector2D(R, R), FVector2D(R * 2.f, R * 2.f), Spark.bGold ? Gold : FLinearColor(1.f, 1.f, 1.f, 0.5f), 2.f);
			}
		}
		// The commander's turret (blinks while invulnerable).
		if (!Halo.IsInvulnerable() || FMath::Fmod(static_cast<float>(Clock), 0.2f) < 0.1f)
		{
			const FVector2D C = At(Halo.PlayerX(), BIB::Arcade::HaloPlayerY);
			Paint.Box(C + FVector2D(-Unit * 1.4f, -Unit * 0.2f), FVector2D(Unit * 2.8f, Unit * 0.8f), Pink);
			Paint.Box(C + FVector2D(-Unit * 0.25f, -Unit * 0.9f), FVector2D(Unit * 0.5f, Unit * 0.8f), Cream);
		}
		// Side panel: lives and waves.
		Paint.Text(TEXT("LIVES"), FVector2D(Size.X * 0.04f, Size.Y * 0.3f), 14, Dim);
		for (int32 Life = 0; Life < Rules.Lives; ++Life)
		{
			Paint.Box(FVector2D(Size.X * 0.04f + Life * 30.f, Size.Y * 0.3f + 26.f), FVector2D(22.f, 12.f), Life < Halo.Lives() ? Pink : FLinearColor(1.f, 1.f, 1.f, 0.1f));
		}
		Paint.Text(FString::Printf(TEXT("Angels left: %d"), Halo.AliveAngels()), FVector2D(Size.X * 0.04f, Size.Y * 0.3f + 60.f), 14, Ink, 0, false);
		Paint.Text(TEXT("Top row x3, Seraph x15"), FVector2D(Size.X * 0.04f, Size.Y * 0.3f + 84.f), 12, Dim, 0, false);
		if (Halo.IsOver() && !bShowResult)
		{
			Paint.Text(Halo.IsWon() ? TEXT("THE SKY IS CLEAR") : TEXT("THE CITY FELL"), FVector2D(Size.X * 0.5f, Size.Y * 0.45f), 40, Halo.IsWon() ? Gold : Danger, 1);
		}
	}

	// Ark Rally (BIBRules/BIBRally.h): a side view that follows the bike. World metres become pixels through the camera.
	struct FRallyView
	{
		FVector2D Origin;    // the top-left of the play area
		FVector2D Extent;
		FVector2D Camera;    // world point at the centre
		float Scale = 40.f;  // pixels per metre

		FVector2D ToScreen(double X, double Y) const
		{
			return Origin + Extent * 0.5f + FVector2D((X - Camera.X) * Scale, -(Y - Camera.Y) * Scale);
		}
	};

	void RallyCircle(const FPainter& Paint, const FVector2D& Centre, float Radius, const FLinearColor& Colour, float Thickness) const
	{
		const int32 Sides = 14;
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const float A0 = 2.f * PI * Side / Sides;
			const float A1 = 2.f * PI * (Side + 1) / Sides;
			Paint.Line(Centre + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Radius, Centre + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Radius, Colour, Thickness);
		}
	}

	// The player's vehicle (or a rival's ghost on the same vehicle, with only the chassis pose known).
	void PaintRallyBike(const FPainter& Paint, const FRallyView& View, double X, double Y, double Angle, const double* WheelX, const double* WheelY, const double* Turn,
		const FLinearColor& Frame, const FLinearColor& Rider, float Alpha) const
	{
		const BIB::Arcade::FRallyVehicleSpec& V = Rally.Spec();
		const bool bBuggy = Rally.Vehicle() == BIB::Arcade::ERallyVehicle::Buggy;
		const double C = FMath::Cos(Angle);
		const double S = FMath::Sin(Angle);
		auto Local = [&](double LX, double LY) { return View.ToScreen(X + C * LX - S * LY, Y + S * LX + C * LY); };
		auto Fade = [Alpha](FLinearColor Colour) { Colour.A *= Alpha; return Colour; };
		const float Px = View.Scale;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const FVector2D Hub = WheelX ? View.ToScreen(WheelX[Index], WheelY[Index]) : Local(V.Anchors[Index].X, V.Anchors[Index].Y);
			const float Radius = static_cast<float>(V.WheelRadius) * Px;
			RallyCircle(Paint, Hub, Radius, Fade(Ink), FMath::Max(2.f, Px * (bBuggy ? 0.14f : 0.09f)));
			RallyCircle(Paint, Hub, Radius * 0.55f, Fade(FLinearColor(1.f, 1.f, 1.f, 0.35f)), 1.5f);
			const float Spin = Turn ? static_cast<float>(Turn[Index]) : 0.f;
			for (int32 Spoke = 0; Spoke < 3; ++Spoke)
			{
				const float Ang = Spin + Spoke * PI / 3.f;
				const FVector2D D(FMath::Cos(Ang) * Radius, -FMath::Sin(Ang) * Radius);
				Paint.Line(Hub - D, Hub + D, Fade(FLinearColor(1.f, 1.f, 1.f, 0.4f)), 1.5f);
			}
			// Swing arm / fork (bike) or suspension arm (buggy) from the body to the hub.
			Paint.Line(bBuggy ? Local(Index == 0 ? -0.6 : 0.6, -0.05) : Local(Index == 0 ? -0.2 : 0.55, 0.05), Hub, Fade(Frame), FMath::Max(2.f, Px * 0.08f));
		}
		if (bBuggy)
		{
			// Buggy: a low tub, a roll cage, the driver's helmet inside it.
			Paint.Line(Local(-1.15, 0.0), Local(1.15, 0.05), Fade(Frame), FMath::Max(4.f, Px * 0.3f));
			Paint.Line(Local(0.9, 0.05), Local(1.25, 0.25), Fade(Frame), FMath::Max(3.f, Px * 0.12f));   // nose
			const FLinearColor Cage = Fade(Ink);
			const float Bar = FMath::Max(2.f, Px * 0.07f);
			Paint.Line(Local(-0.7, 0.12), Local(-0.45, 0.72), Cage, Bar);
			Paint.Line(Local(-0.45, 0.72), Local(0.45, 0.72), Cage, Bar);
			Paint.Line(Local(0.45, 0.72), Local(0.8, 0.12), Cage, Bar);
			Paint.Line(Local(-0.05, 0.72), Local(0.1, 0.12), Cage, Bar * 0.7f);
			const FVector2D Head = Local(-0.15, 0.42);
			const float HeadSize = Px * 0.36f;
			Paint.Box(Head - FVector2D(HeadSize, HeadSize) * 0.5f, FVector2D(HeadSize, HeadSize), Fade(Rider));
			Paint.Box(Head + FVector2D(0.f, -HeadSize * 0.15f), FVector2D(HeadSize * 0.5f, HeadSize * 0.25f), Fade(Void));
		}
		else
		{
			// Dirt bike: tank and seat, handlebar, the rider standing on the pegs.
			Paint.Line(Local(-0.75, 0.15), Local(0.0, 0.2), Fade(Frame), FMath::Max(3.f, Px * 0.16f));
			Paint.Line(Local(0.0, 0.2), Local(0.6, 0.3), Fade(Frame), FMath::Max(3.f, Px * 0.2f));
			Paint.Line(Local(0.55, 0.3), Local(0.75, 0.55), Fade(Ink), FMath::Max(2.f, Px * 0.06f));
			Paint.Line(Local(-0.2, 0.25), Local(0.2, 0.0), Fade(Rider), FMath::Max(2.f, Px * 0.1f));
			Paint.Line(Local(-0.2, 0.25), Local(-0.1, 0.65), Fade(Rider), FMath::Max(3.f, Px * 0.16f));
			Paint.Line(Local(-0.1, 0.6), Local(0.7, 0.52), Fade(Rider), FMath::Max(2.f, Px * 0.08f));
			const FVector2D Head = Local(V.HeadLocal.X, V.HeadLocal.Y);
			const float HeadSize = Px * 0.42f;
			Paint.Box(Head - FVector2D(HeadSize, HeadSize) * 0.5f, FVector2D(HeadSize, HeadSize), Fade(Rider));
			Paint.Box(Head + FVector2D(0.f, -HeadSize * 0.15f), FVector2D(HeadSize * 0.5f, HeadSize * 0.25f), Fade(Void));
		}
	}

	void PaintRally(const FPainter& Paint, const FVector2D& Size) const
	{
		const BIB::FSettings Settings = Instance.IsValid() ? Instance->GetProfile().Meta.Settings : BIB::FSettings();
		const BIB::Arcade::FRallyTrack& Track = Rally.Track();
		PaintMiniHeader(Paint, Size, FString::Printf(TEXT("ARK RALLY - %s - %s"), Rally.IsCustom() ? TEXT("CUSTOM") : *ToF(BIB::Arcade::DifficultyName(Rally.GetDifficulty())).ToUpper(),
			*ToF(Track.Name).ToUpper()), FString::Printf(TEXT("%d:%04.1f"), FMath::FloorToInt(Rally.Time() / 60.0), FMath::Fmod(Rally.Time(), 60.0)),
			FString::Printf(TEXT("Gas %s / Up   Brake %s / Down   Lean back %s / Left   Lean fwd %s / Right   Nitro %s / Shift   R: respawn   Esc: give up"),
				*KeyLabel(Settings, 0), *KeyLabel(Settings, 1), *KeyLabel(Settings, 2), *KeyLabel(Settings, 3), *KeyLabel(Settings, 4)));
		FRallyView View;
		View.Origin = FVector2D(0.f, 84.f);
		View.Extent = FVector2D(Size.X, Size.Y - 84.f - 52.f);
		View.Camera = RallyCamera;
		View.Scale = FMath::Max(18.f, static_cast<float>(View.Extent.X) / 26.f);
		const double HalfW = View.Extent.X * 0.5 / View.Scale + 1.0;
		const double Left = View.Camera.X - HalfW;
		const double Right = View.Camera.X + HalfW;
		const float Bottom = static_cast<float>(View.Origin.Y + View.Extent.Y);
		// Sky and a slow skyline (parallax).
		const int32 Bands = 6;
		for (int32 Band = 0; Band < Bands; ++Band)
		{
			const float T = static_cast<float>(Band) / Bands;
			Paint.Box(FVector2D(0.f, View.Origin.Y + View.Extent.Y * T), FVector2D(Size.X, View.Extent.Y / Bands + 1.f),
				FMath::Lerp(FLinearColor(0.05f, 0.03f, 0.11f, 1.f), FLinearColor(0.16f, 0.06f, 0.2f, 1.f), T));
		}
		const float TowerW = 70.f;
		const float Shift = FMath::Fmod(static_cast<float>(View.Camera.X) * View.Scale * 0.25f, TowerW * 12.f);
		for (int32 Tower = -1; Tower < static_cast<int32>(Size.X / TowerW) + 2; ++Tower)
		{
			const int32 Seed = (Tower + FMath::FloorToInt(static_cast<float>(View.Camera.X) * View.Scale * 0.25f / (TowerW * 12.f)) * 12) & 0xFFFF;
			const float H = View.Extent.Y * (0.12f + 0.22f * FMath::Frac(Seed * 0.7548776f));
			const float TX = Tower * TowerW - FMath::Fmod(Shift, TowerW);
			Paint.Box(FVector2D(TX, Bottom - H - View.Extent.Y * 0.15f), FVector2D(TowerW - 6.f, H), FLinearColor(0.1f, 0.07f, 0.16f, 1.f));
			Paint.Box(FVector2D(TX, Bottom - H - View.Extent.Y * 0.15f), FVector2D(TowerW - 6.f, 2.f), Seed % 3 == 0 ? Pink : (Seed % 3 == 1 ? Cyan : Purple));
		}
		// The ground: filled columns under the surface, then the surface line.
		const float Column = 6.f;
		for (float SX = 0.f; SX < Size.X; SX += Column)
		{
			const double WX = View.Camera.X + (SX + Column * 0.5f - View.Extent.X * 0.5f) / View.Scale;
			const float SY = static_cast<float>(View.ToScreen(WX, Track.HeightAt(WX)).Y);
			if (SY < Bottom)
			{
				Paint.Box(FVector2D(SX, FMath::Max(SY, static_cast<float>(View.Origin.Y))), FVector2D(Column + 0.5f, Bottom - FMath::Max(SY, static_cast<float>(View.Origin.Y))), Panel);
			}
		}
		const std::vector<BIB::Arcade::FVec2>& Ground = Track.Ground;
		for (size_t Index = 0; Index + 1 < Ground.size(); ++Index)
		{
			if (Ground[Index + 1].X < Left || Ground[Index].X > Right)
			{
				continue;
			}
			Paint.Line(View.ToScreen(Ground[Index].X, Ground[Index].Y), View.ToScreen(Ground[Index + 1].X, Ground[Index + 1].Y), Pink, 3.f);
		}
		// Start and finish lines.
		for (const double LineX : {Track.StartX, Track.FinishX})
		{
			if (LineX < Left || LineX > Right)
			{
				continue;
			}
			const FVector2D Foot = View.ToScreen(LineX, Track.HeightAt(LineX));
			const float Cell = View.Scale * 0.35f;
			for (int32 Row = 0; Row < 8; ++Row)
			{
				Paint.Box(Foot + FVector2D((Row % 2) * Cell - Cell, -(Row + 1) * Cell), FVector2D(Cell, Cell), LineX == Track.FinishX ? Ink : FLinearColor(1.f, 1.f, 1.f, 0.3f));
			}
		}
		// Things on the track.
		const float Px = View.Scale;
		for (const BIB::Arcade::FRallyThing& Thing : Track.Things)
		{
			if (Thing.X < Left || Thing.X > Right)
			{
				continue;
			}
			const FVector2D At = View.ToScreen(Thing.X, Thing.Y);
			switch (Thing.Kind)
			{
			case BIB::Arcade::ERallyThing::Coin:
				if (!Thing.bUsed)
				{
					const float Wobble = bReduceMotion ? 1.f : FMath::Abs(FMath::Cos(static_cast<float>(Clock) * 3.f + static_cast<float>(Thing.X)));
					Paint.Box(At - FVector2D(Px * 0.22f * Wobble, Px * 0.22f), FVector2D(Px * 0.44f * FMath::Max(0.25f, Wobble), Px * 0.44f), Gold);
				}
				break;
			case BIB::Arcade::ERallyThing::Bomb:
				if (!Thing.bUsed)
				{
					Paint.Box(At + FVector2D(-Px * 0.3f, -Px * 0.6f), FVector2D(Px * 0.6f, Px * 0.6f), Danger);
					Paint.Line(At + FVector2D(0.f, -Px * 0.6f), At + FVector2D(Px * 0.2f, -Px * 0.9f), Cream, 2.f);
					Paint.Text(TEXT("!"), At + FVector2D(0.f, -Px * 0.62f), 12, Void, 1);
				}
				else
				{
					Paint.Box(At + FVector2D(-Px * 0.4f, -Px * 0.1f), FVector2D(Px * 0.8f, Px * 0.1f), FLinearColor(0.f, 0.f, 0.f, 0.6f));   // scorch
				}
				break;
			case BIB::Arcade::ERallyThing::Spring:
				for (int32 Coil = 0; Coil < 4; ++Coil)
				{
					Paint.Line(At + FVector2D(-Px * 0.35f, -Coil * Px * 0.12f), At + FVector2D(Px * 0.35f, -(Coil + 0.5f) * Px * 0.12f), Cyan, 2.f);
				}
				Paint.Box(At + FVector2D(-Px * 0.45f, -Px * 0.55f), FVector2D(Px * 0.9f, Px * 0.08f), Cyan);
				break;
			case BIB::Arcade::ERallyThing::Boost:
			{
				const FVector2D End = View.ToScreen(Thing.X + 3.5, Track.HeightAt(Thing.X + 3.5));
				Paint.Line(At + FVector2D(0.f, -2.f), End + FVector2D(0.f, -2.f), Thing.bUsed ? Gold : Cyan, 5.f);
				Paint.Text(TEXT(">>>"), (At + End) * 0.5f + FVector2D(0.f, -Px * 0.6f), 14, Thing.bUsed ? Gold : Cyan, 1);
				break;
			}
			case BIB::Arcade::ERallyThing::Checkpoint:
				Paint.Line(At, At + FVector2D(0.f, -Px * 2.2f), Ink, 2.f);
				Paint.Box(At + FVector2D(0.f, -Px * 2.2f), FVector2D(Px * 0.7f, Px * 0.45f), Thing.bUsed ? Gold : Purple);
				break;
			}
		}
		// The rivals' ghosts, then the player's bike.
		for (int32 Rival = 0; Rival < BIB::Arcade::RallyRivals; ++Rival)
		{
			const BIB::Arcade::FRallyGhostFrame Ghost = Rally.RivalAt(Rival, Rally.Time());
			if (Ghost.X < Left - 2.0 || Ghost.X > Right + 2.0)
			{
				continue;
			}
			const FLinearColor Tint = Rival == 0 ? Gold : (Rival == 1 ? Cyan : Purple);
			PaintRallyBike(Paint, View, Ghost.X, Ghost.Y, Ghost.Angle, nullptr, nullptr, nullptr, Tint, Tint, 0.35f);
			Paint.Text(OperatorName(Rally.RivalFace(Rival)), View.ToScreen(Ghost.X, Ghost.Y + 1.6), 11, FLinearColor(Tint.R, Tint.G, Tint.B, 0.7f), 1);
		}
		const BIB::Arcade::FRallyBody& Body = Rally.Chassis();
		const double WheelX[2] = {Rally.Wheel(0).Pos.X, Rally.Wheel(1).Pos.X};
		const double WheelY[2] = {Rally.Wheel(0).Pos.Y, Rally.Wheel(1).Pos.Y};
		const double Turn[2] = {Rally.Wheel(0).Turn, Rally.Wheel(1).Turn};
		const bool bBlink = Rally.IsCrashed() && FMath::Fmod(static_cast<float>(Clock), 0.2f) < 0.1f;
		if (!bBlink)
		{
			if (Rally.IsNitroOn())
			{
				// The nitro flame out of the back.
				const double C = FMath::Cos(Body.Angle);
				const double S = FMath::Sin(Body.Angle);
				const float Flicker = bReduceMotion ? 1.f : 0.75f + 0.25f * FMath::Sin(static_cast<float>(Clock) * 40.f);
				const FVector2D Exhaust = View.ToScreen(Body.Pos.X - C * 1.0, Body.Pos.Y - S * 1.0);
				const FVector2D Tip = View.ToScreen(Body.Pos.X - C * (1.0 + 1.6 * Flicker), Body.Pos.Y - S * (1.0 + 1.6 * Flicker));
				Paint.Line(Exhaust, Tip, Cyan, View.Scale * 0.3f);
				Paint.Line(Exhaust, (Exhaust + Tip) * 0.5f, Cream, View.Scale * 0.15f);
			}
			PaintRallyBike(Paint, View, Body.Pos.X, Body.Pos.Y, Body.Angle, WheelX, WheelY, Turn, Pink, Cream, 1.f);
		}
		// HUD: progress with every rider on it, place, coins, crashes, speed.
		const FVector2D BarPos(Size.X * 0.2f, 66.f);
		const FVector2D BarSize(Size.X * 0.6f, 6.f);
		Paint.Box(BarPos, BarSize, FLinearColor(1.f, 1.f, 1.f, 0.12f));
		const double Span = FMath::Max(1.0, Track.FinishX - Track.StartX);
		for (int32 Rival = 0; Rival < BIB::Arcade::RallyRivals; ++Rival)
		{
			const double P = FMath::Clamp((Rally.RivalAt(Rival, Rally.Time()).X - Track.StartX) / Span, 0.0, 1.0);
			Paint.Box(BarPos + FVector2D(BarSize.X * static_cast<float>(P) - 3.f, -4.f), FVector2D(6.f, 14.f), Rival == 0 ? Gold : (Rival == 1 ? Cyan : Purple));
		}
		Paint.Box(BarPos + FVector2D(BarSize.X * static_cast<float>(Rally.Progress()) - 5.f, -6.f), FVector2D(10.f, 18.f), Pink);
		Paint.Text(PlaceText(Rally.Place()), FVector2D(28.f, 96.f), 34, Rally.Place() == 1 ? Gold : Cream);
		Paint.Text(TEXT("PLACE"), FVector2D(28.f, 140.f), 11, Dim);
		Paint.Text(FString::Printf(TEXT("Coins %d / %d"), Rally.Coins(), Track.Coins), FVector2D(Size.X - 28.f, 96.f), 16, Gold, 2);
		Paint.Text(FString::Printf(TEXT("Crashes %d"), Rally.Crashes()), FVector2D(Size.X - 28.f, 120.f), 14, Rally.Crashes() ? Danger : Dim, 2, false);
		Paint.Text(FString::Printf(TEXT("%d km/h"), FMath::RoundToInt(Rally.Speed() * 3.6)), FVector2D(Size.X - 28.f, 142.f), 14, Ink, 2, false);
		Paint.Text(FString::Printf(TEXT("%s   Flips %d"), *ToF(BIB::Arcade::RallyVehicleName(Rally.Vehicle())), Rally.Flips()), FVector2D(Size.X - 28.f, 164.f), 14, Dim, 2, false);
		// The nitro meter (flips fill it).
		const FVector2D NitroPos(28.f, 168.f);
		const FVector2D NitroSize(Size.X * 0.16f, 12.f);
		Paint.Text(TEXT("NITRO"), FVector2D(28.f, 150.f), 11, Rally.IsNitroOn() ? Cyan : Dim);
		Paint.Box(NitroPos, NitroSize, FLinearColor(1.f, 1.f, 1.f, 0.12f));
		Paint.Box(NitroPos, FVector2D(NitroSize.X * static_cast<float>(Rally.Nitro()), NitroSize.Y), Rally.IsNitroOn() ? Cream : Cyan);
		Paint.Frame(NitroPos, NitroSize, FLinearColor(1.f, 1.f, 1.f, 0.3f), 1.f);
		if (!Rally.HasGhosts())
		{
			Paint.Text(TEXT("(the rivals could not ride this track: their ghosts glide)"), FVector2D(Size.X * 0.5f, 96.f), 11, Dim, 1, false);
		}
		const double FlashAge = Clock - RallyFlashAt;
		if (FlashAge < 1.2 && !RallyFlash.IsEmpty())
		{
			Paint.Text(RallyFlash, FVector2D(Size.X * 0.5f, Size.Y * 0.3f), 34, RallyFlash.StartsWith(TEXT("CRASH")) || RallyFlash == TEXT("BOOM") ? Danger
				: (RallyFlash.Contains(TEXT("NITRO")) ? Cyan : Gold), 1);
		}
		if (Rally.IsCrashed())
		{
			Paint.Text(TEXT("Back on track..."), FVector2D(Size.X * 0.5f, Size.Y * 0.3f + 46.f), 14, Ink, 1, false);
		}
		// On-screen controls (mouse / touch): lean on the left, brake and gas on the right.
		const FVector2D Pad(Size.X * 0.09f, 64.f);
		const float PadY = Size.Y - 52.f - Pad.Y - 12.f;
		auto Held = [this](int32 Index) { return bRallyKey[Index] || bRallyTouch[Index]; };
		Button(Paint, FVector2D(24.f, PadY), Pad, TEXT("BACK"), TEXT("lean"), TEXT("rhold:2"), Held(2), Cyan);
		Button(Paint, FVector2D(36.f + Pad.X, PadY), Pad, TEXT("FWD"), TEXT("lean"), TEXT("rhold:3"), Held(3), Cyan);
		Button(Paint, FVector2D(Size.X - 36.f - Pad.X * 2.f, PadY), Pad, TEXT("BRAKE"), TEXT("reverse"), TEXT("rhold:1"), Held(1), Purple);
		Button(Paint, FVector2D(Size.X - 24.f - Pad.X, PadY), Pad, TEXT("GAS"), FString(), TEXT("rhold:0"), Held(0), Pink);
		Button(Paint, FVector2D(Size.X - 24.f - Pad.X, PadY - Pad.Y - 12.f), Pad, TEXT("NITRO"), Rally.Nitro() > 0.0 ? FString::Printf(TEXT("%d%%"),
			FMath::RoundToInt(Rally.Nitro() * 100.0)) : FString(TEXT("flip to fill")), TEXT("rhold:4"), Held(4), Cyan);
		if (Rally.IsOver() && !bShowResult)
		{
			Paint.Text(Rally.IsWon() ? FString::Printf(TEXT("FINISHED - %s"), *PlaceText(Rally.Place())) : FString(TEXT("OUT OF TIME")),
				FVector2D(Size.X * 0.5f, Size.Y * 0.42f), 40, Rally.IsWon() ? Gold : Danger, 1);
		}
	}

	void PaintMemory(const FPainter& Paint, const FVector2D& Size) const
	{
		const BIB::FSettings Settings = Instance.IsValid() ? Instance->GetProfile().Meta.Settings : BIB::FSettings();
		PaintMiniHeader(Paint, Size, FString::Printf(TEXT("NEON MEMORY - %s"), *ToF(BIB::Arcade::DifficultyName(Memory.GetDifficulty())).ToUpper()),
			ToF(BIB::Meta::Num(Memory.Score())), FString::Printf(TEXT("Click a card, or arrows + %s   Esc: give up"), *KeyLabel(Settings, 4)));
		const double Left = Memory.InPreview() ? Memory.Rules().Preview - Memory.Time() : Memory.TimeLeft();
		Paint.Text(Memory.InPreview() ? FString::Printf(TEXT("MEMORISE  %d"), FMath::CeilToInt(Left)) : FString::Printf(TEXT("TIME %d"), FMath::CeilToInt(Left)),
			FVector2D(Size.X * 0.5f, 52.f), 22, Memory.InPreview() ? Gold : (Left < 10.0 ? Danger : Ink), 1);
		Paint.Text(FString::Printf(TEXT("Pairs %d / %d   Moves %d   Combo x%d"), Memory.Matched(), Memory.Pairs(), Memory.Moves(), Memory.Combo()),
			FVector2D(Size.X - 28.f, 58.f), 14, Memory.Combo() >= 3 ? Gold : Ink, 2);
		const int32 Cols = FMath::Max(1, Memory.Cols());
		const int32 Rows = FMath::Max(1, Memory.Rows());
		const FVector2D Area(Size.X * 0.8f, Size.Y - 190.f);
		const float Gap = 12.f;
		const float CardW = FMath::Min((Area.X - Gap * (Cols - 1)) / Cols, ((Area.Y - Gap * (Rows - 1)) / Rows) * 0.8f);
		const float CardH = CardW * 1.25f;
		const FVector2D Origin((Size.X - (CardW * Cols + Gap * (Cols - 1))) * 0.5f, 100.f + (Area.Y - (CardH * Rows + Gap * (Rows - 1))) * 0.5f);
		for (int32 Card = 0; Card < Memory.CardCount(); ++Card)
		{
			const FVector2D Pos = Origin + FVector2D((Card % Cols) * (CardW + Gap), (Card / Cols) * (CardH + Gap));
			const FVector2D CardSize(CardW, CardH);
			const bool bUp = Memory.IsFaceUp(Card);
			const bool bMatched = Memory.IsMatched(Card);
			if (bUp)
			{
				Paint.Box(Pos, CardSize, FLinearColor(0.12f, 0.1f, 0.2f, 1.f));
				const float Face = CardW - 12.f;
				Paint.Image(Portrait(OperatorFace(Memory.FaceOf(Card))), Pos + FVector2D(6.f, 6.f), FVector2D(Face, Face), bMatched ? FLinearColor(1.f, 1.f, 1.f, 0.55f) : FLinearColor::White);
				Paint.Text(OperatorName(Memory.FaceOf(Card)), Pos + FVector2D(CardW * 0.5f, Face + 10.f), 12, bMatched ? Gold : Cream, 1);
			}
			else
			{
				Paint.Box(Pos, CardSize, Panel);
				Paint.Box(Pos + FVector2D(6.f, 6.f), CardSize - FVector2D(12.f, 12.f), FLinearColor(Pink.R * 0.25f, Pink.G * 0.25f, Pink.B * 0.25f, 1.f));
				Paint.Text(TEXT("BB"), Pos + FVector2D(CardW * 0.5f, CardH * 0.5f - 16.f), 22, Pink, 1);
			}
			const bool bFlash = bMatched && Clock - MemoryFlash < 0.4 && Memory.FaceOf(Card) == Memory.LastMatch();
			Paint.Frame(Pos, CardSize, bFlash ? Gold : (Card == MemoryCursor ? Cyan : (bMatched ? FLinearColor(Gold.R, Gold.G, Gold.B, 0.5f) : FLinearColor(1.f, 1.f, 1.f, 0.18f))),
				Card == MemoryCursor || bFlash ? 3.f : 2.f);
			Hit(Pos, CardSize, FString::Printf(TEXT("card:%d"), Card));
		}
		if (Memory.IsOver() && !bShowResult)
		{
			Paint.Text(Memory.IsWon() ? TEXT("ALL PAIRS FOUND") : TEXT("TIME UP"), FVector2D(Size.X * 0.5f, Size.Y * 0.45f), 40, Memory.IsWon() ? Gold : Danger, 1);
		}
	}

	void PaintBreach(const FPainter& Paint, const FVector2D& Size) const
	{
		const BIB::FSettings Settings = Instance.IsValid() ? Instance->GetProfile().Meta.Settings : BIB::FSettings();
		const BIB::Arcade::FBreachRules& Rules = Breach.Rules();
		PaintMiniHeader(Paint, Size, FString::Printf(TEXT("FIREWALL BREACH - %s"), *ToF(BIB::Arcade::DifficultyName(Breach.GetDifficulty())).ToUpper()),
			ToF(BIB::Meta::Num(Breach.Score())), FString::Printf(TEXT("Stop the cursor in the open port: %s, Enter or click   Esc: give up"), *KeyLabel(Settings, 4)));
		// The layer stack on the left: breached layers glow.
		const float StackX = Size.X * 0.06f;
		const float StackTop = Size.Y * 0.18f;
		const float RowH = FMath::Min(28.f, (Size.Y * 0.6f) / FMath::Max(1, Rules.Layers));
		Paint.Text(TEXT("LAYERS"), FVector2D(StackX, StackTop - 26.f), 13, Dim);
		for (int32 Layer = 0; Layer < Rules.Layers; ++Layer)
		{
			const float Y = StackTop + (Rules.Layers - 1 - Layer) * RowH;
			const bool bDone = Layer < Breach.LayersBreached();
			const bool bNow = Layer == Breach.LayersBreached() && !Breach.IsOver();
			Paint.Box(FVector2D(StackX, Y), FVector2D(140.f, RowH - 4.f), bDone ? FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.6f) : (bNow ? FLinearColor(Pink.R, Pink.G, Pink.B, 0.5f) : Panel));
			Paint.Text(FString::Printf(TEXT("L%02d"), Layer + 1), FVector2D(StackX + 8.f, Y + 2.f), 11, bDone ? Void : Ink);
		}
		// Integrity and the trace.
		const float InfoX = Size.X * 0.78f;
		Paint.Text(TEXT("INTEGRITY"), FVector2D(InfoX, Size.Y * 0.2f), 13, Dim);
		for (int32 Pip = 0; Pip < Rules.Integrity; ++Pip)
		{
			Paint.Box(FVector2D(InfoX + Pip * 30.f, Size.Y * 0.2f + 24.f), FVector2D(22.f, 22.f), Pip < Breach.Integrity() ? Cyan : FLinearColor(1.f, 1.f, 1.f, 0.1f));
		}
		const float Trace = static_cast<float>(FMath::Clamp(Breach.TraceLeft() / FMath::Max(0.01, Rules.TraceSeconds), 0.0, 1.0));
		Paint.Text(TEXT("TRACE"), FVector2D(InfoX, Size.Y * 0.2f + 64.f), 13, Dim);
		Paint.Box(FVector2D(InfoX, Size.Y * 0.2f + 88.f), FVector2D(160.f, 12.f), Panel);
		Paint.Box(FVector2D(InfoX, Size.Y * 0.2f + 88.f), FVector2D(160.f * Trace, 12.f), Trace < 0.3f ? Danger : Gold);
		Paint.Text(FString::Printf(TEXT("Perfect %d   Breached %d   Blocked %d"), Breach.Perfects(), Breach.Goods(), Breach.Misses()), FVector2D(InfoX, Size.Y * 0.2f + 116.f), 12, Ink, 0, false);
		// The bar: the open port, its Perfect core and the sweeping cursor.
		const FVector2D BarPos(Size.X * 0.22f, Size.Y * 0.46f);
		const FVector2D BarSize(Size.X * 0.52f, 56.f);
		Paint.Text(FString::Printf(TEXT("LAYER %d / %d"), FMath::Min(Breach.Layer(), Rules.Layers), Rules.Layers), FVector2D(BarPos.X + BarSize.X * 0.5f, BarPos.Y - 48.f), 22, Cream, 1);
		Paint.Box(BarPos, BarSize, FLinearColor(0.06f, 0.05f, 0.1f, 1.f));
		for (int32 Tick = 1; Tick < 20; ++Tick)
		{
			Paint.Box(FVector2D(BarPos.X + BarSize.X * Tick / 20.f, BarPos.Y), FVector2D(1.f, BarSize.Y), FLinearColor(1.f, 1.f, 1.f, 0.06f));
		}
		const float PortL = static_cast<float>(Breach.PortCentre() - Breach.PortHalfWidth());
		const float PortW = static_cast<float>(Breach.PortHalfWidth() * 2.0);
		Paint.Box(FVector2D(BarPos.X + BarSize.X * PortL, BarPos.Y), FVector2D(BarSize.X * PortW, BarSize.Y), FLinearColor(0.2f, 0.85f, 0.45f, 0.55f));
		const float CoreW = PortW * static_cast<float>(BIB::Arcade::BreachPerfectShare);
		Paint.Box(FVector2D(BarPos.X + BarSize.X * (static_cast<float>(Breach.PortCentre()) - CoreW * 0.5f), BarPos.Y), FVector2D(BarSize.X * CoreW, BarSize.Y), FLinearColor(Gold.R, Gold.G, Gold.B, 0.8f));
		Paint.Frame(BarPos, BarSize, Cyan, 2.f);
		const float CursorX = BarPos.X + BarSize.X * static_cast<float>(Breach.Cursor());
		Paint.Box(FVector2D(CursorX - 2.f, BarPos.Y - 10.f), FVector2D(4.f, BarSize.Y + 20.f), Pink);
		Hit(BarPos - FVector2D(0.f, 20.f), BarSize + FVector2D(0.f, 40.f), TEXT("press"));
		if (Breach.LastJudge() != BIB::Arcade::EBreachJudge::None && Breach.Time() - Breach.LastJudgeTime() < 0.6)
		{
			const BIB::Arcade::EBreachJudge Judge = Breach.LastJudge();
			Paint.Text(ToF(BIB::Arcade::BreachJudgeName(Judge)), FVector2D(BarPos.X + BarSize.X * 0.5f, BarPos.Y + BarSize.Y + 28.f), 28,
				Judge == BIB::Arcade::EBreachJudge::Perfect ? Gold : (Judge == BIB::Arcade::EBreachJudge::Miss ? Danger : Cream), 1);
		}
		Button(Paint, FVector2D(Size.X * 0.4f, Size.Y * 0.72f), FVector2D(Size.X * 0.2f, 64.f), TEXT("BREACH"), KeyLabel(Settings, 4), TEXT("press"), false, Pink);
		if (Breach.IsOver() && !bShowResult)
		{
			Paint.Text(Breach.IsWon() ? TEXT("ACCESS GRANTED") : TEXT("ACCESS DENIED"), FVector2D(Size.X * 0.5f, Size.Y * 0.3f), 40, Breach.IsWon() ? Gold : Danger, 1);
		}
	}

	// ------------------------------------------------------------------------------------------ painting: arena replay
	void PaintArena(const FPainter& Paint, const FVector2D& Size) const
	{
		Paint.Text(TEXT("ARENA"), FVector2D(Size.X * 0.5f, 20.f), 30, Pink, 1);
		Paint.Text(FString::Printf(TEXT("%s  vs  %s"), *ToF(Attack.Name), *ToF(Defence.Name)), FVector2D(Size.X * 0.5f, 62.f), 16, Ink, 1);
		Paint.Text(TEXT("Space: skip"), FVector2D(Size.X - 24.f, 20.f), 12, Dim, 2, false);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const BIB::Arena::FLineup& Lineup = Side == 0 ? Attack : Defence;
			const float ColumnX = Side == 0 ? Size.X * 0.12f : Size.X * 0.62f;
			for (int32 Index = 0; Index < static_cast<int32>(Lineup.Team.size()); ++Index)
			{
				const BIB::Arena::FFighter& Fighter = Lineup.Team[static_cast<size_t>(Index)];
				const float Y = Size.Y * 0.18f + Index * Size.Y * 0.25f;
				const float Face = Size.Y * 0.18f;
				const bool bDown = Hp[Side].IsValidIndex(Index) && Hp[Side][Index] <= 0.f;
				FString Name = Fighter.Op >= 0 && Fighter.Op < static_cast<int32>(BIB::Roster().size()) ? ToF(BIB::Roster()[static_cast<size_t>(Fighter.Op)].Name) : TEXT("?");
				Paint.Image(Portrait(Name.ToLower()), FVector2D(ColumnX, Y), FVector2D(Face, Face), bDown ? FLinearColor(0.3f, 0.3f, 0.3f, 0.6f) : FLinearColor::White);
				Paint.Text(FString::Printf(TEXT("%s  Lv %d"), *Name, Fighter.Level), FVector2D(ColumnX + Face + 14.f, Y + 6.f), 16, bDown ? Dim : Cream);
				const float BarW = Size.X * 0.22f;
				const float Frac = MaxHp[Side].IsValidIndex(Index) && MaxHp[Side][Index] > 0.f ? Hp[Side][Index] / MaxHp[Side][Index] : 0.f;
				Paint.Box(FVector2D(ColumnX + Face + 14.f, Y + 40.f), FVector2D(BarW, 14.f), Panel);
				Paint.Box(FVector2D(ColumnX + Face + 14.f, Y + 40.f), FVector2D(BarW * Frac, 14.f), Side == 0 ? Cyan : Pink);
				Paint.Text(ToF(BIB::Meta::Num(static_cast<long long>(FMath::Max(0.f, Hp[Side].IsValidIndex(Index) ? Hp[Side][Index] : 0.f)))),
					FVector2D(ColumnX + Face + 14.f, Y + 60.f), 12, Dim, 0, false);
				for (const FFloat& Float : Floats)
				{
					const double Age = Clock - Float.Born;
					if (FMath::RoundToInt(Float.Where.X) == Side && FMath::RoundToInt(Float.Where.Y) == Index && Age < 0.9)
					{
						const float Rise = bReduceMotion ? 0.f : static_cast<float>(Age) * 40.f;
						Paint.Text(Float.Text, FVector2D(ColumnX + Face * 0.5f, Y + Face * 0.3f - Rise), Float.bBig ? 22 : 16,
							FLinearColor(Float.bBig ? Gold.R : 1.f, Float.bBig ? Gold.G : 1.f, Float.bBig ? Gold.B : 1.f, static_cast<float>(1.0 - Age / 0.9)), 1);
					}
				}
			}
		}
	}
};

// ================================================================================================ prompt and key capture
class SBIBPromptOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBIBPromptOverlay) {}
	SLATE_END_ARGS()

	TFunction<void(bool, const FString&)> OnDone;
	TFunction<void()> RequestClose;
	TSharedPtr<SEditableTextBox> TextBox;
	bool bCapture = false;

	void Construct(const FArguments& InArgs, const FString& Title, const FString& Hint, const FString& Initial, int32 InMaxLength, bool bInCapture)
	{
		bCapture = bInCapture;
		MaxLength = InMaxLength;
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock).Text(FText::FromString(Title)).Font(FCoreStyle::GetDefaultFontStyle("Bold", 22)).ColorAndOpacity(Cream)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
			[
				SNew(STextBlock).Text(FText::FromString(Hint)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).ColorAndOpacity(Ink).AutoWrapText(true)
			];
		if (!bCapture)
		{
			Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
			[
				SAssignNew(TextBox, SEditableTextBox)
					.Text(FText::FromString(Initial))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
					.OnTextChanged_Lambda([this](const FText& Text)
					{
						if (TextBox.IsValid() && Text.ToString().Len() > MaxLength)
						{
							TextBox->SetText(FText::FromString(Text.ToString().Left(MaxLength)));
						}
					})
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type Commit)
					{
						if (Commit == ETextCommit::OnEnter)
						{
							Close(true, Text.ToString());
						}
					})
			];
			Box->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SButton).Text(FText::FromString(TEXT("OK"))).OnClicked_Lambda([this]()
					{
						Close(true, TextBox.IsValid() ? TextBox->GetText().ToString() : FString());
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(FText::FromString(TEXT("Cancel"))).OnClicked_Lambda([this]()
					{
						Close(false, FString());
						return FReply::Handled();
					})
				]
			];
		}
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.7f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(620.f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(Panel)
					.Padding(FMargin(28.f))
					[
						Box
					]
				]
			]
		];
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override
	{
		const FString Key = bCapture ? KeyEvent.GetKey().GetFName().ToString() : OverlayKeyName(KeyEvent.GetKey());   // a capture takes the raw key
		if (bCapture)
		{
			Close(true, Key);
			return FReply::Handled();
		}
		if (Key == TEXT("Escape"))
		{
			Close(false, FString());
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bCapture)
		{
			Close(true, MouseEvent.GetEffectingButton().GetFName().ToString());
		}
		return FReply::Handled();
	}

	void Close(bool bOk, const FString& Text)
	{
		if (bClosed)
		{
			return;
		}
		bClosed = true;
		if (OnDone)
		{
			OnDone(bOk, Text.TrimStartAndEnd());
		}
		if (RequestClose)
		{
			RequestClose();
		}
	}

private:
	int32 MaxLength = 60;
	bool bClosed = false;
};

// ================================================================================================ damage numbers
class SBIBDamageLayer : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBIBDamageLayer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, APlayerController* InController)
	{
		Controller = InController;
		SetVisibility(EVisibility::HitTestInvisible);
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
		{
			Invalidate(EInvalidateWidgetReason::Paint);
			return EActiveTimerReturnType::Continue;
		}));
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		APlayerController* PC = Controller.Get();
		UWorld* World = PC ? PC->GetWorld() : nullptr;
		ABIBGameMode* Mode = World ? World->GetAuthGameMode<ABIBGameMode>() : nullptr;
		UBIBGameInstance* Instance = World ? Cast<UBIBGameInstance>(World->GetGameInstance()) : nullptr;
		if (!Mode || !Instance || !Mode->IsInStage())
		{
			return LayerId;
		}
		const BIB::FSettings& Settings = Instance->GetProfile().Meta.Settings;
		if (!Settings.bDamageNumbers)
		{
			return LayerId;
		}
		// Anime style: numbers move with the held 12 fps frames (they are re-placed only on a new visual step) and
		// carry a thick ink outline like the world's lines.
		const int64 Step = ABIBAnimeStyleVolume::GetVisualStep(World);
		const float Scale = AllottedGeometry.Scale > 0.f ? AllottedGeometry.Scale : 1.f;
		if (Step < 0 || Step != CachedStep)
		{
			CachedStep = Step;
			Cached.Reset();
			const float Now = World->GetTimeSeconds();
			for (const FBIBDamagePopup& Popup : Mode->GetDamagePopups())
			{
				const float Age = Now - Popup.Time;
				if (Age < 0.f || Age > 0.9f)
				{
					continue;
				}
				FVector2D Screen;
				if (!PC->ProjectWorldLocationToScreen(Popup.Location, Screen, true))
				{
					continue;
				}
				const float Rise = Settings.bReduceMotion ? 0.f : Age * 50.f;
				FCachedNumber& Number = Cached.AddDefaulted_GetRef();
				Number.Text = FString::FormatAsNumber(FMath::RoundToInt(Popup.Amount));
				Number.Pos = Screen / Scale - FVector2D(0.f, Rise);
				// Zenless-style numbers (2026-10-09): heavy numerals that punch in big and settle; crits / stuns / bosses in gold-rose.
				const float Punch = 1.f + 0.6f * FMath::Clamp(1.f - Age / 0.12f, 0.f, 1.f);
				Number.Size = FMath::RoundToInt((Popup.bBig ? 26.f : 19.f) * Punch);
				Number.Colour = Popup.bBig ? FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.78f, 0.2f), FLinearColor(1.f, 0.24f, 0.55f), FMath::Clamp(Age / 0.5f, 0.f, 1.f))
					: FLinearColor(0.97f, 0.96f, 0.98f);
				Number.Colour.A = 1.f - FMath::Square(Age / 0.9f);
			}
		}
		const FPainter Paint{AllottedGeometry, OutDrawElements, LayerId, FMath::Clamp(Settings.HudScale, 75, 125) / 100.f};
		for (const FCachedNumber& Number : Cached)
		{
			FSlateFontInfo Font = BIBFonts::Numeral(FMath::RoundToInt(Number.Size * Paint.TextScale));
			Font.OutlineSettings.OutlineSize = 3;
			Font.OutlineSettings.OutlineColor = FLinearColor(0.1f, 0.05f, 0.3f, Number.Colour.A);   // violet ink
			const FVector2D Extent = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Number.Text, Font);
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(Extent, FSlateLayoutTransform(Number.Pos - FVector2D(Extent.X * 0.5f, 0.f))),
				Number.Text, Font, ESlateDrawEffect::None, Number.Colour);
		}
		return LayerId + 2;
	}

private:
	struct FCachedNumber
	{
		FString Text;
		FVector2D Pos = FVector2D::ZeroVector;
		int32 Size = 16;
		FLinearColor Colour = FLinearColor::White;
	};

	TWeakObjectPtr<APlayerController> Controller;
	mutable int64 CachedStep = -2;
	mutable TArray<FCachedNumber> Cached;
};

// ================================================================================================ host
namespace BIBOverlay
{
	namespace
	{
		TSharedPtr<SWidget> Current;
		TWeakPtr<SBIBGameOverlay> CurrentGame;
		TWeakPtr<SBIBPromptOverlay> CurrentPrompt;
		TWeakObjectPtr<UWorld> CurrentWorld;

		void Remove()
		{
			if (Current.IsValid())
			{
				if (UWorld* World = CurrentWorld.Get())
				{
					if (UGameViewportClient* Viewport = World->GetGameViewport())
					{
						Viewport->RemoveViewportWidgetContent(Current.ToSharedRef());
					}
				}
			}
			Current.Reset();
			CurrentGame.Reset();
			CurrentPrompt.Reset();
			if (UWorld* World = CurrentWorld.Get())
			{
				if (APlayerController* PC = World->GetFirstPlayerController())
				{
					FInputModeUIOnly Mode;
					Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
					PC->SetInputMode(Mode);
					PC->bShowMouseCursor = true;
				}
			}
		}

		void Show(UWorld* World, const TSharedRef<SWidget>& Widget)
		{
			Remove();
			UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
			if (!Viewport)
			{
				return;
			}
			Current = Widget;
			CurrentWorld = World;
			Viewport->AddViewportWidgetContent(Widget, 200);
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				FInputModeUIOnly Mode;
				Mode.SetWidgetToFocus(Widget);
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PC->SetInputMode(Mode);
				PC->bShowMouseCursor = true;
			}
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().SetAllUserFocus(Widget, EFocusCause::SetDirectly);
			}
		}
	}

	void OpenGame(UWorld* World, UBIBGameInstance* Instance, EBIBOverlayKind Kind, int32 A, int32 B, TFunction<void(const FString&)> OnDone)
	{
		TSharedRef<SBIBGameOverlay> Widget = SNew(SBIBGameOverlay, World, Instance, Kind, A, B);
		Widget->OnDone = MoveTemp(OnDone);
		TWeakPtr<SBIBGameOverlay> Weak = Widget;
		Widget->RequestClose = [Weak]()
		{
			// Remove on the next tick: the widget is still on the call stack.
			if (FSlateApplication::IsInitialized())
			{
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
				{
					if (Weak.IsValid() && Current == StaticCastSharedPtr<SWidget>(Weak.Pin()))
					{
						Remove();
					}
					return false;
				}));
			}
		};
		Show(World, Widget);
		CurrentGame = Widget;
		UE_LOG(LogBIBArcade, Display, TEXT("Arcade overlay opened (kind %d, %d, %d)"), static_cast<int32>(Kind), A, B);
	}

	void OpenPrompt(UWorld* World, const FString& Title, const FString& Hint, const FString& Initial, int32 MaxLength, TFunction<void(bool, const FString&)> OnDone)
	{
		TSharedRef<SBIBPromptOverlay> Widget = SNew(SBIBPromptOverlay, Title, Hint, Initial, MaxLength, false);
		Widget->OnDone = MoveTemp(OnDone);
		TWeakPtr<SBIBPromptOverlay> Weak = Widget;
		Widget->RequestClose = [Weak]()
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
			{
				if (Weak.IsValid() && Current == StaticCastSharedPtr<SWidget>(Weak.Pin()))
				{
					Remove();
				}
				return false;
			}));
		};
		Show(World, Widget);
		CurrentPrompt = Widget;
		if (Widget->TextBox.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocus(Widget->TextBox, EFocusCause::SetDirectly);
		}
	}

	void OpenKeyCapture(UWorld* World, const FString& Label, TFunction<void(const FString&)> OnDone)
	{
		TSharedRef<SBIBPromptOverlay> Widget = SNew(SBIBPromptOverlay, TEXT("Press a key"), Label, FString(), 0, true);
		Widget->OnDone = [Done = MoveTemp(OnDone)](bool, const FString& Key)
		{
			if (Done)
			{
				Done(Key);
			}
		};
		TWeakPtr<SBIBPromptOverlay> Weak = Widget;
		Widget->RequestClose = [Weak]()
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
			{
				if (Weak.IsValid() && Current == StaticCastSharedPtr<SWidget>(Weak.Pin()))
				{
					Remove();
				}
				return false;
			}));
		};
		Show(World, Widget);
		CurrentPrompt = Widget;
	}

	bool IsOpen()
	{
		return Current.IsValid();
	}

	void CloseAll()
	{
		if (TSharedPtr<SBIBGameOverlay> Game = CurrentGame.Pin())
		{
			Game->DebugCommand(TEXT("close"));
		}
		Remove();
	}

	FString DebugState()
	{
		if (TSharedPtr<SBIBGameOverlay> Game = CurrentGame.Pin())
		{
			return Game->DebugState();
		}
		return Current.IsValid() ? FString(TEXT("prompt")) : FString(TEXT("none"));
	}

	bool DebugCommand(const FString& Command)
	{
		if (TSharedPtr<SBIBGameOverlay> Game = CurrentGame.Pin())
		{
			return Game->DebugCommand(Command);
		}
		if (TSharedPtr<SBIBPromptOverlay> Prompt = CurrentPrompt.Pin())
		{
			// prompt:text:<answer>, prompt:key:<key name>, prompt:cancel
			if (Command.StartsWith(TEXT("prompt:text:")) || Command.StartsWith(TEXT("prompt:key:")))
			{
				Prompt->Close(true, Command.StartsWith(TEXT("prompt:text:")) ? Command.RightChop(12) : Command.RightChop(11));
				return true;
			}
			if (Command == TEXT("prompt:cancel"))
			{
				Prompt->Close(false, FString());
				return true;
			}
		}
		return false;
	}

	TSharedRef<SWidget> MakeDamageLayer(APlayerController* Controller)
	{
		return SNew(SBIBDamageLayer, Controller);
	}
}
