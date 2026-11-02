#include "BIBGame/BIBMotion.h"

#include "BIBGame/BIBAnimeStyle.h"
#include "BIBGame/BIBFonts.h"

#include "Brushes/SlateImageBrush.h"
#include "Dom/JsonObject.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SLeafWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"

namespace BIBMotionInternal
{
	struct FSlotDef
	{
		FString Name;
		FBox2D Rect = FBox2D(ForceInit);
		FString Font;
		float Size = 26.f;
		FLinearColor Colour = FLinearColor::White;
		FLinearColor Shadow = FLinearColor::Transparent;
		bool bShadow = false;
		int32 Align = 0;   // 0 left, 1 centre, 2 right
		float From = -1.f;
		float To = 1000.f;
	};

	struct FPiece
	{
		FString Id;
		bool bTransition = false;
		float Duration = 5.f;
		TArray<int32> Frames;
		TMap<FString, FVector2D> Clips;
		TArray<FSlotDef> Slots;
		FVector2D Anchor = FVector2D(960.0, 540.0);
		TMap<FString, FVector2D> Anchors;
		FIntPoint Cell = FIntPoint(2, 2);
		int32 Cols = 1;
		int32 Rows = 1;
		int32 Pages = 0;
		FIntPoint Page = FIntPoint(0, 0);   // atlas page size (0 = exactly cols x cells)
		FBox2D Box = FBox2D(ForceInit);
		TArray<TStrongObjectPtr<UTexture2D>> Textures;   // loaded on first use
		TSet<int32> Requested;                           // pages already queued for an async load (no hitch when they are reached)
		TArray<TSharedPtr<FStreamableHandle>> Handles;
		FString Mode;
		FString Who;
		FString Slot;
		double LastUsed = 0.0;
	};

	struct FActive
	{
		BIBMotion::FPlay Params;
		FPiece* Piece = nullptr;
		FVector2D Clip = FVector2D::ZeroVector;
		FVector2D AnchorPoint = FVector2D(960.0, 540.0);
		double Started = 0.0;
		bool bFollows = false;
		// A followed actor is projected once per world step: the 3D scene shows a held image between steps, so a VFX that
		// slid with the live actor would run ahead of the character it belongs to.
		int64 LastStep = -1;
		FVector2D CachedFeet = FVector2D::ZeroVector;
		FVector2D CachedHead = FVector2D::ZeroVector;
		bool bCached = false;
	};

	int32 GFps = 12;
	TMap<FString, FPiece> GPieces;
	TArray<FActive> GActive;
	TSet<FString> GPlayed;
	bool GLoaded = false;
	FVector2D GCursor = FVector2D(960.0, 540.0);
	TWeakObjectPtr<APlayerController> GController;
	TMap<FString, FString> GAbility;   // "who|slotkey" -> piece

	FString SlotKey(const FString& Slot)
	{
		const FString S = Slot.TrimStartAndEnd();
		if (S.StartsWith(TEXT("Diano M")))
		{
			return TEXT("D") + S.Mid(7, 1);
		}
		if (S.StartsWith(TEXT("M1")))
		{
			return TEXT("M1");
		}
		if (S.StartsWith(TEXT("Move ")) && S.Len() >= 6 && FChar::IsDigit(S[5]))
		{
			return S.Mid(5, 1);
		}
		if (S.StartsWith(TEXT("Move alt")))
		{
			return TEXT("MALT");
		}
		if (S.StartsWith(TEXT("Special alt")))
		{
			return TEXT("SALT");
		}
		if (S.StartsWith(TEXT("Special")))
		{
			return TEXT("S");
		}
		if (S == TEXT("Ultimate"))
		{
			return TEXT("U");
		}
		if (S.StartsWith(TEXT("Ultimate (")))
		{
			return TEXT("U2");
		}
		return S.ToUpper();   // IDLE, ALWAYS, PASSIVE, PASSIVES, ALT, ALT (PARRY) ...
	}

	FLinearColor Hex(const FString& Text)
	{
		return Text.IsEmpty() ? FLinearColor::White : FLinearColor(FColor::FromHex(Text));
	}

