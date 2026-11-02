// The BoldInBeauty brand fonts for the Slate UI (2026-10-09): League Gothic (the voice: headings, names, banners), JetBrains Mono
// (HUD labels and system lines), Archivo Black (big numerals). Files in Content/BIBFonts (SIL OFL, licences beside them), staged
// with the game (Config/DefaultGame.ini). A missing file falls back to the engine font.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

namespace BIBFonts
{
	inline FSlateFontInfo FromFile(const TCHAR* File, int32 Size, const TCHAR* FallbackStyle)
	{
		static TMap<FString, TSharedPtr<const FCompositeFont>> Cache;
		const FString Path = FPaths::ProjectContentDir() / TEXT("BIBFonts") / File;
		TSharedPtr<const FCompositeFont>* Found = Cache.Find(Path);
		if (!Found)
		{
			TSharedPtr<const FCompositeFont> Font;
			if (FPaths::FileExists(Path))
			{
				Font = MakeShared<FStandaloneCompositeFont>(NAME_None, Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			}
			Found = &Cache.Add(Path, Font);
		}
		return Found->IsValid() ? FSlateFontInfo(*Found, static_cast<float>(Size)) : FCoreStyle::GetDefaultFontStyle(FallbackStyle, Size);
	}
	inline FSlateFontInfo Display(int32 Size) { return FromFile(TEXT("LeagueGothic.ttf"), Size, TEXT("Bold")); }
	inline FSlateFontInfo Mono(int32 Size) { return FromFile(TEXT("JetBrainsMono.ttf"), Size, TEXT("Regular")); }
	inline FSlateFontInfo Numeral(int32 Size) { return FromFile(TEXT("ArchivoBlack.ttf"), Size, TEXT("Bold")); }
}
