#include "BIBGame/BIBArkUI.h"

#include "BIBGame/BIBMotion.h"
#include "BIBGame/BIBArkStation.h"
#include "BIBGame/BIBBlueprintLibrary.h"
#include "BIBGame/BIBGameInstance.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBGame/BIBPlayerController.h"
#include "BIBGame/BIBTouch.h"
#include "BIBGame/BIBFonts.h"
#include "BIBRules/BIBArk.h"
#include "BIBRules/BIBPages.h"
#include "BIBRules/BIBTutorial.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleDefaults.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace BIBArkUIInternal
{
	using FWeakPC = TWeakObjectPtr<ABIBPlayerController>;

	// ------------------------------------------------------------------ look
	// BoldInBeauty style guide (the trailer, 2026-10-09 owner request "fix up the ui ... layout and feel" like Zenless / Arknights):
	// Neo Black ground, Midnight Steel panels, one Bold Rose focal point, Electric Violet depth, thin Angel Cyan system lines.
	const FLinearColor Glass(0.0035f, 0.0045f, 0.0075f, 0.86f);        // Neo Black #090B12 (linear)
	const FLinearColor GlassLight(0.0075f, 0.01f, 0.019f, 0.93f);     // Midnight Steel #151925
	const FLinearColor Ink(0.93f, 0.9f, 0.95f, 1.f);                   // Angel White #F7F4FA
	const FLinearColor Dim(0.55f, 0.56f, 0.66f, 1.f);
	const FLinearColor Gold(1.f, 0.78f, 0.3f, 1.f);
	const FLinearColor Danger(1.f, 0.24f, 0.55f, 1.f);                 // Bold Rose #FF3D8D
	const FLinearColor Good(0.45f, 1.f, 0.7f, 1.f);
	const FLinearColor Cyan(0.39f, 0.91f, 1.f, 1.f);                   // Angel Cyan #64E8FF
	const FLinearColor Rose(1.f, 0.24f, 0.55f, 1.f);
	const FLinearColor Violet(0.46f, 0.34f, 1.f, 1.f);                 // Electric Violet #7657FF

	// Brand fonts from Content/BIBFonts (OFL): League Gothic = the voice (headings, numbers on bars), JetBrains Mono = HUD
	// labels, Archivo Black = big numerals. Missing files fall back to the engine font.
	FSlateFontInfo Display(int32 Size) { return BIBFonts::Display(Size); }
	FSlateFontInfo Mono(int32 Size) { return BIBFonts::Mono(Size); }
	FSlateFontInfo Numeral(int32 Size) { return BIBFonts::Numeral(Size); }

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		// Headings in the condensed display face (a size up so they read at the same height); body text stays the clean sans.
		return bBold ? Display(Size + Size / 3 + 1) : FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
	}

	// The trailer's universal cutting angle: slabs and wipes lean -12 degrees.
	FSlateRenderTransform Lean(float Degrees = -12.f)
	{
		return FSlateRenderTransform(FShear2D::FromShearAngles(FVector2D(Degrees, 0.f)));
	}

	FText T(const FString& Text)
	{
		return FText::FromString(Text);
	}

	FString F(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	double Now()
	{
		return FPlatformTime::Seconds();
	}

	FLinearColor WithAlpha(FLinearColor Colour, float Alpha)
	{
		Colour.A = Alpha;
		return Colour;
	}

	// Rounded brushes are white and tinted by the border's colour.
	const FSlateBrush* Round(float Radius)
	{
		static TMap<int32, TSharedPtr<FSlateRoundedBoxBrush>> Brushes;
		const int32 Key = FMath::RoundToInt(Radius);
		if (const TSharedPtr<FSlateRoundedBoxBrush>* Found = Brushes.Find(Key))
		{
			return Found->Get();
		}
		TSharedPtr<FSlateRoundedBoxBrush> Brush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, Radius);
		Brushes.Add(Key, Brush);
		return Brush.Get();
	}

	const FSlateBrush* Outline(float Radius, float Width)
	{
		static TMap<int32, TSharedPtr<FSlateRoundedBoxBrush>> Brushes;
		const int32 Key = FMath::RoundToInt(Radius) * 100 + FMath::RoundToInt(Width * 10.f);
		if (const TSharedPtr<FSlateRoundedBoxBrush>* Found = Brushes.Find(Key))
		{
			return Found->Get();
		}
		TSharedPtr<FSlateRoundedBoxBrush> Brush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::Transparent, Radius, FLinearColor::White, Width);
		Brushes.Add(Key, Brush);
		return Brush.Get();
	}

	const FSlateBrush* White()
	{
		static FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	// Icons and portraits: the same textures as the 2D hub (Art/Icons, Art/Portraits imported as T_<name>).
	const FSlateBrush* Icon(const FString& IconId, float Size = 40.f)
	{
		static TMap<FString, TSharedPtr<FSlateBrush>> Cache;
		static TArray<TStrongObjectPtr<UTexture2D>> Keep;
		if (IconId.IsEmpty())
		{
			return nullptr;
		}
		const FString Key = IconId + FString::Printf(TEXT("@%d"), FMath::RoundToInt(Size));
		if (const TSharedPtr<FSlateBrush>* Found = Cache.Find(Key))
		{
			return Found->Get();
		}
		FString Path;
		if (IconId.StartsWith(TEXT("portrait:")))
		{
			const FString Name = IconId.RightChop(9);
			Path = FString::Printf(TEXT("/Game/BIB/UI/Portraits/T_%s_head.T_%s_head"), *Name, *Name);
		}
		else if (IconId.StartsWith(TEXT("full:")))
		{
			const FString Name = IconId.RightChop(5);
			Path = FString::Printf(TEXT("/Game/BIB/UI/Portraits/T_%s.T_%s"), *Name, *Name);
		}
		else
		{
			Path = FString::Printf(TEXT("/Game/BIB/UI/Icons/T_%s.T_%s"), *IconId, *IconId);
		}
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		TSharedPtr<FSlateBrush> Brush;
		if (Texture)
		{
			Keep.Emplace(Texture);
			Brush = MakeShared<FSlateBrush>();
			Brush->SetResourceObject(Texture);
			const float Aspect = Texture->GetSizeY() > 0 ? static_cast<float>(Texture->GetSizeX()) / Texture->GetSizeY() : 1.f;
			Brush->ImageSize = IconId.StartsWith(TEXT("full:")) ? FVector2D(Size * Aspect, Size) : FVector2D(Size, Size);
		}
		Cache.Add(Key, Brush);
		return Brush.Get();
	}

	UBIBGameInstance* Instance(const FWeakPC& PC)
	{
		return PC.IsValid() ? Cast<UBIBGameInstance>(PC->GetGameInstance()) : nullptr;
	}

	ABIBGameMode* Mode(const FWeakPC& PC)
	{
		return PC.IsValid() ? PC->GetBIBGameMode() : nullptr;
	}

	// ------------------------------------------------------------------ widgets
	TSharedRef<STextBlock> Text(const FString& Value, int32 Size, const FLinearColor& Colour = Ink, bool bBold = false, bool bWrap = false)
	{
		return SNew(STextBlock).Text(T(Value)).Font(Font(Size, bBold)).ColorAndOpacity(FSlateColor(Colour)).AutoWrapText(bWrap)
			.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	}

	TSharedRef<STextBlock> DynText(TFunction<FString()> Getter, int32 Size, const FLinearColor& Colour = Ink, bool bBold = false, bool bWrap = false)
	{
		return SNew(STextBlock).Text_Lambda([Getter]() { return T(Getter()); }).Font(Font(Size, bBold)).ColorAndOpacity(FSlateColor(Colour)).AutoWrapText(bWrap)
			.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	}

	TSharedRef<SWidget> IconImage(const FString& IconId, float Size, const FLinearColor& Tint = FLinearColor::White)
	{
		const FSlateBrush* Brush = Icon(IconId, Size);
		if (!Brush)
		{
			return SNew(SBox).WidthOverride(Size).HeightOverride(Size);
		}
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[SNew(SImage).Image(Brush).ColorAndOpacity(FSlateColor(Tint))];
	}

	// Fades (and optionally slides) its content; RenderOpacity is not an attribute, the border's colour is.
	TSharedRef<SWidget> Fade(TFunction<float()> Alpha, const TSharedRef<SWidget>& Content, TFunction<float()> SlideX = nullptr)
	{
		return SNew(SBorder).BorderImage(FStyleDefaults::GetNoBrush()).Padding(0.f)
			.ColorAndOpacity_Lambda([Alpha]() { return FLinearColor(1.f, 1.f, 1.f, Alpha()); })
			.RenderTransform_Lambda([SlideX]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D(SlideX ? SlideX() : 0.f, 0.f))); })
			[Content];
	}

	// A horizontal or vertical colour gradient (header strips, glows).
	class SBIBGradient : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBIBGradient) : _From(FLinearColor::White), _To(FLinearColor::Transparent), _bHorizontal(true) {}
			SLATE_ARGUMENT(FLinearColor, From)
			SLATE_ARGUMENT(FLinearColor, To)
			SLATE_ARGUMENT(bool, bHorizontal)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			From = Args._From;
			To = Args._To;
			bHorizontal = Args._bHorizontal;
		}

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
		{
			const FVector2D Size = Geometry.GetLocalSize();
			TArray<FSlateGradientStop> Stops;
			Stops.Add(FSlateGradientStop(FVector2D::ZeroVector, From));
			Stops.Add(FSlateGradientStop(bHorizontal ? FVector2D(Size.X, 0.f) : FVector2D(0.f, Size.Y), To));
			FSlateDrawElement::MakeGradient(Out, Layer, Geometry.ToPaintGeometry(), Stops, bHorizontal ? Orient_Vertical : Orient_Horizontal);
			return Layer;
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D(8.f, 8.f);
		}

	private:
		FLinearColor From;
		FLinearColor To;
		bool bHorizontal = true;
	};

	// Flow-chart connector between two menu columns: a glowing curve from the picked card to the next column's spine.
	class SBIBLink : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBIBLink) : _FromY(0.f), _ToY(0.f), _Colour(FLinearColor::White), _Born(0.0) {}
			SLATE_ARGUMENT(float, FromY)
			SLATE_ARGUMENT(float, ToY)
			SLATE_ARGUMENT(FLinearColor, Colour)
			SLATE_ARGUMENT(double, Born)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			FromY = Args._FromY;
			ToY = Args._ToY;
			Colour = Args._Colour;
			Born = Args._Born;
		}

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
		{
			const float Width = static_cast<float>(Geometry.GetLocalSize().X);
			const float Grow = FMath::Clamp(static_cast<float>((Now() - Born) / 0.25), 0.f, 1.f);
			const FVector2D Start(0.f, FromY);
			const FVector2D End(Width * Grow, FMath::Lerp(FromY, ToY, Grow));
			const FVector2D Tangent(Width * 0.9f, 0.f);
			FSlateDrawElement::MakeSpline(Out, Layer, Geometry.ToPaintGeometry(), Start, Tangent, End, Tangent, 7.f, ESlateDrawEffect::None, WithAlpha(Colour, 0.18f));
			FSlateDrawElement::MakeSpline(Out, Layer + 1, Geometry.ToPaintGeometry(), Start, Tangent, End, Tangent, 2.5f, ESlateDrawEffect::None, WithAlpha(Colour, 0.95f));
			return Layer + 1;
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D(56.f, 10.f);
		}

	private:
		float FromY = 0.f;
		float ToY = 0.f;
		FLinearColor Colour;
		double Born = 0.0;
	};

	// A clickable card: rounded glass that lights up in its colour on hover. Content is anything.
	// While a menu / panel is being built, its live cards are listed here so the screen can focus the right one when it opens.
	struct FFocusSink
	{
		TArray<TWeakPtr<SWidget>> Items;
		TWeakPtr<SWidget> Highlighted;   // the tutorial's target wins
		int32 FirstPreferred = 0;        // index of the first card of the column / grid the player is working in

		TWeakPtr<SWidget> Pick() const
		{
			if (Highlighted.IsValid())
			{
				return Highlighted;
			}
			for (int32 Index = FirstPreferred; Index < Items.Num(); ++Index)
			{
				if (Items[Index].IsValid())
				{
					return Items[Index];
				}
			}
			return Items.Num() > 0 ? Items[0] : TWeakPtr<SWidget>();
		}
	};
	FFocusSink* GFocusSink = nullptr;

	// A disabled card with OnDisabled stays clickable (and focusable) so the click can say why nothing happened.
	TSharedRef<SWidget> Card(const TSharedRef<SWidget>& Content, const FLinearColor& Colour, TFunction<void()> OnClick, bool bEnabled = true,
		bool bSelected = false, bool bHighlight = false, float Radius = 12.f, FMargin Padding = FMargin(12.f, 8.f), TFunction<void()> OnDisabled = nullptr)
	{
		TSharedRef<TWeakPtr<SButton>> Holder = MakeShared<TWeakPtr<SButton>>();
		const double Born = Now();
		TSharedPtr<SButton> Button;
		SAssignNew(Button, SButton)
			.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
			.ContentPadding(FMargin(0.f))
			.IsEnabled(bEnabled || static_cast<bool>(OnDisabled))
			.Cursor(bEnabled ? EMouseCursor::Hand : EMouseCursor::Default)
			.OnClicked_Lambda([OnClick, OnDisabled, bEnabled]()
			{
				if (!bEnabled)
				{
					if (OnDisabled) { OnDisabled(); }
				}
				else if (OnClick)
				{
					OnClick();
				}
				return FReply::Handled();
			})
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(Round(Radius))
					.BorderBackgroundColor_Lambda([Holder, Colour, bSelected, bEnabled]()
					{
						const TSharedPtr<SButton> Self = Holder->Pin();
						// Keyboard / gamepad focus lights a card like hover does, so navigation is visible.
						const bool bHover = Self.IsValid() && (Self->IsHovered() || Self->HasAnyUserFocus().IsSet()) && bEnabled;
						const bool bDown = Self.IsValid() && Self->IsPressed() && bEnabled;
						if (bSelected)
						{
							return FSlateColor(FLinearColor::LerpUsingHSV(Glass, Colour, bHover ? 0.55f : 0.42f).CopyWithNewOpacity(0.95f));
						}
						return FSlateColor(bDown ? FLinearColor::LerpUsingHSV(GlassLight, Colour, 0.5f) : (bHover ? FLinearColor::LerpUsingHSV(GlassLight, Colour, 0.25f) : GlassLight));
					})
					.Padding(Padding)
					[
						Content
					]
				]
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(Outline(Radius, 1.5f))
					.Visibility(EVisibility::HitTestInvisible)
					.BorderBackgroundColor_Lambda([Holder, Colour, bSelected, bHighlight, bEnabled, Born]()
					{
						const TSharedPtr<SButton> Self = Holder->Pin();
						const bool bHover = Self.IsValid() && Self->IsHovered() && bEnabled;
						if (Self.IsValid() && Self->HasAnyUserFocus().IsSet())
						{
							return FSlateColor(FLinearColor::LerpUsingHSV(Colour, Ink, 0.65f));   // the focus ring
						}
						if (bHighlight)
						{
							// The tutorial's target pulses gold.
							const float Pulse = 0.55f + 0.45f * FMath::Sin(static_cast<float>(Now() - Born) * 5.f);
							return FSlateColor(WithAlpha(Gold, Pulse));
						}
						return FSlateColor(WithAlpha(Colour, bSelected ? 1.f : (bHover ? 0.85f : 0.28f)));
					})
				]
			];
		*Holder = Button;
		if (GFocusSink && bEnabled)
		{
			GFocusSink->Items.Add(Button);
			if (bHighlight && !GFocusSink->Highlighted.IsValid())
			{
				GFocusSink->Highlighted = Button;
			}
		}
		if (!bEnabled)
		{
			Button->SetRenderOpacity(0.55f);   // still readable: the second line is usually the reason
		}
		return Button.ToSharedRef();
	}

	TSharedRef<SWidget> Badge()
	{
		return SNew(SBox).WidthOverride(12.f).HeightOverride(12.f)
			[SNew(SBorder).BorderImage(Round(6.f)).BorderBackgroundColor(FSlateColor(Danger))];
	}

	// What a disabled card says when it is clicked: its own second line (cost / state) is the reason.
	TFunction<void()> DisabledReason(const FWeakPC& PC, const FString& Label, const FString& Sub)
	{
		return [PC, Label, Sub]()
		{
			if (PC.IsValid())
			{
				PC->ArkToast(Sub.IsEmpty() ? Label + TEXT(": not available right now") : Label + TEXT(": ") + Sub);
			}
		};
	}

	// Refusals ("Need ...", a disabled card's reason, a failed teleport) while a menu or panel is open: the Ark HUD that shows
	// ArkUi.Toast is hidden behind them.
	TSharedRef<SWidget> ToastStrip(const FWeakPC& PC)
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(0.f, 0.f, 0.f, 64.f))
			.Visibility_Lambda([PC]() { return PC.IsValid() && Now() - PC->ArkUi.ToastAt < 4.0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SBorder).BorderImage(Round(12.f)).BorderBackgroundColor(FSlateColor(Glass)).Padding(FMargin(16.f, 8.f))
				[DynText([PC]() { return PC.IsValid() ? PC->ArkUi.Toast : FString(); }, 15, Gold)]
			];
	}

	// A key cap: [F]
	TSharedRef<SWidget> KeyCap(const FString& Key, const FLinearColor& Colour = Ink)
	{
		return SNew(SBorder).BorderImage(Round(6.f)).BorderBackgroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.14f))).Padding(FMargin(9.f, 3.f))
			[Text(Key, 14, Colour, true)];
	}

	// Focusable root of the menu and the panel: Esc / M close, Backspace goes back.
	class SBIBArkScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBIBArkScreen) {}
			SLATE_DEFAULT_SLOT(FArguments, Content)
			SLATE_ARGUMENT(TFunction<void()>, OnClose)
			SLATE_ARGUMENT(TFunction<void()>, OnBack)
			SLATE_ARGUMENT(TFunction<void()>, OnMenu)
			SLATE_ARGUMENT(TWeakPtr<SWidget>, FocusTarget)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			OnClose = Args._OnClose;
			OnBack = Args._OnBack;
			OnMenu = Args._OnMenu;
			FocusTarget = Args._FocusTarget;
			ChildSlot[Args._Content.Widget];
		}

		virtual bool SupportsKeyboardFocus() const override
		{
			return true;
		}

		// The controller focuses this root when the screen opens; hand the focus on to the first live card so the arrow keys /
		// d-pad and Enter / A work straight away (Esc, M and Backspace still reach OnKeyDown: key events bubble up).
		virtual FReply OnFocusReceived(const FGeometry&, const FFocusEvent& Event) override
		{
			const TSharedPtr<SWidget> Target = FocusTarget.Pin();
			if (Target.IsValid() && (Event.GetCause() == EFocusCause::SetDirectly || Event.GetCause() == EFocusCause::Navigation))
			{
				return FReply::Handled().SetUserFocus(Target.ToSharedRef(), EFocusCause::Navigation);
			}
			return FReply::Unhandled();
		}

		virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& Event) override
		{
			const FKey Key = Event.GetKey();
			if ((Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right) && OnClose)
			{
				OnClose();
				return FReply::Handled();
			}
			if (Key == EKeys::M && OnMenu)
			{
				OnMenu();
				return FReply::Handled();
			}
			if ((Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right) && OnBack)
			{
				OnBack();
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

	private:
		TFunction<void()> OnClose;
		TFunction<void()> OnBack;
		TFunction<void()> OnMenu;
		TWeakPtr<SWidget> FocusTarget;
	};

	// The three-line menu button (top right).
	TSharedRef<SWidget> MenuButton(const FWeakPC& PC, bool bClose)
	{
		TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
		if (bClose)
		{
			Lines->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Text(TEXT("X"), 24, Ink, true)];
		}
		else
		{
			for (int32 Line = 0; Line < 3; ++Line)
			{
				Lines->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 3.f)
				[
					SNew(SBox).WidthOverride(Line == 1 ? 24.f : 30.f).HeightOverride(4.f)
					[SNew(SBorder).BorderImage(Round(2.f)).BorderBackgroundColor(FSlateColor(Ink))]
				];
			}
		}
		const TSharedRef<SWidget> Body = SNew(SBox).WidthOverride(64.f).HeightOverride(64.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Lines]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, -4.f, -4.f, 0.f)
			[
				SNew(SBox).Visibility_Lambda([PC, bClose]()
				{
					// Something to claim (or a tutorial step) anywhere in the menu puts a dot on the button. Checked once a second.
					static double CheckedAt = -100.0;
					static bool bDot = false;
					UBIBGameInstance* Owner = Instance(PC);
					if (bClose || !Owner)
					{
						return EVisibility::Collapsed;
					}
					if (Now() - CheckedAt > 1.0)
					{
						CheckedAt = Now();
						bDot = false;
						for (const BIB::Ark::FNode& Node : BIB::Ark::Children(Owner->GetProfile(), Owner->UiState, BIB::Ark::RootId))
						{
							bDot = bDot || Node.bBadge || Node.bHighlight;
						}
					}
					return bDot ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				[Badge()]
			]
		];
		return Card(Body, Cyan, [PC, bClose]()
		{
			if (PC.IsValid())
			{
				if (bClose)
				{
					PC->CloseArkScreens();
				}
				else
				{
					PC->OpenArkMenu();
				}
			}
		}, true, false, false, 14.f, FMargin(0.f));
	}

	// ======================================================================= Ark HUD
	bool ArkHudShown(const FWeakPC& PC)
	{
		const ABIBGameMode* GameMode = Mode(PC);
		return PC.IsValid() && GameMode && GameMode->IsInArk() && PC->GetScreen() == EBIBScreen::None;
	}

	TSharedRef<SWidget> BuildArkHud(const FWeakPC& PC)
	{
		auto Station = [PC]() -> ABIBArkStation*
		{
			const ABIBGameMode* GameMode = Mode(PC);
			return GameMode ? GameMode->GetStationInReach() : nullptr;
		};
		auto Shown = [PC]() { return ArkHudShown(PC) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; };
		return SNew(SOverlay)
			.Visibility_Lambda(Shown)
			// Top right: the only permanent thing on screen.
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 26.f, 30.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[MenuButton(PC, false)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(SBox).Visibility_Lambda([PC]()
					{
						// While Alt is held (cursor visible) there is no need for the hint.
						return PC.IsValid() && PC->bShowMouseCursor ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
					})
					[KeyCap(TEXT("M"), Dim)]
				]
			]
			// Top centre: the tutorial's instruction while it runs.
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 28.f, 0.f, 0.f)
			[
				SNew(SBox).MaxDesiredWidth(980.f).Visibility_Lambda([PC]()
				{
					UBIBGameInstance* Owner = Instance(PC);
					return Owner && BIB::Tutorial::Active(Owner->GetProfile()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				[
					SNew(SBorder).BorderImage(Round(14.f)).BorderBackgroundColor(FSlateColor(Glass)).Padding(FMargin(18.f, 10.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
						[SNew(SBox).WidthOverride(6.f).HeightOverride(36.f)[SNew(SBorder).BorderImage(Round(3.f)).BorderBackgroundColor(FSlateColor(Gold))]]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							DynText([PC]()
							{
								UBIBGameInstance* Owner = Instance(PC);
								return Owner ? F(BIB::Ark::TutorialLine(Owner->GetProfile())) : FString();
							}, 15, Ink, false, true)
						]
					]
				]
			]
			// Arrival banner after a teleport.
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 150.f, 0.f, 0.f)
			[
				SNew(SBox)
				.Visibility_Lambda([PC]() { return PC.IsValid() && Now() - PC->ArkUi.ArrivalAt < 3.0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					Fade([PC]()
					{
						const double Age = PC.IsValid() ? Now() - PC->ArkUi.ArrivalAt : 9.0;
						return static_cast<float>(FMath::Clamp(FMath::Min(Age / 0.25, (3.0 - Age) / 0.6), 0.0, 1.0));
					},
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(Font(46, true)).ShadowOffset(FVector2D(2.f, 2.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
						.Text_Lambda([PC]() { return T(PC.IsValid() ? PC->ArkUi.ArrivalName.ToUpper() : FString()); })
						.ColorAndOpacity_Lambda([PC]() { return FSlateColor(PC.IsValid() ? PC->ArkUi.ArrivalColour : Ink); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f)
					[SNew(SBox).WidthOverride(420.f).HeightOverride(3.f)[SNew(SBIBGradient).From(WithAlpha(Cyan, 0.f)).To(WithAlpha(Cyan, 0.9f))]]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[DynText([PC]() { return PC.IsValid() ? PC->ArkUi.ArrivalSub : FString(); }, 16, Dim)])
				]
			]
			// Bottom centre: the station prompt inside a ring.
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 120.f)
			[
				SNew(SBox).Visibility_Lambda([Station]() { return Station() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SBorder).BorderImage(Round(18.f)).BorderBackgroundColor(FSlateColor(Glass)).Padding(FMargin(16.f, 10.f, 22.f, 10.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[KeyCap(TEXT("F"), Gold)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
						[
							SNew(SBox).WidthOverride(6.f).HeightOverride(30.f)
							[
								SNew(SBorder).BorderImage(Round(3.f)).BorderBackgroundColor_Lambda([Station]()
								{
									const ABIBArkStation* Here = Station();
									return FSlateColor(Here ? Here->GetColour() : Ink);
								})
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[DynText([Station]()
							{
								const ABIBArkStation* Here = Station();
								return Here ? TEXT("Open ") + Here->GetDisplayName() : FString();
							}, 18, Ink, true)]
							+ SVerticalBox::Slot().AutoHeight()[DynText([Station]()
							{
								const ABIBArkStation* Here = Station();
								const BIB::Ark::FStation* Data = Here ? BIB::Ark::FindStation(TCHAR_TO_UTF8(*Here->GetStationId())) : nullptr;
								return Data ? F(Data->Blurb) : FString();
							}, 13, Dim)]
						]
					]
				]
			]
			// Toast (errors, refusals) above the prompt.
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 200.f)
			[
				SNew(SBox)
				.Visibility_Lambda([PC]() { return PC.IsValid() && Now() - PC->ArkUi.ToastAt < 4.0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SBorder).BorderImage(Round(12.f)).BorderBackgroundColor(FSlateColor(Glass)).Padding(FMargin(14.f, 8.f))
					[DynText([PC]() { return PC.IsValid() ? PC->ArkUi.Toast : FString(); }, 15, Gold)]
				]
			];
	}

	// "Entering the Ark ..." while the hub map hands over to L_Ark.
	TSharedRef<SWidget> BuildTravelNote(const FWeakPC& PC)
	{
		return SNew(SBox)
			.HAlign(HAlign_Center).VAlign(VAlign_Center)
			.Visibility_Lambda([PC]()
			{
				const ABIBGameMode* GameMode = Mode(PC);
				const UBIBGameInstance* Owner = Instance(PC);
				const bool bShown = PC.IsValid() && GameMode && Owner && PC->GetScreen() == EBIBScreen::None && !Owner->NeedsSignIn() && GameMode->ShouldUseArk()
					&& !GameMode->IsArkWorld() && GameMode->GetFlow() == EBIBFlow::Hub && !GameMode->HasPendingStart();
				return bShown ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			[
				SNew(SBorder).BorderImage(Round(16.f)).BorderBackgroundColor(FSlateColor(Glass)).Padding(FMargin(28.f, 16.f))
				[Text(TEXT("Entering the Ark ..."), 24, Cyan, true)]
			];
	}

	// ======================================================================= battle HUD
	// Read once per frame: dozens of HUD bindings ask for it.
	const FBIBHudInfo& HudInfo(const FWeakPC& PC)
	{
		static FBIBHudInfo Cached;
		static uint64 Frame = MAX_uint64;
		if (Frame != GFrameCounter)
		{
			Frame = GFrameCounter;
			Cached = PC.IsValid() ? UBIBBlueprintLibrary::GetHudInfo(PC.Get()) : FBIBHudInfo();
		}
		return Cached;
	}

	bool BattleShown(const FWeakPC& PC)
	{
		const ABIBGameMode* GameMode = Mode(PC);
		const ABIBPlayerCharacter* PlayerChar = PC.IsValid() ? PC->GetBIBCharacter() : nullptr;
		return PC.IsValid() && GameMode && GameMode->IsInStage() && PC->GetScreen() == EBIBScreen::None && PlayerChar && PlayerChar->GetHud().bValid;
	}

	TSharedRef<SWidget> Bar(TFunction<float()> Fraction, TFunction<FLinearColor()> Colour, float Width, float Height)
	{
		return SNew(SBox).WidthOverride(Width).HeightOverride(Height)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SBorder).BorderImage(Round(Height * 0.5f)).BorderBackgroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.12f)))]
			+ SOverlay::Slot().HAlign(HAlign_Left)
			[
				SNew(SBox).HeightOverride(Height).WidthOverride_Lambda([Fraction, Width]() { return FOptionalSize(Width * FMath::Clamp(Fraction(), 0.f, 1.f)); })
				[SNew(SBorder).BorderImage(Round(Height * 0.5f)).BorderBackgroundColor_Lambda([Colour]() { return FSlateColor(Colour()); })]
			]
		];
	}

	TSharedRef<SWidget> BuildBattleHud(const FWeakPC& PC)
	{
		// Messages: show the newest three for five seconds after the list last changed.
		struct FToastState
		{
			int32 Count = -1;
			FString Last;
			double ChangedAt = -100.0;
		};
		TSharedRef<FToastState> Toasts = MakeShared<FToastState>();
		auto MessagesText = [PC, Toasts]() -> FString
		{
			const FBIBHudInfo& Info = HudInfo(PC);
			const FString Last = Info.Messages.Num() > 0 ? Info.Messages.Last() : FString();
			if (Info.Messages.Num() != Toasts->Count || Last != Toasts->Last)
			{
				Toasts->Count = Info.Messages.Num();
				Toasts->Last = Last;
				Toasts->ChangedAt = Now();
			}
			if (Now() - Toasts->ChangedAt > 5.0)
			{
				return FString();
			}
			FString Out;
			for (int32 Index = FMath::Max(0, Info.Messages.Num() - 3); Index < Info.Messages.Num(); ++Index)
			{
				Out += (Out.IsEmpty() ? TEXT("") : TEXT("\n")) + Info.Messages[Index];
			}
			return Out;
		};

		// ---- Zenless-style layout (2026-10-09): squad top left, boss top centre, objective top right, combo counter right,
		// banners centre, skills bottom right, kit prompt bottom centre. Everything reads the character's FBIBHud directly.
		auto Feel = [PC]() -> const FBIBHud*
		{
			const ABIBPlayerCharacter* PlayerChar = PC.IsValid() ? PC->GetBIBCharacter() : nullptr;
			return PlayerChar ? &PlayerChar->GetHud() : nullptr;
		};

		// Skills: Move 1-3 and Special as leaning slabs, the ultimate as the big one. A slot's cooldown is drawn as a dark
		// curtain that falls away; the longest cooldown seen for the slot is its full height.
		struct FCooldownPeaks
		{
			float Peak[8] = {};
			FString Name[8];   // the ability the peak belongs to (an operator switch changes it)
		};
		TSharedRef<FCooldownPeaks> Peaks = MakeShared<FCooldownPeaks>();
		// Indexed by slot: this runs for every slab every frame, and used to build a key string and hash it each time.
		auto SkillFraction = [Peaks](int32 Slot, const FBIBAbilityInfo& Info) -> float
		{
			if (Slot < 0 || Slot >= 8)
			{
				return 0.f;
			}
			float& Peak = Peaks->Peak[Slot];
			if (Info.CooldownRemaining <= 0.f)
			{
				Peak = 0.f;
				return 0.f;
			}
			if (Peaks->Name[Slot] != Info.Name)
			{
				Peaks->Name[Slot] = Info.Name;
				Peak = 0.f;
			}
			Peak = FMath::Max(Peak, Info.CooldownRemaining);
			return Info.CooldownRemaining / FMath::Max(0.01f, Peak);
		};
		auto SkillSlab = [PC, SkillFraction](int32 Index, float Width, float Height, bool bUltimate) -> TSharedRef<SWidget>
		{
			// By reference: these lambdas run several times per slab per frame, and each copy duplicated three strings.
			auto Ability = [PC, Index]() -> const FBIBAbilityInfo&
			{
				static const FBIBAbilityInfo None;
				const FBIBHudInfo& Info = HudInfo(PC);
				return Info.Abilities.IsValidIndex(Index) ? Info.Abilities[Index] : None;
			};
			return SNew(SBox).WidthOverride(Width).HeightOverride(Height).RenderTransform(Lean()).RenderTransformPivot(FVector2D(0.5f, 0.5f))
				.Visibility_Lambda([PC, Index]() { return HudInfo(PC).Abilities.IsValidIndex(Index) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FSlateColor(GlassLight))]
					// ready glow in the operator's colour (rose for the ultimate when it is full)
					+ SOverlay::Slot().VAlign(VAlign_Bottom)
					[
						SNew(SBox).HeightOverride(4.f)
						[SNew(SBorder).BorderImage(White()).BorderBackgroundColor_Lambda([PC, Ability, bUltimate]()
						{
							const FBIBAbilityInfo& Info = Ability();
							if (Info.CooldownRemaining > 0.f)
							{
								return FSlateColor(WithAlpha(Dim, 0.25f));
							}
							const FLinearColor Base = bUltimate ? Rose : HudInfo(PC).Colour;
							return FSlateColor(bUltimate ? FLinearColor::LerpUsingHSV(Base, Ink, 0.3f + 0.3f * FMath::Sin(static_cast<float>(Now()) * 6.f)) : Base);
						})]
					]
					// cooldown curtain
					+ SOverlay::Slot().VAlign(VAlign_Top)
					[
						SNew(SBox).HeightOverride_Lambda([Ability, SkillFraction, Height, Index]() { return FOptionalSize(Height * SkillFraction(Index, Ability())); })
						[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.62f)))]
					]
					+ SOverlay::Slot().Padding(FMargin(12.f, 6.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()
							[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FSlateColor(WithAlpha(Cyan, 0.16f))).Padding(FMargin(6.f, 0.f))
								[SNew(STextBlock).Font(Mono(12)).ColorAndOpacity(FSlateColor(Cyan)).Text_Lambda([Ability]() { return T(Ability().Key.ToUpper()); })]]
							+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right)
							[SNew(STextBlock).Font(Numeral(bUltimate ? 22 : 16)).ColorAndOpacity(FSlateColor(Ink)).Text_Lambda([Ability]()
							{
								const float Cd = Ability().CooldownRemaining;
								return T(Cd <= 0.f ? FString() : (Cd >= 10.f ? FString::Printf(TEXT("%d"), FMath::CeilToInt(Cd)) : FString::Printf(TEXT("%.1f"), Cd)));
							})]
						]
						+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Bottom)
						[SNew(STextBlock).Font(Display(bUltimate ? 22 : 17)).AutoWrapText(true).Text_Lambda([Ability]()
							{
								// "Ult: Reality Destabilisation" -> "REALITY DESTABILISATION": the slab says what it is already.
								FString Name = Ability().Name;
								int32 Colon = INDEX_NONE;
								if (Name.FindChar(TEXT(':'), Colon) && Colon < 9)
								{
									Name = Name.RightChop(Colon + 1).TrimStart();
								}
								return T(Name.ToUpper());
							})
							.ColorAndOpacity_Lambda([Ability]() { return FSlateColor(Ability().CooldownRemaining > 0.f ? Dim : Ink); })]
					]
					// ready: the whole slab is outlined in the operator's colour (the 4 px strip alone was easy to miss mid-fight)
					+ SOverlay::Slot()[SNew(SBorder).BorderImage(Outline(0.f, 1.5f)).BorderBackgroundColor_Lambda([Ability, PC]()
					{
						return FSlateColor(Ability().CooldownRemaining > 0.f ? WithAlpha(Cyan, 0.22f) : WithAlpha(HudInfo(PC).Colour, 0.95f));
					})]
				];
		};

		TSharedRef<SVerticalBox> Squad = SNew(SVerticalBox);
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			auto Member = [PC, Slot]() -> const FBIBTeamMemberInfo&
			{
				static const FBIBTeamMemberInfo None;
				const FBIBHudInfo& Info = HudInfo(PC);
				return Info.Team.IsValidIndex(Slot) ? Info.Team[Slot] : None;
			};
			Squad->AddSlot().AutoHeight().Padding(0.f, 3.f)
			[
				SNew(SBox).Visibility_Lambda([Member]() { const FBIBTeamMemberInfo& M = Member(); return !M.Name.IsEmpty() && !M.bActive && M.Name != TEXT("(empty)") ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SBorder).BorderImage(White()).Padding(FMargin(6.f, 4.f, 12.f, 4.f)).RenderTransform(Lean()).RenderTransformPivot(FVector2D(0.f, 0.5f))
					.BorderBackgroundColor_Lambda([Feel]()
					{
						// A chain attack is ready: the bench lights up gold.
						const FBIBHud* Hud = Feel();
						return FSlateColor(Hud && Hud->bChainReady ? FLinearColor::LerpUsingHSV(Glass, Gold, 0.35f + 0.25f * FMath::Sin(static_cast<float>(Now()) * 12.f)) : Glass);
					})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
						[SNew(SImage).Image_Lambda([Member]() { return Icon(TEXT("portrait:") + Member().Name.ToLower(), 40.f); })
							.ColorAndOpacity_Lambda([Member]() { return FSlateColor(Member().bAlive ? FLinearColor::White : FLinearColor(0.25f, 0.25f, 0.25f)); })]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Display(18)).ColorAndOpacity(FSlateColor(Ink))
								.Text_Lambda([Member]() { const FBIBTeamMemberInfo& M = Member(); return T(M.bAlive ? M.Name.ToUpper() : M.Name.ToUpper() + TEXT("  //  DOWN")); })]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[Bar([Member]() { return Member().HPFraction; }, []() { return Rose; }, 150.f, 6.f)]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
						[SNew(STextBlock).Font(Mono(11)).Text_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return T(Hud && Hud->bChainReady ? TEXT("TAB  RELAY") : TEXT("TAB")); })
							.ColorAndOpacity_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return FSlateColor(Hud && Hud->bChainReady ? Gold : Dim); })]
					]
				]
			];
		}

		return SNew(SOverlay)
			.Visibility_Lambda([PC]() { return BattleShown(PC) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			// Top left: the active operator, then the bench.
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(36.f, 26.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
					[
						SNew(SBox).WidthOverride(92.f).HeightOverride(92.f).RenderTransform(Lean()).RenderTransformPivot(FVector2D(0.5f, 0.5f))
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SBorder).BorderImage(White()).BorderBackgroundColor_Lambda([PC]() { return FSlateColor(WithAlpha(HudInfo(PC).Colour, 0.9f)); })]
							+ SOverlay::Slot().Padding(3.f)[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FSlateColor(Glass)).Padding(0.f)
								[SNew(SImage).Image_Lambda([PC]() { return Icon(TEXT("portrait:") + HudInfo(PC).ActiveName.ToLower(), 86.f); })]]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(16.f, 0.f, 0.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Font(Display(40)).Text_Lambda([PC]() { return T(HudInfo(PC).ActiveName.ToUpper()); })
								.ColorAndOpacity(FSlateColor(Ink)).ShadowOffset(FVector2D(3.f, 2.f)).ShadowColorAndOpacity_Lambda([PC]() { return WithAlpha(HudInfo(PC).Colour, 0.85f); })]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(12.f, 0.f, 0.f, 8.f)
							[SNew(STextBlock).Font(Mono(14)).ColorAndOpacity(FSlateColor(Ink)).ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f)).Text_Lambda([PC]()
								{ const FBIBHudInfo& Info = HudInfo(PC); return T(FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Info.HP), FMath::CeilToInt(Info.MaxHP))); })]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
						[
							SNew(SBox).RenderTransform(Lean()).RenderTransformPivot(FVector2D(0.f, 0.5f))
							[Bar([PC]() { return HudInfo(PC).HPFraction; }, [PC]()
							{
								const float Fraction = HudInfo(PC).HPFraction;
								return Fraction > 0.3f ? Rose : FLinearColor::LerpUsingHSV(Rose, Ink, 0.5f + 0.5f * FMath::Sin(static_cast<float>(Now()) * 10.f));
							}, 380.f, 14.f)]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[SNew(SBox).RenderTransform(Lean()).RenderTransformPivot(FVector2D(0.f, 0.5f))
								[Bar([PC]() { return HudInfo(PC).UltFraction; }, [PC]()
								{
									const float Fraction = HudInfo(PC).UltFraction;
									return Fraction >= 1.f ? FLinearColor::LerpUsingHSV(Violet, Gold, 0.5f + 0.5f * FMath::Sin(static_cast<float>(Now()) * 6.f)) : Violet;
								}, 260.f, 6.f)]]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
							[SNew(STextBlock).Font(Mono(12)).Text_Lambda([PC]()
								{ const float Fraction = HudInfo(PC).UltFraction; return T(Fraction >= 1.f ? FString(TEXT("ULTIMATE READY")) : FString::Printf(TEXT("ULT %d%%"), FMath::FloorToInt(Fraction * 100.f))); })
								.ColorAndOpacity_Lambda([PC]() { return FSlateColor(HudInfo(PC).UltFraction >= 1.f ? Gold : Violet); })]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 12.f, 0.f, 0.f)[Squad]
			]
			// Top centre: the boss (name, health, Daze) and what the angels have learned of you.
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 24.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).Visibility_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return Hud && !Hud->BossName.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Font(Display(30)).ColorAndOpacity(FSlateColor(Ink))
								.Text_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return T(Hud ? Hud->BossName.ToUpper() : FString()); })]
							+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 5.f)
							[SNew(STextBlock).Font(Mono(12)).Text_Lambda([Feel]()
								{ const FBIBHud* Hud = Feel(); return T(!Hud ? FString() : (Hud->bBossStunned ? FString(TEXT("BROKEN  //  x1.6 DAMAGE")) : FString::Printf(TEXT("STRAIN %d%%"), FMath::FloorToInt(Hud->BossDaze * 100.f)))); })
								.ColorAndOpacity_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return FSlateColor(Hud && Hud->bBossStunned ? Gold : Cyan); })]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
						[SNew(SBox).RenderTransform(Lean()).RenderTransformPivot(FVector2D(0.5f, 0.5f))
							[Bar([Feel]() { const FBIBHud* Hud = Feel(); return Hud ? Hud->BossHP : 0.f; }, []() { return Rose; }, 820.f, 16.f)]]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
						[SNew(SBox).RenderTransform(Lean()).RenderTransformPivot(FVector2D(0.5f, 0.5f))
							[Bar([Feel]() { const FBIBHud* Hud = Feel(); return Hud ? Hud->BossDaze : 0.f; }, [Feel]()
							{
								const FBIBHud* Hud = Feel();
								return Hud && Hud->bBossStunned ? FLinearColor::LerpUsingHSV(Gold, Ink, 0.5f + 0.5f * FMath::Sin(static_cast<float>(Now()) * 12.f)) : Cyan;
							}, 820.f, 5.f)]]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(SBox).Visibility_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return Hud && !Hud->AngelLine.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Font(Mono(11)).ColorAndOpacity(FSlateColor(Rose)).Text(T(TEXT("ANGEL MIND ")))]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
						[Bar([Feel]() { const FBIBHud* Hud = Feel(); return Hud ? Hud->AngelAdaptation : 0.f; }, []() { return Rose; }, 90.f, 4.f)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
						[SNew(STextBlock).Font(Mono(11)).ColorAndOpacity(FSlateColor(Dim)).Text_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return T(Hud ? Hud->AngelLine.ToUpper() : FString()); })]
					]
				]
			]
			// Top right: stage and objective, then short toasts.
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 92.f, 36.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[SNew(STextBlock).Font(Mono(12)).ColorAndOpacity(FSlateColor(Cyan))
					.Text_Lambda([PC]() { return T(HudInfo(PC).StageTitle.ToUpper()); })]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 2.f, 0.f, 0.f)
				[SNew(SBox).MaxDesiredWidth(440.f)[SNew(STextBlock).Font(Display(24)).ColorAndOpacity(FSlateColor(Ink)).AutoWrapText(true).Justification(ETextJustify::Right)
					.Text_Lambda([PC]() { return T(HudInfo(PC).Objective); })]]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 2.f, 0.f, 0.f)
				[SNew(SBox).WidthOverride(140.f).HeightOverride(2.f)[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FSlateColor(Rose))]]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 14.f, 0.f, 0.f)
				[
					SNew(SBox).MaxDesiredWidth(420.f).Visibility_Lambda([MessagesText]() { return MessagesText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
					[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FSlateColor(WithAlpha(Glass, 0.7f))).Padding(FMargin(12.f, 8.f))
						[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13)).ColorAndOpacity(FSlateColor(Ink)).AutoWrapText(true)
							.Text_Lambda([MessagesText]() { return T(MessagesText()); })]]
				]
			]
			// Right: the combo counter.
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.f, 0.f, 60.f, 120.f)
			[
				SNew(SBox).RenderTransform(FSlateRenderTransform(FShear2D::FromShearAngles(FVector2D(-8.f, 0.f)))).RenderTransformPivot(FVector2D(0.5f, 0.5f))
				.Visibility_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return Hud && Hud->ComboHits >= 2 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[SNew(STextBlock).Font_Lambda([Feel]()
						{
							// Each new hit punches the number in big, then it settles.
							const FBIBHud* Hud = Feel();
							const float Age = Hud ? Hud->ComboAge : 1.f;
							return Numeral(56 + FMath::RoundToInt(18.f * FMath::Clamp(1.f - Age / 0.12f, 0.f, 1.f)));
						})
						.ColorAndOpacity_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return FSlateColor(WithAlpha(Rose, Hud ? FMath::Clamp(1.f - (Hud->ComboAge - 1.8f) / 0.7f, 0.f, 1.f) : 0.f)); })
						.ShadowOffset(FVector2D(5.f, 4.f)).ShadowColorAndOpacity(WithAlpha(Violet, 0.9f))
						.Text_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return T(Hud ? FString::FromInt(Hud->ComboHits) : FString()); })]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[SNew(STextBlock).Font(Display(24)).ColorAndOpacity(FSlateColor(Ink)).Text(T(TEXT("HITS")))]
				]
			]
			// Centre: banners (perfect dodge, stun / chain, parry, the angels adapting).
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 0.f, 0.f, 260.f)
			[
				SNew(SBox).Visibility_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return Hud && !Hud->Banner.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				.RenderOpacity(1.f)
				[
					SNew(SBorder).BorderImage(White()).Padding(FMargin(40.f, 6.f)).RenderTransform(Lean()).RenderTransformPivot(FVector2D(0.5f, 0.5f))
					.BorderBackgroundColor_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return FSlateColor(WithAlpha(Glass, Hud ? 0.8f * Hud->BannerAlpha : 0.f)); })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[SNew(STextBlock).Font(Display(58)).Text_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return T(Hud ? Hud->Banner : FString()); })
							.ColorAndOpacity_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return FSlateColor(Hud ? WithAlpha(Hud->BannerColour, Hud->BannerAlpha) : FLinearColor::Transparent); })
							.ShadowOffset(FVector2D(4.f, 3.f)).ShadowColorAndOpacity(WithAlpha(Violet, 0.8f))]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[SNew(SBox).MaxDesiredWidth(760.f)[SNew(STextBlock).Font(Mono(15)).AutoWrapText(true).Justification(ETextJustify::Center)
							.Text_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return T(Hud ? Hud->BannerSub.ToUpper() : FString()); })
							.ColorAndOpacity_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return FSlateColor(WithAlpha(Ink, Hud ? Hud->BannerAlpha : 0.f)); })]]
					]
				]
			]
			// Bottom right: skills. Moves on a leaning row, Special and Ultimate bigger beside them. (On touch screens the button
			// arc of BIBTouch shows the same cooldowns, so the slabs step aside.)
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 44.f, 64.f)
			[
				SNew(SVerticalBox).Visibility(BIBTouch::Enabled() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.f, 0.f)[SkillSlab(1, 128.f, 70.f, false)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.f, 0.f)[SkillSlab(2, 128.f, 70.f, false)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.f, 0.f)[SkillSlab(3, 128.f, 70.f, false)]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(5.f, 0.f)[SkillSlab(0, 128.f, 70.f, false)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(5.f, 0.f)[SkillSlab(4, 150.f, 92.f, false)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(5.f, 0.f)[SkillSlab(5, 190.f, 112.f, true)]
				]
			]
			// Bottom centre: the kit's prompt, kit line and traversal state.
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 36.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(SBox).Visibility_Lambda([Feel]() { const FBIBHud* Hud = Feel(); return Hud && (Hud->bDodgeCounterReady || !Hud->MoveState.IsEmpty()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FSlateColor(WithAlpha(Violet, 0.75f))).Padding(FMargin(14.f, 3.f)).RenderTransform(Lean()).RenderTransformPivot(FVector2D(0.5f, 0.5f))
						[SNew(STextBlock).Font(Mono(13)).ColorAndOpacity(FSlateColor(Ink)).Text_Lambda([Feel]()
						{
							const FBIBHud* Hud = Feel();
							if (!Hud) { return T(FString()); }
							return T(Hud->bDodgeCounterReady ? FString(TEXT("LMB  PHANTOM COUNTER")) : Hud->MoveState
								+ (Hud->MoveState == TEXT("CLIMBING") ? TEXT("   SPACE jump off  //  F let go") : TEXT("   SPACE up  //  CTRL dive")));
						})]]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(SBox).Visibility_Lambda([PC]() { const FBIBHudInfo& Info = HudInfo(PC); return Info.ActionLine.IsEmpty() || Info.bJudgeModeMenu ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
					[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FSlateColor(Glass)).Padding(FMargin(14.f, 5.f))
						[SNew(STextBlock).Font(Display(22)).ColorAndOpacity(FSlateColor(Gold)).Text_Lambda([PC]() { return T(HudInfo(PC).ActionLine); })]]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[SNew(SBox).MaxDesiredWidth(560.f)[SNew(STextBlock).Font(Mono(11)).ColorAndOpacity(FSlateColor(Cyan)).AutoWrapText(true).Justification(ETextJustify::Center)
					.Text_Lambda([PC]() { return T(HudInfo(PC).KitLine); })]]
			]
			// Dialogue (talk to the story contact).
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 150.f)
			[
				SNew(SBox).WidthOverride(900.f).Visibility_Lambda([PC]()
				{
					// the motion pack's story-dialogue panel replaces this box when the pack is imported
					return HudInfo(PC).bDialogueOpen && !BIBMotion::HasPiece(TEXT("story-dialogue")) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				[
					SNew(SBorder).BorderImage(Round(16.f)).BorderBackgroundColor(FSlateColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.94f))).Padding(FMargin(20.f, 14.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[DynText([PC]() { return HudInfo(PC).DialogueSpeaker; }, 18, Gold, true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)[DynText([PC]() { return HudInfo(PC).DialogueLine; }, 17, Ink, false, true)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[KeyCap(TEXT("T"), Gold)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)[Text(TEXT("continue"), 13, Dim)]]
					]
				]
			]
			// Judge's court setting (the choice prompt rides on ActionLine).
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 0.f, 0.f, 220.f)
			[
				SNew(SBox).Visibility_Lambda([PC]() { return HudInfo(PC).bJudgeModeMenu ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SBorder).BorderImage(Round(16.f)).BorderBackgroundColor(FSlateColor(FLinearColor(0.12f, 0.08f, 0.02f, 0.95f))).Padding(FMargin(20.f, 14.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Text(TEXT("COURT SETTING"), 20, Gold, true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)[DynText([PC]() { return HudInfo(PC).ActionLine; }, 16, Ink)]
					]
				]
			]
			// Narrator's Global Exchange (needs the cursor; the controller shows it while this is open).
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).Visibility_Lambda([PC]() { return HudInfo(PC).bExchangeOpen ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SBorder).BorderImage(Round(16.f)).BorderBackgroundColor(FSlateColor(FLinearColor(0.05f, 0.04f, 0.09f, 0.96f))).Padding(FMargin(20.f, 14.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Text(TEXT("GLOBAL EXCHANGE"), 20, Gold, true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)[DynText([PC]()
						{
							const FBIBHudInfo Info = HudInfo(PC);
							FString Line;
							for (int32 Stat = 0; Stat < Info.ExchangeValues.Num(); ++Stat)
							{
								Line += FString::Printf(TEXT("%s %.2fx%s   "), *UBIBBlueprintLibrary::GetExchangeStatName(Stat), Info.ExchangeValues[Stat],
									Stat == Info.ExchangeFrom ? TEXT(" <from") : (Stat == Info.ExchangeTo ? TEXT(" >to") : TEXT("")));
							}
							return Line;
						}, 15, Ink, false, true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[SNew(SBox)[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(TEXT("From"), 13, Dim)]
							+ SHorizontalBox::Slot().AutoWidth()[Card(Text(UBIBBlueprintLibrary::GetExchangeStatName(0), 13), Cyan, [PC]() { UBIBBlueprintLibrary::ExchangeSelect(PC.Get(), true, 0); }, true, false, false, 8.f)]
							+ SHorizontalBox::Slot().AutoWidth()[Card(Text(UBIBBlueprintLibrary::GetExchangeStatName(1), 13), Cyan, [PC]() { UBIBBlueprintLibrary::ExchangeSelect(PC.Get(), true, 1); }, true, false, false, 8.f)]
							+ SHorizontalBox::Slot().AutoWidth()[Card(Text(UBIBBlueprintLibrary::GetExchangeStatName(2), 13), Cyan, [PC]() { UBIBBlueprintLibrary::ExchangeSelect(PC.Get(), true, 2); }, true, false, false, 8.f)]
							+ SHorizontalBox::Slot().AutoWidth()[Card(Text(UBIBBlueprintLibrary::GetExchangeStatName(3), 13), Cyan, [PC]() { UBIBBlueprintLibrary::ExchangeSelect(PC.Get(), true, 3); }, true, false, false, 8.f)]
							+ SHorizontalBox::Slot().AutoWidth()[Card(Text(UBIBBlueprintLibrary::GetExchangeStatName(4), 13), Cyan, [PC]() { UBIBBlueprintLibrary::ExchangeSelect(PC.Get(), true, 4); }, true, false, false, 8.f)]]]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[SNew(SBox)[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(TEXT("To   "), 13, Dim)]
							+ SHorizontalBox::Slot().AutoWidth()[Card(Text(UBIBBlueprintLibrary::GetExchangeStatName(0), 13), Gold, [PC]() { UBIBBlueprintLibrary::ExchangeSelect(PC.Get(), false, 0); }, true, false, false, 8.f)]
							+ SHorizontalBox::Slot().AutoWidth()[Card(Text(UBIBBlueprintLibrary::GetExchangeStatName(1), 13), Gold, [PC]() { UBIBBlueprintLibrary::ExchangeSelect(PC.Get(), false, 1); }, true, false, false, 8.f)]
							+ SHorizontalBox::Slot().AutoWidth()[Card(Text(UBIBBlueprintLibrary::GetExchangeStatName(2), 13), Gold, [PC]() { UBIBBlueprintLibrary::ExchangeSelect(PC.Get(), false, 2); }, true, false, false, 8.f)]
							+ SHorizontalBox::Slot().AutoWidth()[Card(Text(UBIBBlueprintLibrary::GetExchangeStatName(3), 13), Gold, [PC]() { UBIBBlueprintLibrary::ExchangeSelect(PC.Get(), false, 3); }, true, false, false, 8.f)]
							+ SHorizontalBox::Slot().AutoWidth()[Card(Text(UBIBBlueprintLibrary::GetExchangeStatName(4), 13), Gold, [PC]() { UBIBBlueprintLibrary::ExchangeSelect(PC.Get(), false, 4); }, true, false, false, 8.f)]]]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()[Card(Text(TEXT("Exchange 10%"), 14, Ink, true), Good, [PC]() { UBIBBlueprintLibrary::ExchangeApply(PC.Get()); }, true, false, false, 10.f)]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)[Card(Text(TEXT("Close"), 14, Ink, true), Danger, [PC]() { UBIBBlueprintLibrary::CloseExchange(PC.Get()); }, true, false, false, 10.f)]]
					]
				]
			]
			// Controls only while F1 is held (the key caps on the ability bar cover the rest).
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 24.f)
			[
				SNew(SBox).Visibility_Lambda([PC]() { return PC.IsValid() && PC->IsInputKeyDown(EKeys::F1) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SBorder).BorderImage(Round(12.f)).BorderBackgroundColor(FSlateColor(Glass)).Padding(FMargin(14.f, 8.f))
					[Text(TEXT("WASD move | Shift run | Space jump | LMB attack string (sprint+LMB dash attack) | RMB block | F parry | Shift+F dodge | 1-3 moves | 4 special | E/R ult | Tab switch (relay strike on a broken angel) | W into a wall climb | Space / Ctrl swim | T talk | P pause"), 14, Ink)]
				]
			];
	}

	// ======================================================================= the flow-chart menu
	constexpr float CardHeight = 70.f;
	constexpr float CardGap = 10.f;
	constexpr float ColumnHeader = 52.f;
	constexpr float ColumnWidth = 344.f;

	FLinearColor NodeColour(const BIB::Ark::FNode& Node)
	{
		return FLinearColor(Node.Colour[0], Node.Colour[1], Node.Colour[2]);
	}

	TSharedRef<SWidget> NodeCard(const FWeakPC& PC, const BIB::Ark::FNode& Node, bool bSelected)
	{
		const FLinearColor Colour = NodeColour(Node);
		FString Tag;
		FLinearColor TagColour = Dim;
		switch (Node.Kind)
		{
		case BIB::Ark::ENodeKind::Branch: Tag = TEXT(">"); break;
		case BIB::Ark::ENodeKind::Teleport: Tag = TEXT("GO"); TagColour = Cyan; break;
		case BIB::Ark::ENodeKind::Page: Tag = TEXT("OPEN"); TagColour = Gold; break;
		case BIB::Ark::ENodeKind::Action: Tag = TEXT("RUN"); TagColour = Good; break;
		}
		const FString Id = F(Node.Id);
		const TSharedRef<SWidget> Content = SNew(SBox).HeightOverride(CardHeight - 16.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(SBorder).BorderImage(Round(10.f)).BorderBackgroundColor(FSlateColor(WithAlpha(Colour, 0.2f))).Padding(4.f)
				[IconImage(F(Node.Icon), 36.f)]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Text(F(Node.Label), 16, Ink, true)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
					[SNew(SBox).Visibility(Node.bBadge ? EVisibility::HitTestInvisible : EVisibility::Collapsed)[Badge()]]
				]
				+ SVerticalBox::Slot().AutoHeight()[Text(F(Node.Sub), 12, Dim)]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
			[Text(Tag, Node.Kind == BIB::Ark::ENodeKind::Branch ? 20 : 12, TagColour, true)]
		];
		return SNew(SBox).WidthOverride(ColumnWidth).HeightOverride(CardHeight)
		[
			Card(Content, Colour, [PC, Id]() { if (PC.IsValid()) { PC->ArkMenuClick(Id); } }, Node.bEnabled, bSelected, Node.bHighlight, 12.f, FMargin(12.f, 8.f),
				DisabledReason(PC, F(Node.Label), F(Node.Sub)))
		];
	}

	TSharedRef<SWidget> Column(const FWeakPC& PC, const FString& Title, const FLinearColor& Colour, const std::vector<BIB::Ark::FNode>& Nodes,
		const FString& Selected, double Born)
	{
		if (GFocusSink)
		{
			GFocusSink->FirstPreferred = GFocusSink->Items.Num();   // the newest column is where the player is working
		}
		TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
		for (const BIB::Ark::FNode& Node : Nodes)
		{
			List->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, CardGap)[NodeCard(PC, Node, F(Node.Id) == Selected)];
		}
		return SNew(SBox).WidthOverride(ColumnWidth)
			[
				Fade([Born]() { return static_cast<float>(FMath::Clamp((Now() - Born) / 0.2, 0.0, 1.0)); },
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(ColumnHeader)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Text(Title.ToUpper(), 13, Colour, true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
						[SNew(SBox).HeightOverride(3.f).WidthOverride(ColumnWidth)[SNew(SBIBGradient).From(WithAlpha(Colour, 0.95f)).To(WithAlpha(Colour, 0.f))]]
					]
				]
				+ SVerticalBox::Slot().AutoHeight()[List],
				[Born]() { return 24.f * (1.f - static_cast<float>(FMath::Clamp((Now() - Born) / 0.2, 0.0, 1.0))); })
			];
	}

	TSharedRef<SWidget> BuildMenuScreen(const FWeakPC& PC)
	{
		UBIBGameInstance* Owner = Instance(PC);
		if (!Owner || !PC.IsValid())
		{
			return SNew(SBox);
		}
		BIB::FProfile& Profile = Owner->GetProfile();
		const BIB::Pages::FUiState& State = Owner->UiState;
		FBIBArkUi& Ui = PC->ArkUi;
		FFocusSink Sink;
		TGuardValue<FFocusSink*> SinkGuard(GFocusSink, &Sink);
		// The tutorial's route opens itself (Menu > The Ark > Operators' Dome ...) so the highlighted card is on screen.
		const std::vector<std::string> Route = BIB::Ark::TutorialPath(Profile);
		if (Ui.MenuPath.Num() == 0 && Route.size() > 2)
		{
			for (size_t Index = 1; Index + 1 < Route.size(); ++Index)
			{
				Ui.MenuPath.Add(F(Route[Index]));
			}
		}
		// Columns: the root's children, then the children of each picked branch.
		TArray<FString> Parents = {F(BIB::Ark::RootId)};
		for (const FString& Picked : Ui.MenuPath)
		{
			const BIB::Ark::FNode Node = BIB::Ark::Node(Profile, State, TCHAR_TO_UTF8(*Picked));
			if (Node.Kind != BIB::Ark::ENodeKind::Branch)
			{
				break;
			}
			Parents.Add(Picked);
		}
		// Only a column that was not on screen before animates in: clicking a card used to replay the fade of every column.
		const int32 ShownBefore = Ui.MenuColumnsShown;
		Ui.MenuColumnsShown = Parents.Num();
		const double Fresh = Now();
		TSharedRef<SHorizontalBox> Flow = SNew(SHorizontalBox);
		for (int32 Index = 0; Index < Parents.Num(); ++Index)
		{
			const double Born = Index < ShownBefore ? Ui.MenuOpenedAt - 1.0 : Fresh;
			const BIB::Ark::FNode Parent = BIB::Ark::Node(Profile, State, TCHAR_TO_UTF8(*Parents[Index]));
			const std::vector<BIB::Ark::FNode> Kids = BIB::Ark::Children(Profile, State, TCHAR_TO_UTF8(*Parents[Index]));
			const FString Selected = Ui.MenuPath.IsValidIndex(Index) ? Ui.MenuPath[Index] : FString();
			const FLinearColor Colour = Index == 0 ? Cyan : NodeColour(Parent);
			if (Index > 0)
			{
				// Connector from the picked card in the previous column to this column's first card.
				const std::vector<BIB::Ark::FNode> Previous = BIB::Ark::Children(Profile, State, TCHAR_TO_UTF8(*Parents[Index - 1]));
				int32 Row = 0;
				for (int32 Search = 0; Search < static_cast<int32>(Previous.size()); ++Search)
				{
					if (F(Previous[static_cast<size_t>(Search)].Id) == Parents[Index])
					{
						Row = Search;
					}
				}
				const float FromY = ColumnHeader + Row * (CardHeight + CardGap) + CardHeight * 0.5f;
				const float ToY = ColumnHeader + CardHeight * 0.5f;
				Flow->AddSlot().AutoWidth()
				[
					SNew(SBox).WidthOverride(56.f).HeightOverride(ColumnHeader + FMath::Max<float>(static_cast<float>(Previous.size()), static_cast<float>(Kids.size())) * (CardHeight + CardGap))
					[SNew(SBIBLink).FromY(FromY).ToY(ToY).Colour(Colour).Born(Born + 0.05 * Index)]
				];
			}
			Flow->AddSlot().AutoWidth().VAlign(VAlign_Top)
			[Column(PC, Index == 0 ? FString(TEXT("Menu")) : F(Parent.Label), Colour, Kids, Selected, Born + 0.06 * Index)];
		}

		// Breadcrumb: Menu / The Ark / Town Square (each crumb jumps back to that column).
		TSharedRef<SHorizontalBox> Crumbs = SNew(SHorizontalBox);
		Crumbs->AddSlot().AutoWidth()[Card(Text(TEXT("MENU"), 15, Ink, true), Cyan, [PC]() { if (PC.IsValid()) { PC->ArkMenuClick(TEXT("menu")); } }, true, Ui.MenuPath.Num() == 0, false, 8.f, FMargin(10.f, 4.f))];
		for (int32 Index = 0; Index < Ui.MenuPath.Num(); ++Index)
		{
			const BIB::Ark::FNode Node = BIB::Ark::Node(Profile, State, TCHAR_TO_UTF8(*Ui.MenuPath[Index]));
			const FString Id = Ui.MenuPath[Index];
			Crumbs->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)[Text(TEXT("/"), 18, Dim)];
			Crumbs->AddSlot().AutoWidth()[Card(Text(F(Node.Label).ToUpper(), 15, Ink, true), NodeColour(Node), [PC, Id]() { if (PC.IsValid()) { PC->ArkMenuClick(Id); } },
				true, Index == Ui.MenuPath.Num() - 1, false, 8.f, FMargin(10.f, 4.f))];
		}

		FWeakPC Weak = PC;
		return SNew(SBIBArkScreen)
			.FocusTarget(Sink.Pick())
			.OnClose([Weak]() { if (Weak.IsValid()) { Weak->CloseArkScreens(); } })
			.OnMenu([Weak]() { if (Weak.IsValid()) { Weak->CloseArkScreens(); } })
			.OnBack([Weak]() { if (Weak.IsValid()) { Weak->ArkMenuUp(); } })
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(6.f)]
				+ SOverlay::Slot()[SNew(SBIBGradient).From(FLinearColor(0.01f, 0.01f, 0.04f, 0.92f)).To(FLinearColor(0.02f, 0.03f, 0.1f, 0.55f))]
				+ SOverlay::Slot()[ToastStrip(PC)]
				+ SOverlay::Slot().Padding(56.f, 34.f, 40.f, 30.f)
				[
					SNew(SVerticalBox)
					// Header: title, breadcrumb, close.
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[Text(TEXT("THE ARK"), 34, Ink, true)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[Crumbs]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[MenuButton(PC, true)]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 18.f)
					[SNew(SBox).HeightOverride(2.f)[SNew(SBIBGradient).From(WithAlpha(Cyan, 0.8f)).To(WithAlpha(Cyan, 0.f))]]
					// The flow chart.
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SScrollBox).Orientation(Orient_Horizontal)
						+ SScrollBox::Slot()
						[
							SNew(SScrollBox).Orientation(Orient_Vertical)
							+ SScrollBox::Slot()[Flow]
						]
					]
					// Footer: how it works, and the tutorial line.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(TEXT("GO"), 12, Cyan, true)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 18.f, 0.f)[Text(TEXT("teleports you there"), 12, Dim)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(TEXT("OPEN"), 12, Gold, true)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 18.f, 0.f)[Text(TEXT("opens it here"), 12, Dim)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[KeyCap(TEXT("Esc"), Dim)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 18.f, 0.f)[Text(TEXT("close"), 12, Dim)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[KeyCap(TEXT("Backspace"), Dim)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 18.f, 0.f)[Text(TEXT("up a level"), 12, Dim)]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Right)
						[Text(F(BIB::Ark::TutorialLine(Profile)), 13, Gold, false, true)]
					]
				]
			];
	}

	// ======================================================================= the station panel
	TSharedRef<SWidget> TileCard(const FWeakPC& PC, const BIB::Pages::FPageAction& Action, const FLinearColor& Colour)
	{
		const FString Id = F(Action.Id);
		const FString IconId = F(Action.Icon);
		const bool bPortrait = IconId.StartsWith(TEXT("portrait:"));
		const TSharedRef<SWidget> Content = SNew(SBox).HeightOverride(72.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(SBorder).BorderImage(Round(10.f)).BorderBackgroundColor(FSlateColor(WithAlpha(Colour, 0.16f))).Padding(3.f)
				[IconImage(IconId, bPortrait ? 52.f : 42.f)]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[Text(F(Action.Label), 15, Action.bHighlight ? Gold : Ink, true, true)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[SNew(SBox).Visibility(Action.bBadge ? EVisibility::HitTestInvisible : EVisibility::Collapsed)[Badge()]]
				]
				+ SVerticalBox::Slot().AutoHeight()[Text(F(Action.Sub), 12, Dim, false, true)]
			]
		];
		return SNew(SBox).WidthOverride(262.f)
		[
			Card(Content, Colour, [PC, Id]() { if (PC.IsValid()) { PC->ArkRunAction(Id); } }, Action.bEnabled, false, Action.bHighlight, 12.f, FMargin(12.f, 8.f),
				DisabledReason(PC, F(Action.Label), F(Action.Sub)))
		];
	}

	TSharedRef<SWidget> BarButton(const FWeakPC& PC, const BIB::Pages::FPageAction& Action, const FLinearColor& Colour)
	{
		const FString Id = F(Action.Id);
		const TSharedRef<SWidget> Content = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Text(F(Action.Label), 15, Action.bHighlight ? Gold : Ink, true)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SBox).Visibility(Action.Sub.empty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)[Text(F(Action.Sub), 11, Dim)]];
		return Card(Content, Action.bHighlight ? Gold : Colour, [PC, Id]() { if (PC.IsValid()) { PC->ArkRunAction(Id); } }, Action.bEnabled, Action.bHighlight, Action.bHighlight, 22.f, FMargin(16.f, 9.f),
			DisabledReason(PC, F(Action.Label), F(Action.Sub)));
	}

	TSharedRef<SWidget> BuildPanelScreen(const FWeakPC& PC)
	{
		UBIBGameInstance* Owner = Instance(PC);
		if (!Owner || !PC.IsValid())
		{
			return SNew(SBox);
		}
		BIB::FProfile& Profile = Owner->GetProfile();
		const BIB::Pages::FPage Page = BIB::Pages::Build(Profile, Owner->UiState);
		const FBIBArkUi& Ui = PC->ArkUi;
		const FLinearColor Colour = Ui.PanelColour;
		const BIB::Ark::FStation* Station = Ui.PanelStation.IsEmpty() ? nullptr : BIB::Ark::FindStation(TCHAR_TO_UTF8(*Ui.PanelStation));
		const bool bAtRoot = Owner->UiState.Tab == Ui.RootTab && Owner->UiState.Page == TCHAR_TO_UTF8(*Ui.RootPage) && Owner->UiState.SummonOps.empty();
		FWeakPC Weak = PC;
		FFocusSink Sink;
		TGuardValue<FFocusSink*> SinkGuard(GFocusSink, &Sink);

		// Header: station name over its colour, the wallet, close.
		TSharedRef<SHorizontalBox> Wallet = SNew(SHorizontalBox);
		for (const BIB::Pages::FResourceView& View : BIB::Pages::Resources(Profile))
		{
			Wallet->AddSlot().AutoWidth().Padding(4.f, 0.f)
			[
				SNew(SBorder).BorderImage(Round(12.f)).BorderBackgroundColor(FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.35f))).Padding(FMargin(8.f, 4.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 5.f, 0.f)[IconImage(F(View.Icon), 18.f)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(FText::AsNumber(View.Amount).ToString(), 13, Ink, true)]
				]
			];
		}

		// Counter tabs (Roll Call: Operators / Squad / Elements ...).
		TSharedRef<SHorizontalBox> Counters = SNew(SHorizontalBox);
		if (Station && Station->Counters.size() > 1)
		{
			for (int32 Index = 0; Index < static_cast<int32>(Station->Counters.size()); ++Index)
			{
				const BIB::Ark::FCounter& Entry = Station->Counters[static_cast<size_t>(Index)];
				const FString StationId = Ui.PanelStation;
				const bool bOn = Index == Ui.PanelCounter;
				Counters->AddSlot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
				[
					Card(SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)[IconImage(F(Entry.Icon), 20.f)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(F(Entry.Label), 14, Ink, true)],
						Colour, [Weak, StationId, Index]() { if (Weak.IsValid()) { Weak->ArkOpenStation(StationId, Index); } }, true, bOn, false, 16.f, FMargin(12.f, 6.f))
				];
			}
		}

		Sink.FirstPreferred = Sink.Items.Num();   // focus starts on the first tile / button, not on the counter tabs

		// Body: text lines, then the tiles.
		TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
		for (const std::string& Line : Page.Lines)
		{
			Body->AddSlot().AutoHeight().Padding(0.f, 1.f)[Text(F(Line), 13, Dim, false, true)];
		}
		TSharedRef<SWrapBox> Grid = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10.f, 10.f));
		for (const BIB::Pages::FPageAction& Action : Page.Grid)
		{
			Grid->AddSlot()[TileCard(PC, Action, Colour)];
		}
		Body->AddSlot().AutoHeight().Padding(0.f, Page.Lines.empty() ? 0.f : 12.f, 0.f, 0.f)[Grid];

		TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
		for (const BIB::Pages::FPageAction& Action : Page.Bar)
		{
			Buttons->AddSlot().FillWidth(1.f).Padding(4.f, 0.f)[BarButton(PC, Action, Colour)];
		}

		const FString Coach = F(Page.Coach);
		const FString Status = F(Owner->UiState.Status);
		const FString PortraitId = Page.Portrait.empty() ? FString() : TEXT("full:") + F(Page.Portrait);
		const double Born = Ui.PanelOpenedAt;

		const TSharedRef<SWidget> Panel = SNew(SBorder).BorderImage(Round(22.f)).BorderBackgroundColor(FSlateColor(Glass)).Padding(0.f)
		[
			SNew(SVerticalBox)
			// Header strip.
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBox).HeightOverride(96.f)[SNew(SBIBGradient).From(WithAlpha(Colour, 0.55f)).To(WithAlpha(Colour, 0.f))]]
				+ SOverlay::Slot().Padding(FMargin(22.f, 14.f, 16.f, 10.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)
					[SNew(SBorder).BorderImage(Round(14.f)).BorderBackgroundColor(FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.3f))).Padding(6.f)[IconImage(Ui.PanelIcon, 52.f)]]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Text(Ui.PanelTitle.ToUpper(), 28, Ink, true)]
						+ SVerticalBox::Slot().AutoHeight()[Text(Station ? F(Station->Blurb) : FString(TEXT("Commander")), 13, Ink)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
					[
						SNew(SBox).WidthOverride(46.f).HeightOverride(46.f)
						[Card(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[Text(TEXT("X"), 20, Ink, true)], Danger,
							[Weak]() { if (Weak.IsValid()) { Weak->CloseArkScreens(); } }, true, false, false, 12.f, FMargin(0.f))]
					]
				]
			]
			// Counters on the left, the wallet on the right.
			+ SVerticalBox::Slot().AutoHeight().Padding(22.f, 4.f, 18.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Counters]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Wallet]
			]
			// Tutorial instruction.
			+ SVerticalBox::Slot().AutoHeight().Padding(22.f, 8.f, 22.f, 0.f)
			[
				SNew(SBox).Visibility(Coach.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
				[SNew(SBorder).BorderImage(Round(10.f)).BorderBackgroundColor(FSlateColor(FLinearColor(0.3f, 0.22f, 0.02f, 0.85f))).Padding(FMargin(12.f, 6.f))[Text(Coach, 14, Gold, true, true)]]
			]
			// Page title row with Back.
			+ SVerticalBox::Slot().AutoHeight().Padding(22.f, 12.f, 22.f, 4.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
				[
					SNew(SBox).Visibility(bAtRoot ? EVisibility::Collapsed : EVisibility::Visible)
					[Card(Text(TEXT("<  Back"), 14, Ink, true), Colour, [Weak]() { if (Weak.IsValid()) { Weak->ArkPanelBack(); } }, true, false, false, 14.f, FMargin(12.f, 5.f))]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Text(F(Page.Title), 22, FLinearColor::LerpUsingHSV(Colour, FLinearColor::White, 0.35f), true)]
					+ SVerticalBox::Slot().AutoHeight()[Text(F(Page.Subtitle), 13, Dim, false, true)]
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(22.f, 6.f, 14.f, 6.f)
			[
				SNew(SScrollBox) + SScrollBox::Slot().Padding(0.f, 0.f, 8.f, 0.f)[Body]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(18.f, 6.f, 18.f, 4.f)[Buttons]
			+ SVerticalBox::Slot().AutoHeight().Padding(22.f, 4.f, 22.f, 14.f)
			[
				SNew(SBox).Visibility(Status.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
				[Text(Status, 14, Status.Contains(TEXT("not")) || Status.Contains(TEXT("Need")) || Status.Contains(TEXT("need")) ? Gold : Good, true, true)]
			]
		];

		return SNew(SBIBArkScreen)
			.FocusTarget(Sink.Pick())
			.OnClose([Weak]() { if (Weak.IsValid()) { Weak->CloseArkScreens(); } })
			.OnBack([Weak]() { if (Weak.IsValid()) { Weak->ArkPanelBack(); } })
			.OnMenu([Weak]() { if (Weak.IsValid()) { Weak->OpenArkMenu(); } })
			[
				SNew(SOverlay)
				// The Ark stays visible on the left; a soft shade on the right makes the glass readable.
				+ SOverlay::Slot()[SNew(SBIBGradient).From(FLinearColor(0.f, 0.f, 0.f, 0.f)).To(FLinearColor(0.f, 0.01f, 0.04f, 0.7f))]
				+ SOverlay::Slot()[ToastStrip(PC)]
				+ SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.32f).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(20.f, 20.f, 0.f, 0.f)
					[
						// A portrait when the page has one (operator details, summon results, banners); otherwise the Ark itself.
						SNew(SBox).Visibility(PortraitId.IsEmpty() ? EVisibility::Hidden : EVisibility::HitTestInvisible)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Bottom)[SNew(SImage).Image(Icon(PortraitId, 760.f))]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 26.f)
							[
								SNew(SBorder).BorderImage(Round(12.f)).BorderBackgroundColor(FSlateColor(Glass)).Padding(FMargin(16.f, 8.f))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Text(F(Page.PortraitName), 24, Ink, true)]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Text(F(Page.PortraitStars), 14, Gold, true)]
								]
							]
						]
					]
					+ SHorizontalBox::Slot().FillWidth(0.68f).Padding(0.f, 26.f, 26.f, 26.f)
					[
						Fade([Born]() { return static_cast<float>(FMath::Clamp((Now() - Born) / 0.2, 0.0, 1.0)); }, Panel,
							[Born]() { return 60.f * (1.f - static_cast<float>(FMath::Clamp((Now() - Born) / 0.25, 0.0, 1.0))); })
					]
				]
			];
	}
}

TSharedRef<SWidget> BIBArkUI::BuildLayer(ABIBPlayerController* PC, bool bBattleHud)
{
	using namespace BIBArkUIInternal;
	const FWeakPC Weak(PC);
	TSharedRef<SOverlay> Layer = SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible);
	Layer->AddSlot()[BuildArkHud(Weak)];
	Layer->AddSlot()[BuildTravelNote(Weak)];
	if (bBattleHud)
	{
		Layer->AddSlot()[BuildBattleHud(Weak)];
	}
	return Layer;
}

TSharedRef<SWidget> BIBArkUI::BuildMenu(ABIBPlayerController* PC)
{
	return BIBArkUIInternal::BuildMenuScreen(BIBArkUIInternal::FWeakPC(PC));
}

TSharedRef<SWidget> BIBArkUI::BuildPanel(ABIBPlayerController* PC)
{
	return BIBArkUIInternal::BuildPanelScreen(BIBArkUIInternal::FWeakPC(PC));
}
