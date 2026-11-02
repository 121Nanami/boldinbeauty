#include "BIBGame/BIBTouch.h"
#include "BIBGame/BIBFonts.h"
#include "BIBGame/BIBMotion.h"
#include "BIBGame/BIBMotionEvents.h"

#include "BIBGame/BIBBlueprintLibrary.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBGame/BIBPlayerController.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

namespace BIBTouchInternal
{
	using FWeakPC = TWeakObjectPtr<ABIBPlayerController>;

	const FLinearColor Rose(1.f, 0.24f, 0.55f, 1.f);
	const FLinearColor Violet(0.46f, 0.34f, 1.f, 1.f);
	const FLinearColor Cyan(0.39f, 0.91f, 1.f, 1.f);
	const FLinearColor Ink(0.93f, 0.9f, 0.95f, 1.f);
	const FLinearColor Ground(0.0035f, 0.0045f, 0.0075f, 1.f);

	// Rounded-box brushes need the real corner radius (half the button's size), so they are cached per size.
	const FSlateBrush* Disc(double Radius)
	{
		static TMap<int32, TSharedPtr<FSlateRoundedBoxBrush>> Cache;
		const int32 Key = FMath::Max(2, FMath::RoundToInt(Radius));
		if (const TSharedPtr<FSlateRoundedBoxBrush>* Found = Cache.Find(Key))
		{
			return Found->Get();
		}
		return Cache.Add(Key, MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, static_cast<float>(Key))).Get();
	}

	FSlateFontInfo Label(int32 Size)
	{
		return BIBFonts::Display(Size);
	}

	enum class EButton : uint8
	{
		Attack,
		Dodge,
		Parry,
		Jump,
		Block,
		Move1,
		Move2,
		Move3,
		Special,
		Ultimate,
		Switch,
		Talk,
		Pause,
		Interact,
		Count
	};

	struct FButtonDef
	{
		EButton Id;
		FVector2D Centre;   // in a 1920 x 1080 frame, anchored to the bottom-right corner (Talk / Pause: see Place)
		float Radius;
		const TCHAR* Text;
		int32 AbilityIndex; // HUD ability shown on it (cooldown, name), -1 = none
	};

	// The arc around the attack button, like the big mobile action games.
	const FButtonDef Buttons[] = {
		{EButton::Attack, FVector2D(1710.0, 870.0), 96.f, TEXT("ATTACK"), 0},
		{EButton::Dodge, FVector2D(1560.0, 960.0), 62.f, TEXT("DODGE"), -1},
		{EButton::Parry, FVector2D(1548.0, 812.0), 50.f, TEXT("PARRY"), -1},
		{EButton::Jump, FVector2D(1840.0, 716.0), 56.f, TEXT("JUMP"), -1},
		{EButton::Block, FVector2D(1430.0, 626.0), 42.f, TEXT("BLOCK"), -1},
		{EButton::Move1, FVector2D(1462.0, 722.0), 52.f, TEXT("1"), 1},
		{EButton::Move2, FVector2D(1578.0, 664.0), 52.f, TEXT("2"), 2},
		{EButton::Move3, FVector2D(1700.0, 640.0), 52.f, TEXT("3"), 3},
		{EButton::Special, FVector2D(1830.0, 560.0), 56.f, TEXT("SP"), 4},
		{EButton::Ultimate, FVector2D(1420.0, 900.0), 68.f, TEXT("ULT"), 5},
		{EButton::Switch, FVector2D(1840.0, 420.0), 46.f, TEXT("SWITCH"), -1},
		{EButton::Talk, FVector2D(1250.0, 980.0), 40.f, TEXT("TALK"), -1},
		{EButton::Pause, FVector2D(1860.0, 150.0), 34.f, TEXT("II"), -1},
		{EButton::Interact, FVector2D(1560.0, 840.0), 64.f, TEXT("ENTER"), -1},
	};

	class STouchLayer : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(STouchLayer) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, ABIBPlayerController* InPC)
		{
			PC = InPC;
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D(1.0, 1.0);
		}

		bool InBattle() const
		{
			const ABIBGameMode* Mode = PC.IsValid() ? PC->GetBIBGameMode() : nullptr;
			const ABIBPlayerCharacter* Char = PC.IsValid() ? PC->GetBIBCharacter() : nullptr;
			return Mode && Mode->IsInStage() && Char && Char->IsCombatActive();
		}

		bool Shown(EButton Id) const
		{
			const bool bBattle = InBattle();
			switch (Id)
			{
			case EButton::Jump:
				return true;
			case EButton::Interact:
				return !bBattle;
			case EButton::Talk:
				return bBattle;
			default:
				return bBattle;
			}
		}

		FVector2D Place(const FButtonDef& Def, const FVector2D& Size) const
		{
			// Uniform scale from 1080p; buttons keep their distance to the bottom-right corner on any aspect ratio.
			const double Scale = Size.Y / 1080.0;
			const FVector2D FromCorner = FVector2D(1920.0, 1080.0) - Def.Centre;
			return Size - FromCorner * Scale;
		}

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer,
			const FWidgetStyle&, bool) const override
		{
			const FVector2D Size = Geometry.GetLocalSize();
			const double Scale = Size.Y / 1080.0;
			const FBIBHudInfo Info = PC.IsValid() ? UBIBBlueprintLibrary::GetHudInfo(PC.Get()) : FBIBHudInfo();
			// Joystick: a resting hint, or the live base and knob.
			const FVector2D Base = JoyPointer >= 0 ? JoyOrigin : FVector2D(260.0 * Scale, Size.Y - 260.0 * Scale);
			const double JoyRadius = 120.0 * Scale;
			const FVector2D Knob = JoyPointer >= 0 ? JoyOrigin + JoyVector * JoyRadius : Base;
			auto Circle = [&Out, &Geometry, Layer](const FVector2D& Centre, double Radius, const FLinearColor& Colour, bool bUnused, int32 Up)
			{
				(void)bUnused;
				const FSlateBrush* Brush = Disc(Radius);
				FSlateDrawElement::MakeBox(Out, Layer + Up, Geometry.ToPaintGeometry(FVector2f(Radius * 2.0, Radius * 2.0),
					FSlateLayoutTransform(FVector2f(Centre - FVector2D(Radius, Radius)))), Brush, ESlateDrawEffect::None, Colour);
			};
			auto Text = [&Out, &Geometry, Layer](const FVector2D& Centre, const FString& Value, int32 FontSize, const FLinearColor& Colour)
			{
				const FSlateFontInfo Font = Label(FontSize);
				const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value, Font);
				FSlateDrawElement::MakeText(Out, Layer + 3, Geometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(Centre - TextSize * 0.5))),
					Value, Font, ESlateDrawEffect::None, Colour);
			};
			// The motion pack's joysticks are the stick's art: the rose ring in the Ark, the tactical dial in fights, the 8-way hex while
			// climbing or swimming; it plays its intro when the thumb lands, then idles. Without the pack: plain circles.
			const ABIBPlayerCharacter* Char = PC.IsValid() ? PC->GetBIBCharacter() : nullptr;
			const bool bTraverse = Char && !Char->GetHud().MoveState.IsEmpty();
			const TCHAR* Stick = bTraverse ? TEXT("joystick-hex") : (InBattle() ? TEXT("joystick-tactical") : TEXT("joystick-ring"));
			const double SinceDown = FPlatformTime::Seconds() - JoyDownAt;
			const bool bArt = BIBMotion::PaintFrame(Stick, SinceDown < 0.5 ? TEXT("intro") : TEXT("idle"), SinceDown < 0.5 ? SinceDown : SinceDown - 0.5,
				SinceDown >= 0.5, Geometry, Out, Layer, Base, static_cast<float>(JoyRadius / 230.0 * 1.15), JoyPointer >= 0 ? 0.95f : 0.55f);
			if (!bArt)
			{
				Circle(Base, JoyRadius, FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.35f), false, 0);
				Circle(Base, JoyRadius - 3.0 * Scale, FLinearColor(Ground.R, Ground.G, Ground.B, JoyPointer >= 0 ? 0.55f : 0.3f), false, 1);
			}
			Circle(Knob, 46.0 * Scale, FLinearColor(Rose.R, Rose.G, Rose.B, JoyPointer >= 0 ? 0.9f : 0.45f), false, 2);

			for (const FButtonDef& Def : Buttons)
			{
				if (!Shown(Def.Id))
				{
					continue;
				}
				const FVector2D Centre = Place(Def, Size);
				const double Radius = Def.Radius * Scale;
				const bool bHeld = HeldButtons.Contains(static_cast<uint8>(Def.Id));
				float Cooldown = 0.f;
				FString Name;
				if (Def.AbilityIndex >= 0 && Info.Abilities.IsValidIndex(Def.AbilityIndex))
				{
					Cooldown = Info.Abilities[Def.AbilityIndex].CooldownRemaining;
				}
				const bool bUlt = Def.Id == EButton::Ultimate;
				const bool bUltReady = bUlt && Info.UltFraction >= 1.f;
				FLinearColor Fill = bHeld ? Rose : FLinearColor(Ground.R, Ground.G, Ground.B, 0.6f);
				if (bUltReady && !bHeld)
				{
					Fill = FLinearColor::LerpUsingHSV(Violet, Rose, 0.5f + 0.5f * FMath::Sin(static_cast<float>(FPlatformTime::Seconds()) * 5.f));
					Fill.A = 0.85f;
				}
				const FLinearColor Edge = Def.Id == EButton::Attack ? Rose : (bUlt ? Violet : Cyan);
				Circle(Centre, Radius, FLinearColor(Edge.R, Edge.G, Edge.B, Cooldown > 0.f ? 0.3f : 0.9f), false, 0);
				Circle(Centre, Radius - 3.0 * Scale, Fill, false, 1);
				if (bUlt && !bUltReady)
				{
					// The ultimate's charge fills the button from the centre.
					Circle(Centre, Radius * FMath::Clamp(Info.UltFraction, 0.f, 1.f), FLinearColor(Violet.R, Violet.G, Violet.B, 0.45f), false, 2);
				}
				const int32 FontSize = FMath::Max(10, FMath::RoundToInt((Def.Id == EButton::Attack ? 30.f : 20.f) * static_cast<float>(Scale)));
				if (Cooldown > 0.f)
				{
					Text(Centre, Cooldown >= 10.f ? FString::FromInt(FMath::CeilToInt(Cooldown)) : FString::Printf(TEXT("%.1f"), Cooldown), FontSize + 4, Ink);
				}
				else
				{
					Text(Centre, Def.Text, FontSize, Ink);
				}
			}
			return Layer + 4;
		}

		const FButtonDef* HitButton(const FGeometry& Geometry, const FVector2D& Local) const
		{
			const FVector2D Size = Geometry.GetLocalSize();
			const double Scale = Size.Y / 1080.0;
			for (const FButtonDef& Def : Buttons)
			{
				// a little bigger than drawn: thumbs are wide
				if (Shown(Def.Id) && FVector2D::Distance(Place(Def, Size), Local) <= Def.Radius * Scale * 1.2)
				{
					return &Def;
				}
			}
			return nullptr;
		}

		void SetButton(EButton Id, bool bDown)
		{
			ABIBPlayerController* Controller = PC.Get();
			if (!Controller)
			{
				return;
			}
			const FBIBHudInfo Info = UBIBBlueprintLibrary::GetHudInfo(Controller);
			auto KeyOf = [&Info](int32 Index, EBIBInput Fallback)
			{
				// Special and Ultimate sit on 4 / E for most operators, R for some (the HUD shows the real key).
				if (Info.Abilities.IsValidIndex(Index))
				{
					const FString& Key = Info.Abilities[Index].Key;
					if (Key == TEXT("R")) { return EBIBInput::UltimateR; }
					if (Key == TEXT("E")) { return EBIBInput::UltimateE; }
					if (Key == TEXT("4")) { return EBIBInput::Special; }
				}
				return Fallback;
			};
			switch (Id)
			{
			case EButton::Attack: Controller->SetVirtualInput(EBIBInput::Attack, bDown); break;
			case EButton::Dodge:
				// MAIN §6: dodge = dash held + parry.
				Controller->SetVirtualInput(EBIBInput::Run, bDown);
				Controller->SetVirtualInput(EBIBInput::Parry, bDown);
				break;
			case EButton::Parry: Controller->SetVirtualInput(EBIBInput::Parry, bDown); break;
			case EButton::Jump: Controller->SetVirtualInput(EBIBInput::Jump, bDown); break;
			case EButton::Block: Controller->SetVirtualInput(EBIBInput::Block, bDown); break;
			case EButton::Move1: Controller->SetVirtualInput(EBIBInput::Move1, bDown); break;
			case EButton::Move2: Controller->SetVirtualInput(EBIBInput::Move2, bDown); break;
			case EButton::Move3: Controller->SetVirtualInput(EBIBInput::Move3, bDown); break;
			case EButton::Special: Controller->SetVirtualInput(KeyOf(4, EBIBInput::Special), bDown); break;
			case EButton::Ultimate: Controller->SetVirtualInput(KeyOf(5, EBIBInput::UltimateE), bDown); break;
			case EButton::Switch: Controller->SetVirtualInput(EBIBInput::SwitchOperator, bDown); break;
			case EButton::Talk: Controller->SetVirtualInput(EBIBInput::Talk, bDown); break;
			case EButton::Pause: Controller->SetVirtualInput(EBIBInput::Pause, bDown); break;
			case EButton::Interact: Controller->SetVirtualInput(EBIBInput::Parry, bDown); break;
			default: break;
			}
		}

		FReply Down(const FGeometry& Geometry, int32 Pointer, const FVector2D& Screen)
		{
			const FVector2D Local = Geometry.AbsoluteToLocal(Screen);
			if (const FButtonDef* Hit = HitButton(Geometry, Local))
			{
				PointerButtons.Add(Pointer, static_cast<uint8>(Hit->Id));
				HeldButtons.Add(static_cast<uint8>(Hit->Id));
				SetButton(Hit->Id, true);
				if (Hit->Id == EButton::Switch || Hit->Id == EButton::Dodge || Hit->Id == EButton::Block)
				{
					BIBMotionEvents::TouchPress(Hit->Id == EButton::Switch ? TEXT("switch") : (Hit->Id == EButton::Dodge ? TEXT("dodge") : TEXT("block")), Place(*Hit, Geometry.GetLocalSize()));
				}
				return FReply::Handled();
			}
			if (Local.X < Geometry.GetLocalSize().X * 0.45 && JoyPointer < 0)
			{
				JoyPointer = Pointer;
				JoyOrigin = Local;
				JoyDownAt = FPlatformTime::Seconds();
				JoyVector = FVector2D::ZeroVector;
				return FReply::Handled();
			}
			if (LookPointer < 0)
			{
				LookPointer = Pointer;
				LookLast = Local;
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		FReply Move(const FGeometry& Geometry, int32 Pointer, const FVector2D& Screen)
		{
			const FVector2D Local = Geometry.AbsoluteToLocal(Screen);
			const double Scale = Geometry.GetLocalSize().Y / 1080.0;
			if (Pointer == JoyPointer)
			{
				const double Radius = 120.0 * Scale;
				FVector2D Offset = Local - JoyOrigin;
				if (Offset.Size() > Radius * 1.6)
				{
					// The base follows a thumb that slides far away (floating stick).
					JoyOrigin = Local - Offset.GetSafeNormal() * Radius * 1.6;
					Offset = Local - JoyOrigin;
				}
				JoyVector = (Offset / Radius).GetClampedToMaxSize(1.0);
				PushMove();
				return FReply::Handled();
			}
			if (Pointer == LookPointer)
			{
				if (ABIBPlayerController* Controller = PC.Get())
				{
					Controller->AddTouchLook((Local - LookLast) / FMath::Max(0.5, Scale));
				}
				LookLast = Local;
				return FReply::Handled();
			}
			return PointerButtons.Contains(Pointer) ? FReply::Handled() : FReply::Unhandled();
		}

		FReply Up(int32 Pointer)
		{
			if (Pointer == JoyPointer)
			{
				JoyPointer = -1;
				JoyVector = FVector2D::ZeroVector;
				PushMove();
				return FReply::Handled();
			}
			if (Pointer == LookPointer)
			{
				LookPointer = -1;
				return FReply::Handled();
			}
			if (const uint8* Id = PointerButtons.Find(Pointer))
			{
				const EButton Button = static_cast<EButton>(*Id);
				HeldButtons.Remove(*Id);
				PointerButtons.Remove(Pointer);
				SetButton(Button, false);
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		void PushMove()
		{
			ABIBPlayerController* Controller = PC.Get();
			ABIBPlayerCharacter* Char = Controller ? Controller->GetBIBCharacter() : nullptr;
			if (Char)
			{
				// Screen up = forward.
				Char->SetTouchMove(FVector2D(JoyVector.X, -JoyVector.Y));
			}
			if (Controller)
			{
				// Pushed to the rim: run (released buttons of the dodge also clear Run, so keep it tied to the stick).
				Controller->SetVirtualInput(EBIBInput::Run, JoyVector.Size() > 0.85 || HeldButtons.Contains(static_cast<uint8>(EButton::Dodge)));
			}
		}

		virtual FReply OnTouchStarted(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			return Down(Geometry, static_cast<int32>(Event.GetPointerIndex()), Event.GetScreenSpacePosition());
		}
		virtual FReply OnTouchMoved(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			return Move(Geometry, static_cast<int32>(Event.GetPointerIndex()), Event.GetScreenSpacePosition());
		}
		virtual FReply OnTouchEnded(const FGeometry&, const FPointerEvent& Event) override
		{
			return Up(static_cast<int32>(Event.GetPointerIndex()));
		}
		// Desktop testing (-BIBTouch): the mouse is one finger.
		virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			const FReply Reply = Down(Geometry, 100, Event.GetScreenSpacePosition());
			return Reply.IsEventHandled() ? FReply::Handled().CaptureMouse(SharedThis(this)) : Reply;
		}
		virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
		{
			return HasMouseCapture() ? Move(Geometry, 100, Event.GetScreenSpacePosition()) : FReply::Unhandled();
		}
		virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override
		{
			const FReply Reply = Up(100);
			return Reply.IsEventHandled() ? FReply::Handled().ReleaseMouseCapture() : Reply;
		}

	private:
		FWeakPC PC;
		int32 JoyPointer = -1;
		double JoyDownAt = -10.0;
		FVector2D JoyOrigin = FVector2D::ZeroVector;
		FVector2D JoyVector = FVector2D::ZeroVector;
		int32 LookPointer = -1;
		FVector2D LookLast = FVector2D::ZeroVector;
		TMap<int32, uint8> PointerButtons;
		TSet<uint8> HeldButtons;
	};
}

bool BIBTouch::Enabled()
{
#if PLATFORM_IOS || PLATFORM_ANDROID
	return true;
#else
	static const bool bFlag = FParse::Param(FCommandLine::Get(), TEXT("BIBTouch"));
	return bFlag;
#endif
}

TSharedRef<SWidget> BIBTouch::Build(ABIBPlayerController* PC)
{
	TSharedRef<BIBTouchInternal::STouchLayer> Layer = SNew(BIBTouchInternal::STouchLayer, PC);
	const TWeakObjectPtr<ABIBPlayerController> Weak(PC);
	// Only while playing (menus, pages and dialogs take touches as clicks).
	Layer->SetVisibility(TAttribute<EVisibility>::CreateLambda([Weak]()
	{
		return Weak.IsValid() && Weak->GetScreen() == EBIBScreen::None ? EVisibility::Visible : EVisibility::Collapsed;
	}));
	return Layer;
}