	void Load()
	{
		if (GLoaded)
		{
			return;
		}
		GLoaded = true;
		FString Json;
		const FString Path = FPaths::ProjectContentDir() / TEXT("BIBMotion/motion.json");
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			UE_LOG(LogTemp, Log, TEXT("BIBMotion: no %s (run Tools/import_motion_pack.sh); motion pack off."), *Path);
			return;
		}
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("BIBMotion: motion.json does not parse."));
			return;
		}
		GFps = FMath::Max(1, static_cast<int32>(Root->GetNumberField(TEXT("fps"))));
		const TSharedPtr<FJsonObject>* Pieces = nullptr;
		if (!Root->TryGetObjectField(TEXT("pieces"), Pieces))
		{
			return;
		}
		for (const auto& Pair : (*Pieces)->Values)
		{
			const TSharedPtr<FJsonObject> P = Pair.Value->AsObject();
			if (!P.IsValid())
			{
				continue;
			}
			FPiece Piece;
			Piece.Id = Pair.Key;
			Piece.bTransition = P->GetStringField(TEXT("kind")) == TEXT("transition");
			Piece.Duration = static_cast<float>(P->GetNumberField(TEXT("duration")));
			for (const TSharedPtr<FJsonValue>& V : P->GetArrayField(TEXT("frames")))
			{
				Piece.Frames.Add(static_cast<int32>(V->AsNumber()));
			}
			const TSharedPtr<FJsonObject>* Clips = nullptr;
			if (P->TryGetObjectField(TEXT("clips"), Clips))
			{
				for (const auto& C : (*Clips)->Values)
				{
					const TArray<TSharedPtr<FJsonValue>>& Range = C.Value->AsArray();
					if (Range.Num() == 2)
					{
						FString Key;
						Key = C.Key;   // UE 5.8: JSON keys are shared strings
						Piece.Clips.Add(Key, FVector2D(Range[0]->AsNumber(), Range[1]->AsNumber()));
					}
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* AnchorArr = nullptr;
			if (P->TryGetArrayField(TEXT("anchor"), AnchorArr) && AnchorArr->Num() == 2)
			{
				Piece.Anchor = FVector2D((*AnchorArr)[0]->AsNumber(), (*AnchorArr)[1]->AsNumber());
			}
			const TSharedPtr<FJsonObject>* Anchors = nullptr;
			if (P->TryGetObjectField(TEXT("anchors"), Anchors))
			{
				for (const auto& A : (*Anchors)->Values)
				{
					const TArray<TSharedPtr<FJsonValue>>& XY = A.Value->AsArray();
					if (XY.Num() == 2)
					{
						FString Key;
						Key = A.Key;
						Piece.Anchors.Add(Key, FVector2D(XY[0]->AsNumber(), XY[1]->AsNumber()));
					}
				}
			}
			for (const TSharedPtr<FJsonValue>& V : P->GetArrayField(TEXT("slots")))
			{
				const TSharedPtr<FJsonObject> S = V->AsObject();
				FSlotDef Slot;
				Slot.Name = S->GetStringField(TEXT("name"));
				const TArray<TSharedPtr<FJsonValue>>& Rc = S->GetArrayField(TEXT("rect"));
				const double X = Rc[0]->AsNumber(), Y = Rc[1]->AsNumber();
				Slot.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + Rc[2]->AsNumber(), Y + Rc[3]->AsNumber()));
				Slot.Font = S->GetStringField(TEXT("font"));
				Slot.Size = static_cast<float>(S->GetNumberField(TEXT("size")));
				Slot.Colour = Hex(S->GetStringField(TEXT("colour")));
				const FString Align = S->GetStringField(TEXT("align"));
				Slot.Align = Align == TEXT("center") ? 1 : (Align == TEXT("right") ? 2 : 0);
				FString Shadow;
				if (S->TryGetStringField(TEXT("shadow"), Shadow))
				{
					Slot.bShadow = true;
					Slot.Shadow = Hex(Shadow);
				}
				const TArray<TSharedPtr<FJsonValue>>* Window = nullptr;
				if (S->TryGetArrayField(TEXT("window"), Window) && Window->Num() == 2)
				{
					Slot.From = static_cast<float>((*Window)[0]->AsNumber());
					Slot.To = static_cast<float>((*Window)[1]->AsNumber());
				}
				Piece.Slots.Add(Slot);
			}
			P->TryGetStringField(TEXT("mode"), Piece.Mode);
			P->TryGetStringField(TEXT("who"), Piece.Who);
			P->TryGetStringField(TEXT("slot"), Piece.Slot);
			const TArray<TSharedPtr<FJsonValue>>& Cell = P->GetArrayField(TEXT("cell"));
			Piece.Cell = FIntPoint(static_cast<int32>(Cell[0]->AsNumber()), static_cast<int32>(Cell[1]->AsNumber()));
			Piece.Cols = static_cast<int32>(P->GetNumberField(TEXT("cols")));
			Piece.Rows = static_cast<int32>(P->GetNumberField(TEXT("rows")));
			Piece.Pages = static_cast<int32>(P->GetNumberField(TEXT("pages")));
			const TArray<TSharedPtr<FJsonValue>>* PageSize = nullptr;
			if (P->TryGetArrayField(TEXT("page"), PageSize) && PageSize->Num() == 2)
			{
				Piece.Page = FIntPoint(static_cast<int32>((*PageSize)[0]->AsNumber()), static_cast<int32>((*PageSize)[1]->AsNumber()));
			}
			const TArray<TSharedPtr<FJsonValue>>& Box = P->GetArrayField(TEXT("box"));
			Piece.Box = FBox2D(FVector2D(Box[0]->AsNumber(), Box[1]->AsNumber()),
				FVector2D(Box[0]->AsNumber() + Box[2]->AsNumber(), Box[1]->AsNumber() + Box[3]->AsNumber()));
			// Only pieces whose atlas pages were imported count as available.
			const FString First = FString::Printf(TEXT("/Game/BIBMotion/%s/T_%s_01.T_%s_01"), *Piece.Id, *Piece.Id.Replace(TEXT("-"), TEXT("_")), *Piece.Id.Replace(TEXT("-"), TEXT("_")));
			if (Piece.Pages > 0 && FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/BIBMotion/%s/T_%s_01"), *Piece.Id, *Piece.Id.Replace(TEXT("-"), TEXT("_")))))
			{
				if (!Piece.Who.IsEmpty() && !Piece.Slot.IsEmpty())
				{
					// first piece per operator slot wins (alt versions keep their own keys)
					const FString Key = Piece.Who + TEXT("|") + SlotKey(Piece.Slot);
					if (!GAbility.Contains(Key))
					{
						GAbility.Add(Key, Piece.Id);
					}
				}
				GPieces.Add(Piece.Id, MoveTemp(Piece));
			}
			(void)First;
		}
		UE_LOG(LogTemp, Log, TEXT("BIBMotion: %d motion pieces available."), GPieces.Num());
	}

	FString PagePath(const FPiece& Piece, int32 Page)
	{
		const FString Name = FString::Printf(TEXT("T_%s_%02d"), *Piece.Id.Replace(TEXT("-"), TEXT("_")), Page + 1);
		return FString::Printf(TEXT("/Game/BIBMotion/%s/%s.%s"), *Piece.Id, *Name, *Name);
	}

	/** Queues a page for an asynchronous load so reaching it mid-play does not stall the game thread. */
	void Prefetch(FPiece& Piece, int32 Page)
	{
		if (Page < 0 || Page >= Piece.Pages || Piece.Requested.Contains(Page))
		{
			return;
		}
		if (Piece.Textures.IsValidIndex(Page) && Piece.Textures[Page].IsValid())
		{
			return;
		}
		Piece.Requested.Add(Page);
		if (UAssetManager::IsInitialized())
		{
			Piece.Handles.Add(UAssetManager::GetStreamableManager().RequestAsyncLoad(FSoftObjectPath(PagePath(Piece, Page))));
		}
		Piece.LastUsed = FPlatformTime::Seconds();
	}

	UTexture2D* PageTexture(FPiece& Piece, int32 Page)
	{
		if (Piece.Textures.Num() != Piece.Pages)
		{
			Piece.Textures.SetNum(Piece.Pages);
		}
		if (!Piece.Textures.IsValidIndex(Page))
		{
			return nullptr;
		}
		if (!Piece.Textures[Page].IsValid())
		{
			UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *PagePath(Piece, Page), nullptr, LOAD_NoWarn | LOAD_Quiet);
			Piece.Textures[Page] = TStrongObjectPtr<UTexture2D>(Texture);
		}
		Piece.LastUsed = FPlatformTime::Seconds();
		return Piece.Textures[Page].Get();
	}

	/** The atlas cell of the piece's frame at Seconds (piece time). */
	bool FrameAt(FPiece& Piece, double Seconds, UTexture2D*& OutTexture, FBox2f& OutUV)
	{
		if (Piece.Frames.Num() == 0)
		{
			return false;
		}
		const int32 Index = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(Seconds * GFps)), 0, Piece.Frames.Num() - 1);
		const int32 Unique = Piece.Frames[Index];
		const int32 PerPage = FMath::Max(1, Piece.Cols * Piece.Rows);
		const int32 Page = Unique / PerPage;
		const int32 Cell = Unique % PerPage;
		OutTexture = PageTexture(Piece, Page);
		Prefetch(Piece, Page + 1);   // the next page streams in while this one plays
		if (!OutTexture)
		{
			return false;
		}
		const float PageW = Piece.Page.X > 0 ? static_cast<float>(Piece.Page.X) : static_cast<float>(Piece.Cols * Piece.Cell.X);
		const float PageH = Piece.Page.Y > 0 ? static_cast<float>(Piece.Page.Y) : static_cast<float>(Piece.Rows * Piece.Cell.Y);
		const float CellU = Piece.Cell.X / PageW;
		const float CellV = Piece.Cell.Y / PageH;
		const float U = static_cast<float>(Cell % Piece.Cols) * CellU;
		const float V = static_cast<float>(Cell / Piece.Cols) * CellV;
		OutUV = FBox2f(FVector2f(U, V), FVector2f(U + CellU, V + CellV));
		return true;
	}

	void DrawCell(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, UTexture2D* Texture, const FBox2f& UV, const FVector2D& Pos,
		const FVector2D& Size, float Opacity)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetUVRegion(UV);
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Pos))), &Brush, ESlateDrawEffect::None,
			FLinearColor(1.f, 1.f, 1.f, Opacity));
	}

	FSlateFontInfo SlotFont(const FSlotDef& Slot, float PixelSize)
	{
		const int32 Size = FMath::Max(6, FMath::RoundToInt(PixelSize * 0.75f));   // CSS px -> Slate points
		if (Slot.Font == TEXT("display"))
		{
			return BIBFonts::Display(Size);
		}
		if (Slot.Font == TEXT("numeral"))
		{
			return BIBFonts::Numeral(Size);
		}
		return BIBFonts::Mono(Size);
	}

	class SMotionLayer : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMotionLayer) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&) {}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.0, 1.0); }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer,
			const FWidgetStyle&, bool) const override
		{
			const FVector2D Size = Geometry.GetLocalSize();
			// The pack is laid out on a 1920x1080 frame: fit its height, centre it horizontally.
			const double S = Size.Y / 1080.0;
			const double OffsetX = (Size.X - 1920.0 * S) * 0.5;
			if (FSlateApplication::IsInitialized())
			{
				GCursor = Geometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
			}
			const double Now = FPlatformTime::Seconds();
			int32 Top = Layer;
			for (int32 Index = 0; Index < GActive.Num(); ++Index)
			{
				FActive& Play = GActive[Index];
				FPiece& Piece = *Play.Piece;
				const double Length = FMath::Max(0.01, Play.Clip.Y - Play.Clip.X);
				double Local = (Now - Play.Started) * Play.Params.Speed;
				if (Local < 0.0)
				{
					continue;   // delayed start
				}
				if (Local > Length)
				{
					if (!Play.Params.bLoop)
					{
						continue;   // finished; removed on the next tick
					}
					Local = FMath::Fmod(Local, Length);
				}
				const double PieceTime = Play.Clip.X + Local;
				UTexture2D* Texture = nullptr;
				FBox2f UV;
				if (!FrameAt(Piece, PieceTime, Texture, UV))
				{
					continue;
				}
				double Scale = S * Play.Params.Scale;
				if (Play.Params.Follow.IsValid())
				{
					// World VFX: feet and head of the actor on screen; the pack's body reference is 300 px tall.
					AActor* Actor = Play.Params.Follow.Get();
					APlayerController* PC = GController.Get();
					float Half = 90.f;
					if (const ACharacter* Char = Cast<ACharacter>(Actor))
					{
						Half = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
					}
					const int64 Step = PC ? ABIBAnimeStyleVolume::GetVisualStep(PC->GetWorld()) : -1;
					FVector2D FeetScreen, HeadScreen;
					if (Step >= 0 && Play.bCached && Step == Play.LastStep)
					{
						FeetScreen = Play.CachedFeet;   // the scene is still showing its held image: stay on the character in it
						HeadScreen = Play.CachedHead;
					}
					else
					{
						const FVector Feet = Actor->GetActorLocation() - FVector(0.0, 0.0, Half);
						if (!PC || !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Feet, FeetScreen, false)
							|| !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Feet + FVector(0.0, 0.0, Play.Params.BodyCm), HeadScreen, false))
						{
							continue;   // behind the camera
						}
						Play.LastStep = Step;
						Play.CachedFeet = FeetScreen;
						Play.CachedHead = HeadScreen;
						Play.bCached = true;
					}
					Play.Params.bAnchored = true;
					Play.Params.Anchor = FeetScreen;
					Scale = FMath::Clamp(FVector2D::Distance(FeetScreen, HeadScreen) / 300.0, 0.05, 4.0) * Play.Params.Scale;
				}
				const bool bFlip = Play.Params.bFlip && Play.Params.bAnchored;
				auto Map = [&](const FVector2D& P) -> FVector2D
				{
					if (Play.Params.bAnchored)
					{
						FVector2D D = (P - Play.AnchorPoint) * Scale;
						if (bFlip)
						{
							D.X = -D.X;
						}
						return Play.Params.Anchor + D;
					}
					return FVector2D(OffsetX, 0.0) + P * S;
				};
				FVector2D Pos = Map(Piece.Box.Min);
				const FVector2D Dim = (Piece.Box.Max - Piece.Box.Min) * (Play.Params.bAnchored ? Scale : S);
				if (bFlip)
				{
					Pos.X -= Dim.X;   // mirrored: the box's min corner maps to its right edge
					Swap(UV.Min.X, UV.Max.X);
				}
				// A soft fade over the last 0.12 s so clips cut out of the middle of a piece end cleanly.
				const float Tail = static_cast<float>(FMath::Clamp((Length - Local) / 0.12, 0.0, 1.0));
				const float Opacity = Play.Params.Opacity * (Play.Params.bLoop ? 1.f : Tail);
				DrawCell(Out, Top + 1, Geometry, Texture, UV, Pos, Dim, Opacity);
				// Real values in the pack's blanked places.
				for (const FSlotDef& Slot : Piece.Slots)
				{
					const FString* Value = Play.Params.Values.Find(Slot.Name);
					if (!Value || Value->IsEmpty() || PieceTime < Slot.From || PieceTime > Slot.To)
					{
						continue;
					}
					const float In = static_cast<float>(FMath::Clamp((PieceTime - Slot.From) / 0.1, 0.0, 1.0));
					const double PixelScale = Play.Params.bAnchored ? Scale : S;
					const FSlateFontInfo Font = SlotFont(Slot, static_cast<float>(Slot.Size * PixelScale));
					const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(*Value, Font);
					const FVector2D RectMin = Map(Slot.Rect.Min);
					const FVector2D RectSize = (Slot.Rect.Max - Slot.Rect.Min) * PixelScale;
					FVector2D At = RectMin;
					if (Slot.Align == 1)
					{
						At.X += (RectSize.X - TextSize.X) * 0.5;
					}
					else if (Slot.Align == 2)
					{
						At.X += RectSize.X - TextSize.X;
					}
					At.Y += FMath::Max(0.0, (RectSize.Y - TextSize.Y) * 0.5);
					const float Alpha = Opacity * In;
					if (Slot.bShadow)
					{
						FLinearColor Shadow = Slot.Shadow;
						Shadow.A *= Alpha;
						FSlateDrawElement::MakeText(Out, Top + 2, Geometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(At + FVector2D(7.0, 6.0) * PixelScale))),
							*Value, Font, ESlateDrawEffect::None, Shadow);
					}
					FLinearColor Colour = Slot.Colour;
					Colour.A *= Alpha;
					FSlateDrawElement::MakeText(Out, Top + 3, Geometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(At))), *Value, Font,
						ESlateDrawEffect::None, Colour);
				}
				Top += 4;
			}
			return Top + 1;
		}

		virtual void Tick(const FGeometry&, const double, const float) override
		{
			const double Now = FPlatformTime::Seconds();
			GActive.RemoveAll([Now](const FActive& Play)
			{
				if (Play.bFollows && !Play.Params.Follow.IsValid())
				{
					return true;   // the followed actor is gone
				}
				return !Play.Params.bLoop && (Now - Play.Started) * Play.Params.Speed > (Play.Clip.Y - Play.Clip.X) + 0.05;
			});
			// Unused atlases are released after a while (the pack is big; only what plays stays in memory).
			for (auto& Pair : GPieces)
			{
				if (Pair.Value.Textures.Num() > 0 && Now - Pair.Value.LastUsed > 20.0 && !GActive.ContainsByPredicate([&Pair](const FActive& P) { return P.Piece == &Pair.Value; }))
				{
					Pair.Value.Textures.Reset();
					Pair.Value.Requested.Reset();
					Pair.Value.Handles.Reset();
				}
			}
		}
	};
}

