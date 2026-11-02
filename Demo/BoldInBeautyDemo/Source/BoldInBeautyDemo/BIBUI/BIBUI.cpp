#include "BIBUI/BIBUI.h"

#include "BIBGame/BIBAccount.h"
#include "BIBGame/BIBGameInstance.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBGame/BIBPlayerController.h"
#include "BIBRules/BIBConfig.h"
#include "BIBRules/BIBCore.h"
#include "BIBRules/BIBMetaOps.h"
#include "BIBRules/BIBPages.h"

#include "Brushes/SlateColorBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "Styling/CoreStyle.h"
#include "Templates/Function.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace BIBUIInternal
{
	using FWeakPC = TWeakObjectPtr<ABIBPlayerController>;

	const FLinearColor PanelColour(0.02f, 0.02f, 0.05f, 0.82f);
	const FLinearColor ButtonColour(0.22f, 0.22f, 0.34f, 1.f);
	const FLinearColor AccentColour(0.55f, 0.35f, 0.95f, 1.f);
	const FLinearColor TextColour(0.92f, 0.92f, 0.96f, 1.f);
	const FLinearColor DimColour(0.65f, 0.65f, 0.72f, 1.f);
	const FLinearColor GoodColour(0.45f, 1.f, 0.55f, 1.f);
	const FLinearColor WarnColour(1.f, 0.75f, 0.3f, 1.f);

	const FSlateBrush* WhiteBrush()
	{
		static FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	FSlateFontInfo MakeFont(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
	}

	FString ToF(const std::string& Value)
	{
		return FString(UTF8_TO_TCHAR(Value.c_str()));
	}

	TSharedRef<SWidget> MakeText(const FString& Value, int32 Size = 14, const FLinearColor& Colour = TextColour, bool bBold = false, bool bWrap = false)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Value))
			.Font(MakeFont(Size, bBold))
			.ColorAndOpacity(Colour)
			.AutoWrapText(bWrap);
	}

	TSharedRef<SWidget> MakeDynText(TFunction<FString()> Getter, int32 Size = 14, const FLinearColor& Colour = TextColour, bool bBold = false, bool bWrap = false)
	{
		return SNew(STextBlock)
			.Text_Lambda([Getter]() { return FText::FromString(Getter ? Getter() : FString()); })
			.Font(MakeFont(Size, bBold))
			.ColorAndOpacity(Colour)
			.AutoWrapText(bWrap);
	}

	TSharedRef<SWidget> MakeButton(const FString& Label, TFunction<void()> OnClick, const FLinearColor& Tint = ButtonColour, int32 Size = 14)
	{
		return SNew(SButton)
			.IsFocusable(false)
			.ButtonColorAndOpacity(Tint)
			.ContentPadding(FMargin(10.f, 5.f))
			.OnClicked_Lambda([OnClick]()
			{
				if (OnClick)
				{
					OnClick();
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(MakeFont(Size, true))
				.ColorAndOpacity(FLinearColor::White)
			];
	}

	// Root of the Results / Pause / Sign-in screens. These used to take the mouse only (their buttons are not focusable, so a
	// click does not steal the game's keyboard focus): now Up / Down (d-pad, stick) move the highlighted button, Enter / Space /
	// A press it, Esc / B / Start cancel, and the screen takes keyboard focus as soon as it is on screen.
	class SBIBKeyScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBIBKeyScreen) {}
			SLATE_DEFAULT_SLOT(FArguments, Content)
			SLATE_ARGUMENT(TFunction<bool()>, OnCancel)                       // returns true when it handled the key
			SLATE_ARGUMENT(TArray<TFunction<void()>>, Actions)                // the buttons, top to bottom
			SLATE_ARGUMENT(TSharedPtr<int32>, Selected)                       // index into Actions, shared with the button tints
			SLATE_ARGUMENT(TFunction<TSharedPtr<SWidget>()>, FocusPick)       // sign-in: the text box that should have focus now
			SLATE_ARGUMENT(bool, bCancelFirst)                                // cancel before a focused text box can swallow it
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			OnCancel = Args._OnCancel;
			Actions = Args._Actions;
			Selected = Args._Selected;
			FocusPick = Args._FocusPick;
			bCancelFirst = Args._bCancelFirst;
			ChildSlot[Args._Content.Widget];
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SBIBKeyScreen::PumpFocus));
		}

		virtual bool SupportsKeyboardFocus() const override
		{
			return true;
		}

		// An active timer rather than Tick: it keeps running while the game is paused and needs no opt-in to ticking.
		EActiveTimerReturnType PumpFocus(double, float)
		{
			if (!FSlateApplication::IsInitialized())
			{
				return EActiveTimerReturnType::Continue;
			}
			if (FocusPick)
			{
				// Focus the field the player types in next (e-mail, then the code) instead of making them click it.
				const TSharedPtr<SWidget> Want = FocusPick();
				if (Want != Current.Pin())
				{
					Current = Want;
					Tries = 30;
				}
				if (Want.IsValid() && Tries > 0 && !Want->HasFocusedDescendants() && !Want->HasAnyUserFocus().IsSet())
				{
					--Tries;
					if (Want->GetVisibility().IsVisible())
					{
						FSlateApplication::Get().SetAllUserFocus(Want.ToSharedRef(), EFocusCause::SetDirectly);
					}
				}
			}
			else if (!HasFocusedDescendants() && !HasAnyUserFocus().IsSet())
			{
				FSlateApplication::Get().SetAllUserFocus(SharedThis(this), EFocusCause::SetDirectly);
			}
			return EActiveTimerReturnType::Continue;
		}

		static bool IsCancel(const FKey& Key)
		{
			return Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right;
		}

		virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
		{
			if (bCancelFirst && IsCancel(Event.GetKey()) && !Event.IsRepeat() && OnCancel && OnCancel())
			{
				return FReply::Handled();
			}
			return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
		}

		virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
		{
			const FKey Key = Event.GetKey();
			if (!bCancelFirst && IsCancel(Key) && !Event.IsRepeat() && OnCancel && OnCancel())
			{
				return FReply::Handled();
			}
			if (Actions.Num() == 0 || !Selected.IsValid())
			{
				return FReply::Unhandled();
			}
			if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
			{
				*Selected = (*Selected + Actions.Num() - 1) % Actions.Num();
				return FReply::Handled();
			}
			if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
			{
				*Selected = (*Selected + 1) % Actions.Num();
				return FReply::Handled();
			}
			if ((Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom) && !Event.IsRepeat())
			{
				const int32 Index = FMath::Clamp(*Selected, 0, Actions.Num() - 1);
				if (Actions[Index])
				{
					Actions[Index]();
				}
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

	private:
		TFunction<bool()> OnCancel;
		TArray<TFunction<void()>> Actions;
		TSharedPtr<int32> Selected;
		TFunction<TSharedPtr<SWidget>()> FocusPick;
		TWeakPtr<SWidget> Current;
		int32 Tries = 0;
		bool bCancelFirst = false;
	};

	// MakeButton, with the keyboard / gamepad highlight: the button at *Selected is drawn lighter.
	TSharedRef<SWidget> MakeKeyButton(const FString& Label, TFunction<void()> OnClick, const FLinearColor& Tint, int32 Size,
		const TSharedPtr<int32>& Selected, int32 Index)
	{
		return SNew(SButton)
			.IsFocusable(false)
			.ButtonColorAndOpacity_Lambda([Selected, Index, Tint]()
			{
				return FSlateColor(Selected.IsValid() && *Selected == Index ? FLinearColor::LerpUsingHSV(Tint, FLinearColor::White, 0.35f) : Tint);
			})
			.ContentPadding(FMargin(10.f, 5.f))
			.OnClicked_Lambda([OnClick, Selected, Index]()
			{
				if (Selected.IsValid())
				{
					*Selected = Index;
				}
				if (OnClick)
				{
					OnClick();
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(MakeFont(Size, true))
				.ColorAndOpacity(FLinearColor::White)
			];
	}

	TSharedRef<SWidget> MakePanel(const TSharedRef<SWidget>& Content, const FLinearColor& Background = PanelColour, float Padding = 12.f)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(Background)
			.Padding(Padding)
			[
				Content
			];
	}

	TSharedRef<SWidget> MakeBar(TFunction<float()> Getter, const FLinearColor& Fill, float Height = 14.f)
	{
		return SNew(SBox)
			.HeightOverride(Height)
			[
				SNew(SProgressBar)
				.Percent_Lambda([Getter]() -> TOptional<float> { return Getter ? Getter() : 0.f; })
				.FillColorAndOpacity(Fill)
			];
	}

	ABIBGameMode* GetMode(const FWeakPC& PC)
	{
		return PC.IsValid() ? PC->GetBIBGameMode() : nullptr;
	}

	BIB::FProfile* GetProfile(const FWeakPC& PC)
	{
		ABIBGameMode* GameMode = GetMode(PC);
		return GameMode ? &GameMode->GetProfile() : nullptr;
	}

	UBIBGameInstance* GetInstance(const FWeakPC& PC)
	{
		return PC.IsValid() ? Cast<UBIBGameInstance>(PC->GetGameInstance()) : nullptr;
	}

	const FBIBHud* GetHud(const FWeakPC& PC)
	{
		const ABIBPlayerCharacter* PlayerChar = PC.IsValid() ? PC->GetBIBCharacter() : nullptr;
		return PlayerChar ? &PlayerChar->GetHud() : nullptr;
	}

	void Report(const FWeakPC& PC, const FString& Text)
	{
		if (PC.IsValid())
		{
			PC->SetStatus(Text);
			PC->MarkMenuDirty();
		}
	}

	void Report(const FWeakPC& PC, const BIB::FActionResult& Result)
	{
		Report(PC, ToF(Result.Message));
	}

	FString CurrencyLine(const FWeakPC& PC)
	{
		const BIB::FProfile* Profile = GetProfile(PC);
		if (!Profile)
		{
			return FString();
		}
		return FString::Printf(TEXT("Gold %d   Diamonds %d   Bytes %d   Angel hearts %d   |   Regular %d   Special %d   Daily %d   Mileage %d   |   Day %d"),
			Profile->Get(BIB::ECurrency::Gold), Profile->Get(BIB::ECurrency::Diamonds), Profile->Get(BIB::ECurrency::Bytes),
			Profile->Get(BIB::ECurrency::AngelHearts), Profile->Get(BIB::ECurrency::RegularTicket), Profile->Get(BIB::ECurrency::SpecialTicket),
			Profile->Get(BIB::ECurrency::DailyTicket), Profile->Get(BIB::ECurrency::Mileage), Profile->Day);
	}

	FString OperatorLabel(const BIB::FProfile& Profile, int32 RosterIndex)
	{
		const BIB::FRosterEntry& Entry = BIB::Roster()[static_cast<size_t>(RosterIndex)];
		return FString::Printf(TEXT("%s (%s)"), *ToF(Entry.Name), *ToF(BIB::StarsLabel(Entry.Stars)));
	}

	// Wraps a menu screen: title, currency bar, body, status line.
	TSharedRef<SWidget> MakeScreenFrame(const FWeakPC& PC, const FString& Title, const TSharedRef<SWidget>& Body, bool bBackButton = true)
	{
		TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
		Column->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[MakeText(Title, 26, AccentColour, true)];
		Column->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[MakeDynText([PC]() { return CurrencyLine(PC); }, 13, WarnColour)];
		Column->AddSlot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[Body]];
		Column->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[MakeDynText([PC]() { return PC.IsValid() ? PC->GetStatus() : FString(); }, 14, GoodColour, false, true)];
		if (bBackButton)
		{
			Column->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 8.f, 0.f, 0.f)
			[
				MakeButton(TEXT("< Back to the Ark"), [PC]()
				{
					if (PC.IsValid())
					{
						PC->SetStatus(FString());
						PC->ShowHub();
					}
				})
			];
		}
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(40.f))
			[
				SNew(SBox)
				.WidthOverride(1150.f)
				.HeightOverride(760.f)
				[
					MakePanel(Column, PanelColour, 20.f)
				]
			];
	}

	// ======================================================================== HUD

	TSharedRef<SWidget> BuildAbilityBox(const FWeakPC& PC, int32 Index)
	{
		auto Entry = [PC, Index]() -> const FBIBAbilityHud*
		{
			const FBIBHud* Hud = GetHud(PC);
			return (Hud && Hud->Abilities.IsValidIndex(Index)) ? &Hud->Abilities[Index] : nullptr;
		};
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		Box->AddSlot().AutoHeight()[MakeDynText([Entry]()
		{
			const FBIBAbilityHud* Ability = Entry();
			return Ability ? FString::Printf(TEXT("[%s] %s"), *Ability->Key, *Ability->Name) : FString();
		}, 12, TextColour, true, true)];
		Box->AddSlot().AutoHeight()[MakeDynText([Entry]()
		{
			const FBIBAbilityHud* Ability = Entry();
			if (!Ability)
			{
				return FString();
			}
			const FString Cooldown = Ability->Remaining > 0.f ? FString::Printf(TEXT("%.1fs"), Ability->Remaining) : FString(TEXT("ready"));
			return Ability->Note.IsEmpty() ? Cooldown : Cooldown + TEXT(" - ") + Ability->Note;
		}, 12, WarnColour, false, true)];
		return SNew(SBox).WidthOverride(170.f)[MakePanel(Box, FLinearColor(0.08f, 0.06f, 0.14f, 0.85f), 6.f)];
	}

	TSharedRef<SWidget> BuildExchangePanel(const FWeakPC& PC)
	{
		auto Character = [PC]() { return PC.IsValid() ? PC->GetBIBCharacter() : nullptr; };
		TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
		Column->AddSlot().AutoHeight()[MakeText(TEXT("GLOBAL EXCHANGE (Narrator) - press 2 to close"), 16, AccentColour, true)];
		Column->AddSlot().AutoHeight().Padding(0.f, 4.f)[MakeDynText([PC]()
		{
			const FBIBHud* Hud = GetHud(PC);
			if (!Hud)
			{
				return FString();
			}
			FString Line;
			for (int32 Stat = 0; Stat < static_cast<int32>(EBIBStat::Count); ++Stat)
			{
				Line += FString::Printf(TEXT("%s %.2fx   "), *ABIBPlayerCharacter::StatName(Stat), Hud->ExchangeValues[Stat]);
			}
			return Line + FString::Printf(TEXT("\nFrom: %s   ->   To: %s"), *ABIBPlayerCharacter::StatName(Hud->ExchangeFrom), *ABIBPlayerCharacter::StatName(Hud->ExchangeTo));
		}, 13)];
		for (int32 Row = 0; Row < 2; ++Row)
		{
			const bool bFrom = Row == 0;
			TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
			Buttons->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[MakeText(bFrom ? TEXT("Lower:") : TEXT("Raise:"), 13)];
			for (int32 Stat = 0; Stat < static_cast<int32>(EBIBStat::Count); ++Stat)
			{
				Buttons->AddSlot().AutoWidth().Padding(2.f)
				[
					MakeButton(ABIBPlayerCharacter::StatName(Stat), [Character, bFrom, Stat]()
					{
						if (ABIBPlayerCharacter* PlayerChar = Character())
						{
							PlayerChar->ExchangeSelect(bFrom, Stat);
						}
					}, ButtonColour, 12)
				];
			}
			Column->AddSlot().AutoHeight().Padding(0.f, 2.f)[Buttons];
		}
		TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
		Actions->AddSlot().AutoWidth().Padding(2.f)[MakeButton(TEXT("Exchange 10%"), [Character]()
		{
			if (ABIBPlayerCharacter* PlayerChar = Character())
			{
				PlayerChar->ExchangeApply();
			}
		}, AccentColour)];
		Actions->AddSlot().AutoWidth().Padding(2.f)[MakeButton(TEXT("Close"), [Character]()
		{
			if (ABIBPlayerCharacter* PlayerChar = Character())
			{
				PlayerChar->CloseExchange();
			}
		})];
		Column->AddSlot().AutoHeight().Padding(0.f, 4.f)[Actions];
		return SNew(SBox)
			.Visibility_Lambda([PC]()
			{
				const FBIBHud* Hud = GetHud(PC);
				return (PC.IsValid() && PC->GetScreen() == EBIBScreen::None && Hud && Hud->bExchangeOpen) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				MakePanel(Column, FLinearColor(0.1f, 0.03f, 0.15f, 0.92f))
			];
	}

	TSharedRef<SWidget> BuildHud(const FWeakPC& PC)
	{
		// Top-left: stage + objective
		TSharedRef<SVerticalBox> StageBox = SNew(SVerticalBox);
		StageBox->AddSlot().AutoHeight()[MakeDynText([PC]() { ABIBGameMode* GameMode = GetMode(PC); return GameMode ? GameMode->GetStageTitle() : FString(); }, 18, AccentColour, true)];
		StageBox->AddSlot().AutoHeight()[MakeDynText([PC]() { ABIBGameMode* GameMode = GetMode(PC); return GameMode ? GameMode->GetObjective() : FString(); }, 15, WarnColour)];

		// Top-right: message log
		TSharedRef<SWidget> LogBox = MakeDynText([PC]()
		{
			ABIBGameMode* GameMode = GetMode(PC);
			return GameMode ? FString::Join(GameMode->GetMessages(), TEXT("\n")) : FString();
		}, 13, TextColour, false, true);

		// Bottom-left: team
		TSharedRef<SVerticalBox> TeamBox = SNew(SVerticalBox);
		TeamBox->AddSlot().AutoHeight()[MakeText(TEXT("TEAM  (Tab to switch)"), 13, DimColour, true)];
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			TeamBox->AddSlot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)[MakeDynText([PC, Slot]()
			{
				const FBIBHud* Hud = GetHud(PC);
				if (!Hud || !Hud->Team.IsValidIndex(Slot))
				{
					return FString();
				}
				const FBIBTeamHud& Member = Hud->Team[Slot];
				return FString::Printf(TEXT("%s%d. %s %s"), Member.bActive ? TEXT("> ") : TEXT("  "), Slot + 1, *Member.Name,
					Member.bAlive ? *FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Member.HPFraction * 100.f)) : TEXT("DOWN"));
			}, 14)];
			TeamBox->AddSlot().AutoHeight()[SNew(SBox).WidthOverride(220.f)[MakeBar([PC, Slot]()
			{
				const FBIBHud* Hud = GetHud(PC);
				return (Hud && Hud->Team.IsValidIndex(Slot)) ? Hud->Team[Slot].HPFraction : 0.f;
			}, FLinearColor(0.3f, 0.85f, 0.4f), 8.f)]];
		}

		// Bottom-centre: active operator
		TSharedRef<SVerticalBox> ActiveBox = SNew(SVerticalBox);
		ActiveBox->AddSlot().AutoHeight()[MakeDynText([PC]()
		{
			const FBIBHud* Hud = GetHud(PC);
			return Hud ? FString::Printf(TEXT("%s   HP %d / %d   ULT %d%%"), *Hud->ActiveName, FMath::CeilToInt(Hud->HP), FMath::CeilToInt(Hud->MaxHP), FMath::FloorToInt(Hud->Ult)) : FString();
		}, 18, TextColour, true)];
		ActiveBox->AddSlot().AutoHeight().Padding(0.f, 2.f)[MakeBar([PC]() { const FBIBHud* Hud = GetHud(PC); return Hud ? Hud->HP / Hud->MaxHP : 0.f; }, FLinearColor(0.85f, 0.2f, 0.25f))];
		ActiveBox->AddSlot().AutoHeight().Padding(0.f, 2.f)[MakeBar([PC]() { const FBIBHud* Hud = GetHud(PC); return Hud ? Hud->Ult / 100.f : 0.f; }, FLinearColor(0.95f, 0.8f, 0.2f), 8.f)];
		TSharedRef<SHorizontalBox> Abilities = SNew(SHorizontalBox);
		for (int32 Index = 0; Index < 6; ++Index)
		{
			Abilities->AddSlot().AutoWidth().Padding(2.f)[BuildAbilityBox(PC, Index)];
		}
		ActiveBox->AddSlot().AutoHeight().Padding(0.f, 4.f)[Abilities];
		ActiveBox->AddSlot().AutoHeight()[MakeDynText([PC]() { const FBIBHud* Hud = GetHud(PC); return Hud ? Hud->KitLine : FString(); }, 13, AccentColour, false, true)];
		ActiveBox->AddSlot().AutoHeight()[MakeDynText([PC]() { const FBIBHud* Hud = GetHud(PC); return Hud ? Hud->ActionLine : FString(); }, 15, WarnColour, true, true)];
		ActiveBox->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[MakeText(
			(GetProfile(PC) && !GetProfile(PC)->Meta.Settings.bShowControlsHint) ? TEXT("") : TEXT("WASD move | Shift run | Space jump | LMB attack | RMB block | F parry | Shift+F dodge | 1-3 moves | 4 special | E/R ult | Tab switch | T talk | P pause"),
			11, DimColour)];

		// Centre: dialogue
		TSharedRef<SVerticalBox> Dialogue = SNew(SVerticalBox);
		Dialogue->AddSlot().AutoHeight()[MakeDynText([PC]() { ABIBGameMode* GameMode = GetMode(PC); return GameMode ? GameMode->GetDialogueSpeaker() : FString(); }, 16, GoodColour, true)];
		Dialogue->AddSlot().AutoHeight().Padding(0.f, 6.f)[MakeDynText([PC]() { ABIBGameMode* GameMode = GetMode(PC); return GameMode ? GameMode->GetDialogueLine() : FString(); }, 16, TextColour, false, true)];
		Dialogue->AddSlot().AutoHeight()[MakeText(TEXT("[T] continue"), 13, DimColour)];

		return SNew(SOverlay)
			.Visibility_Lambda([PC]()
			{
				const FBIBHud* Hud = GetHud(PC);
				return (PC.IsValid() && PC->GetScreen() == EBIBScreen::None && Hud && Hud->bValid) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
			})
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(20.f))
			[
				SNew(SBox).Visibility(EVisibility::HitTestInvisible)[MakePanel(StageBox)]
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(20.f))
			[
				SNew(SBox).WidthOverride(460.f).Visibility(EVisibility::HitTestInvisible)[MakePanel(LogBox)]
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(20.f))
			[
				SNew(SBox).Visibility(EVisibility::HitTestInvisible)[MakePanel(TeamBox)]
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(20.f))
			[
				SNew(SBox).WidthOverride(1080.f).Visibility(EVisibility::HitTestInvisible)[MakePanel(ActiveBox)]
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(20.f, 20.f, 20.f, 260.f))
			[
				SNew(SBox)
				.WidthOverride(700.f)
				.Visibility_Lambda([PC]()
				{
					ABIBGameMode* GameMode = GetMode(PC);
					return (GameMode && GameMode->IsDialogueOpen()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				[
					MakePanel(Dialogue, FLinearColor(0.02f, 0.08f, 0.04f, 0.9f), 16.f)
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(FMargin(20.f))
			[
				BuildExchangePanel(PC)
			];
	}

	// ======================================================================== screens

	// The 2D lobby (the hub with seven tabs) and its old Summon / Operators / Equipment / Team / Merchant / Demo tools screens
	// were deleted on 2026-10-07 (owner request): the Ark is the hub (BIBArkUI.h). Results, Pause and Sign in remain.

	TSharedRef<SWidget> BuildResults(const FWeakPC& PC)
	{
		TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
		if (!PC.IsValid())
		{
			return Column;
		}
		for (const FString& Line : PC->ResultLines)
		{
			Column->AddSlot().AutoHeight().Padding(0.f, 2.f)[MakeText(Line, 16, TextColour, false, true)];
		}
		const TFunction<void()> Continue = [PC]()
		{
			if (PC.IsValid())
			{
				PC->ContinueFromResults();
			}
		};
		const TSharedPtr<int32> Selected = MakeShared<int32>(0);
		Column->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 14.f, 0.f, 0.f)[MakeKeyButton(TEXT("Continue"), Continue, AccentColour, 16, Selected, 0)];
		// Enter / Space / A and Esc / B / Start all continue: there is only one way out of this screen.
		return SNew(SBIBKeyScreen)
			.Actions({Continue})
			.Selected(Selected)
			.OnCancel([Continue]() { Continue(); return true; })
			[
				MakeScreenFrame(PC, PC->bResultSuccess ? TEXT("STAGE CLEARED") : TEXT("STAGE FAILED"), Column, false)
			];
	}

	TSharedRef<SWidget> BuildPause(const FWeakPC& PC)
	{
		const TFunction<void()> Resume = [PC]()
		{
			if (PC.IsValid())
			{
				PC->SetScreen(EBIBScreen::None);
			}
		};
		const TFunction<void()> Abandon = [PC]()
		{
			if (PC.IsValid())
			{
				PC->SetScreen(EBIBScreen::None); // unpause first
			}
			if (ABIBGameMode* GameMode = GetMode(PC))
			{
				GameMode->AbandonStage();
			}
		};
		const TSharedPtr<int32> Selected = MakeShared<int32>(0);   // Resume is highlighted: Enter alone never abandons
		TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
		Column->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(2.f)[MakeKeyButton(TEXT("Resume"), Resume, AccentColour, 16, Selected, 0)];
		Column->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(2.f)[MakeKeyButton(TEXT("Abandon stage"), Abandon, FLinearColor(0.4f, 0.12f, 0.15f), 16, Selected, 1)];
		return SNew(SBIBKeyScreen)
			.Actions({Resume, Abandon})
			.Selected(Selected)
			.OnCancel([Resume]() { Resume(); return true; })
			[
				MakeScreenFrame(PC, TEXT("PAUSED"), Column, false)
			];
	}
	// ---- the sign-in screen: log in / create an account with an e-mailed code, or play offline as a guest
	FString TextOf(const TWeakPtr<SEditableTextBox>& Box)
	{
		const TSharedPtr<SEditableTextBox> Pinned = Box.Pin();
		return Pinned.IsValid() ? Pinned->GetText().ToString().TrimStartAndEnd() : FString();
	}

	TSharedRef<SWidget> BuildSignIn(const FWeakPC& PC)
	{
		UBIBGameInstance* Instance = GetInstance(PC);
		UBIBAccountSubsystem* Account = Instance ? Instance->GetSubsystem<UBIBAccountSubsystem>() : nullptr;
		const TWeakObjectPtr<UBIBAccountSubsystem> WeakAccount(Account);

		TSharedPtr<SEditableTextBox> UsernameBox;
		TSharedPtr<SEditableTextBox> EmailBox;
		TSharedPtr<SEditableTextBox> CodeBox;
		// Enter in a text box does what the button next to it does. The boxes are created before the actions that read them.
		const TSharedRef<TFunction<void()>> SendHolder = MakeShared<TFunction<void()>>();
		const TSharedRef<TFunction<void()>> VerifyHolder = MakeShared<TFunction<void()>>();
		auto StageIs = [WeakAccount](EBIBAuthStage Wanted) -> EVisibility
		{
			return WeakAccount.IsValid() && WeakAccount->GetStage() == Wanted ? EVisibility::Visible : EVisibility::Collapsed;
		};

		TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
		Column->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 4.f)[MakeText(TEXT("BOLDINBEAUTY"), 40, AccentColour, true)];
		Column->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 14.f)
		[MakeText(TEXT("Sign in to play. Your progress is saved to your account."), 14, DimColour)];

		// -- step 1: who are you
		TSharedRef<SVerticalBox> Start = SNew(SVerticalBox);
		TSharedRef<SHorizontalBox> Modes = SNew(SHorizontalBox);
		Modes->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)[SNew(SButton)
			.IsFocusable(false)
			.HAlign(HAlign_Center)
			.ButtonColorAndOpacity_Lambda([WeakAccount]() { return FSlateColor(WeakAccount.IsValid() && WeakAccount->GetMode() == EBIBAuthMode::LogIn ? AccentColour : ButtonColour); })
			.OnClicked_Lambda([WeakAccount]() { if (WeakAccount.IsValid()) { WeakAccount->SetMode(EBIBAuthMode::LogIn); } return FReply::Handled(); })
			[MakeText(TEXT("Log in"), 16, FLinearColor::White, true)]];
		Modes->AddSlot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)[SNew(SButton)
			.IsFocusable(false)
			.HAlign(HAlign_Center)
			.ButtonColorAndOpacity_Lambda([WeakAccount]() { return FSlateColor(WeakAccount.IsValid() && WeakAccount->GetMode() == EBIBAuthMode::SignUp ? AccentColour : ButtonColour); })
			.OnClicked_Lambda([WeakAccount]() { if (WeakAccount.IsValid()) { WeakAccount->SetMode(EBIBAuthMode::SignUp); } return FReply::Handled(); })
			[MakeText(TEXT("Create account"), 16, FLinearColor::White, true)]];
		Start->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Modes];

		TSharedRef<SVerticalBox> UsernameRow = SNew(SVerticalBox)
			.Visibility_Lambda([WeakAccount]() { return WeakAccount.IsValid() && WeakAccount->GetMode() == EBIBAuthMode::SignUp ? EVisibility::Visible : EVisibility::Collapsed; });
		UsernameRow->AddSlot().AutoHeight()[MakeText(TEXT("Username (3-20 letters, digits or _)"), 13, DimColour)];
		UsernameRow->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 8.f)[SAssignNew(UsernameBox, SEditableTextBox).Font(MakeFont(16)).HintText(FText::FromString(TEXT("Username")))];
		Start->AddSlot().AutoHeight()[UsernameRow];

		Start->AddSlot().AutoHeight()[MakeText(TEXT("E-mail"), 13, DimColour)];
		Start->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 10.f)[SAssignNew(EmailBox, SEditableTextBox).Font(MakeFont(16)).HintText(FText::FromString(TEXT("you@example.com")))
			.OnTextCommitted_Lambda([SendHolder](const FText&, ETextCommit::Type Type) { if (Type == ETextCommit::OnEnter && *SendHolder) { (*SendHolder)(); } })];
		const TWeakPtr<SEditableTextBox> WeakUserBox = UsernameBox;
		const TWeakPtr<SEditableTextBox> WeakEmailBox = EmailBox;
		const TFunction<void()> SendCode = [WeakAccount, WeakUserBox, WeakEmailBox]()
		{
			UBIBAccountSubsystem* Subsystem = WeakAccount.Get();
			if (!Subsystem)
			{
				return;
			}
			if (Subsystem->GetMode() == EBIBAuthMode::SignUp)
			{
				Subsystem->RequestSignup(TextOf(WeakUserBox), TextOf(WeakEmailBox));
			}
			else
			{
				Subsystem->RequestLogin(TextOf(WeakEmailBox));
			}
		};
		*SendHolder = SendCode;
		Start->AddSlot().AutoHeight()[MakeButton(TEXT("E-mail me a code"), SendCode, AccentColour, 16)];
		Start->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)[MakeText(TEXT("No account? Pick Create account: a username and an e-mail, then the code we send you."), 12, DimColour, false, true)];
		Start->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)[MakeButton(TEXT("Play offline as a guest (saved on this device only)"), [WeakAccount]()
		{
			if (WeakAccount.IsValid())
			{
				WeakAccount->ContinueAsGuest();
			}
		}, ButtonColour, 13)];
		Column->AddSlot().AutoHeight()[SNew(SBox).Visibility_Lambda([StageIs]() { return StageIs(EBIBAuthStage::SignedOut); })[Start]];

		// -- step 2: the code
		TSharedRef<SVerticalBox> CodeStep = SNew(SVerticalBox);
		CodeStep->AddSlot().AutoHeight()[MakeText(TEXT("6-digit code from the e-mail"), 13, DimColour)];
		CodeStep->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 10.f)[SAssignNew(CodeBox, SEditableTextBox).Font(MakeFont(26)).HintText(FText::FromString(TEXT("123456")))
			.OnTextCommitted_Lambda([VerifyHolder](const FText&, ETextCommit::Type Type) { if (Type == ETextCommit::OnEnter && *VerifyHolder) { (*VerifyHolder)(); } })];
		const TWeakPtr<SEditableTextBox> WeakCodeBox = CodeBox;
		const TFunction<void()> Verify = [WeakAccount, WeakCodeBox]()
		{
			if (WeakAccount.IsValid())
			{
				WeakAccount->SubmitCode(TextOf(WeakCodeBox));
			}
		};
		*VerifyHolder = Verify;
		TSharedRef<SHorizontalBox> CodeButtons = SNew(SHorizontalBox);
		CodeButtons->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)[MakeButton(TEXT("Verify"), Verify, AccentColour, 16)];
		CodeButtons->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)[MakeButton(TEXT("Send a new code"), [WeakAccount]()
		{
			if (WeakAccount.IsValid())
			{
				WeakAccount->ResendCode(FString());
			}
		}, ButtonColour, 13)];
		CodeButtons->AddSlot().AutoWidth()[MakeButton(TEXT("Back"), [WeakAccount]()
		{
			if (WeakAccount.IsValid())
			{
				WeakAccount->BackToStart();
			}
		}, ButtonColour, 13)];
		CodeStep->AddSlot().AutoHeight()[CodeButtons];
		Column->AddSlot().AutoHeight()[SNew(SBox).Visibility_Lambda([StageIs]() { return StageIs(EBIBAuthStage::AwaitingCode); })[CodeStep]];

		// -- waiting
		Column->AddSlot().AutoHeight().HAlign(HAlign_Center)
		[SNew(SBox).Visibility_Lambda([StageIs]() { return StageIs(EBIBAuthStage::Busy); })[MakeText(TEXT("Please wait..."), 18, WarnColour, true)]];

		// -- message line (errors in red) and the server address
		Column->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text_Lambda([WeakAccount]() { return FText::FromString(WeakAccount.IsValid() ? WeakAccount->GetStatusMessage() : FString()); })
			.Font(MakeFont(14, true))
			.ColorAndOpacity_Lambda([WeakAccount]() { return FSlateColor(WeakAccount.IsValid() && WeakAccount->IsMessageError() ? FLinearColor(1.f, 0.45f, 0.4f) : GoodColour); })
			.AutoWrapText(true)
		];
		Column->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
		[MakeText(Account ? FString::Printf(TEXT("Account server: %s"), *Account->GetApiBaseUrl()) : FString(TEXT("Account service unavailable.")), 11, DimColour)];

		const TWeakPtr<SEditableTextBox> WeakCode = CodeBox;
		return SNew(SBIBKeyScreen)
			.bCancelFirst(true)
			// Esc / B on the code step goes back to the e-mail step (the Back button).
			.OnCancel([WeakAccount]()
			{
				if (WeakAccount.IsValid() && WeakAccount->GetStage() == EBIBAuthStage::AwaitingCode)
				{
					WeakAccount->BackToStart();
					return true;
				}
				return false;
			})
			// Type straight away: focus the e-mail (username when creating an account), then the code box when it appears.
			.FocusPick([WeakAccount, WeakUserBox, WeakEmailBox, WeakCode]() -> TSharedPtr<SWidget>
			{
				if (!WeakAccount.IsValid())
				{
					return nullptr;
				}
				switch (WeakAccount->GetStage())
				{
				case EBIBAuthStage::AwaitingCode: return WeakCode.Pin();
				case EBIBAuthStage::SignedOut: return WeakAccount->GetMode() == EBIBAuthMode::SignUp ? WeakUserBox.Pin() : WeakEmailBox.Pin();
				default: return nullptr;
				}
			})
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(560.f)
					[
						MakePanel(Column, PanelColour, 28.f)
					]
				]
			];
	}

}

TSharedRef<SWidget> BIBUI::BuildRoot(ABIBPlayerController* PC, TSharedPtr<SBox>& OutMenuHost, bool bIncludeHud)
{
	using namespace BIBUIInternal;
	const FWeakPC WeakPC(PC);
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			bIncludeHud ? BuildHud(WeakPC) : TSharedRef<SWidget>(SNew(SSpacer))
		]
		+ SOverlay::Slot()
		[
			SAssignNew(OutMenuHost, SBox)
		];
}

TSharedRef<SWidget> BIBUI::BuildScreen(ABIBPlayerController* PC, EBIBScreen Screen)
{
	using namespace BIBUIInternal;
	const FWeakPC WeakPC(PC);
	switch (Screen)
	{
	case EBIBScreen::Results: return BuildResults(WeakPC);
	case EBIBScreen::Pause: return BuildPause(WeakPC);
	case EBIBScreen::SignIn: return BuildSignIn(WeakPC);
	default: return SNew(SSpacer);
	}
}
