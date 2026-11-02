// Boldinbeauty demo — the save system's format. Turns a whole profile (currencies, owned operators with their levels,
// upgrade stars and Angel-heart gates, equipment, squad, banner pity, the 3-day banner rotation, the tutorial step, stages
// cleared, quests, mail, season pass, Outer World, arcade, tower, arena, settings...) into one JSON document and back.
// Engine-independent: the Unreal layer (UBIBGameInstance) only reads and writes the text, locally and to the account
// server.
//
// What is NOT in a save: the account (name, id, admin flag) because the server alone decides that, the unlimited-currency
// flag, and the random generator state. Operators and banners are written by name, so reordering the roster or adding an
// operator never breaks an old save. Loading is all-or-nothing and sanitises every number, because a save file is a text
// file on the player's disk.
#pragma once

#include <cstdint>
#include <string>

#include "BIBCore.h"

namespace BIB
{
	namespace Save
	{
		constexpr int FormatVersion = 1;

		struct FSaveInfo
		{
			bool bValid = false;
			int Version = 0;
			int64_t SavedAtUnix = 0;
			std::string UserId; // the account the save belongs to; empty for a guest or developer save
		};

		// The whole profile as JSON text. SavedAtUnix is stamped into the file and used to pick the newer of a local and a
		// cloud save.
		std::string Serialize(const FProfile& Profile, int64_t SavedAtUnix);

		// Reads only the header (cheap; used to compare saves and to refuse another account's file).
		FSaveInfo Peek(const std::string& Text);

		// Replaces the progress in Profile. On failure the profile is left untouched and Error says why. The profile's
		// account, clock and random generator are kept.
		bool Deserialize(const std::string& Text, FProfile& Profile, std::string& Error);

		const char* TutorialStepName(int Step);
		int TutorialStepFromName(const std::string& Name, int Fallback);
	}
}