using namespace BIBMotionInternal;

bool BIBMotion::Play(const FPlay& Params)
{
	Load();
	FPiece* Piece = GPieces.Find(Params.Piece);
	if (!Piece)
	{
		return false;
	}
	const FVector2D* Clip = Params.Clip.IsEmpty() ? nullptr : Piece->Clips.Find(Params.Clip);
	FActive Active;
	Active.Params = Params;
	Active.Params.Speed = FMath::Max(0.05f, Params.Speed);
	Active.Piece = Piece;
	Active.Clip = Clip ? *Clip : FVector2D(0.0, Piece->Duration);
	const FVector2D* ClipAnchor = Piece->Anchors.Find(Params.Clip);
	Active.AnchorPoint = ClipAnchor ? *ClipAnchor : Piece->Anchor;
	// Frames flip on the same instants as the 3D scene's anime steps: a piece started between two steps begins at the last
	// step boundary (its first frame shows at once, the rest follow the world), so the UI / VFX never beat against the scene.
	double Phase = 0.0;
	if (APlayerController* PC = GController.Get())
	{
		if (const UWorld* World = PC->GetWorld())
		{
			if (ABIBAnimeStyleVolume::GetVisualStep(World) >= 0)
			{
				Phase = FMath::Frac((World->GetRealTimeSeconds() + FMath::Max(0.f, Params.Delay)) * static_cast<double>(GFps)) / static_cast<double>(GFps);
			}
		}
	}
	Active.Started = FPlatformTime::Seconds() + FMath::Max(0.f, Params.Delay) - Phase;
	if (Active.Piece && Active.Piece->Pages > 0)
	{
		Prefetch(*Active.Piece, 0);
		Prefetch(*Active.Piece, 1);
	}
	Active.bFollows = Params.Follow.IsValid();
	if (!Params.Tag.IsEmpty())
	{
		Stop(Params.Tag);
	}
	GActive.Add(MoveTemp(Active));
	GPlayed.Add(Params.Piece);
	return true;
}

bool BIBMotion::Play(const FString& Piece, const FString& Clip, float Speed, const FString& Tag)
{
	FPlay Params;
	Params.Piece = Piece;
	Params.Clip = Clip;
	Params.Speed = Speed;
	Params.Tag = Tag;
	return Play(Params);
}

void BIBMotion::Stop(const FString& Tag)
{
	GActive.RemoveAll([&Tag](const FActive& Play) { return Play.Params.Tag == Tag; });
}

bool BIBMotion::IsPlaying(const FString& Tag)
{
	const double Now = FPlatformTime::Seconds();
	return GActive.ContainsByPredicate([&Tag, Now](const FActive& Play)
	{
		return Play.Params.Tag == Tag && (Play.Params.bLoop || (Now - Play.Started) * Play.Params.Speed <= Play.Clip.Y - Play.Clip.X);
	});
}

void BIBMotion::StopAll()
{
	GActive.Reset();
}

void BIBMotion::StopPrefix(const FString& Prefix)
{
	GActive.RemoveAll([&Prefix](const FActive& Play) { return Play.Params.Tag.StartsWith(Prefix); });
}

void BIBMotion::SetController(APlayerController* Controller)
{
	GController = Controller;
}

FString BIBMotion::AbilityPiece(const FString& Who, const FString& SlotKeyIn)
{
	Load();
	const FString* Found = GAbility.Find(Who + TEXT("|") + SlotKeyIn);
	return Found ? *Found : FString();
}

FString BIBMotion::FindFx(const FString& Slug)
{
	Load();
	static TMap<FString, FString> Cache;
	if (const FString* Hit = Cache.Find(Slug))
	{
		return *Hit;
	}
	FString Found;
	for (const auto& Pair : GPieces)
	{
		if (Pair.Key.StartsWith(TEXT("fx")) && Pair.Key.EndsWith(TEXT("-") + Slug))
		{
			Found = Pair.Key;
			break;
		}
	}
	Cache.Add(Slug, Found);
	return Found;
}

FString BIBMotion::PieceMode(const FString& PieceId)
{
	Load();
	const FPiece* Piece = GPieces.Find(PieceId);
	return Piece ? Piece->Mode : FString();
}

bool BIBMotion::HasPiece(const FString& Piece)
{
	Load();
	return GPieces.Contains(Piece);
}

int32 BIBMotion::PieceCount()
{
	Load();
	return GPieces.Num();
}

const TSet<FString>& BIBMotion::PlayedPieces()
{
	return GPlayed;
}

FVector2D BIBMotion::CursorPosition()
{
	return GCursor;
}

TSharedRef<SWidget> BIBMotion::MakeLayer()
{
	Load();
	TSharedRef<SMotionLayer> Layer = SNew(SMotionLayer);
	Layer->SetVisibility(EVisibility::HitTestInvisible);
	return Layer;
}

bool BIBMotion::PaintFrame(const FString& PieceId, const FString& ClipName, double Seconds, bool bLoop, const FGeometry& Geometry, FSlateWindowElementList& Out,
	int32 Layer, const FVector2D& Centre, float PixelScale, float Opacity)
{
	Load();
	FPiece* Piece = GPieces.Find(PieceId);
	if (!Piece)
	{
		return false;
	}
	const FVector2D* Clip = Piece->Clips.Find(ClipName);
	const FVector2D Range = Clip ? *Clip : FVector2D(0.0, Piece->Duration);
	const double Length = FMath::Max(0.01, Range.Y - Range.X);
	const double Local = bLoop ? FMath::Fmod(FMath::Max(0.0, Seconds), Length) : FMath::Clamp(Seconds, 0.0, Length);
	UTexture2D* Texture = nullptr;
	FBox2f UV;
	if (!FrameAt(*Piece, Range.X + Local, Texture, UV))
	{
		return false;
	}
	const FVector2D Pos = Centre + (Piece->Box.Min - Piece->Anchor) * PixelScale;
	DrawCell(Out, Layer, Geometry, Texture, UV, Pos, (Piece->Box.Max - Piece->Box.Min) * PixelScale, Opacity);
	GPlayed.Add(PieceId);
	return true;
}
