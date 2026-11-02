// Prints every gameplay tag the operator kits, statuses, buffs and cues use (one per line). Tools/gen_tags.py turns
// the list into Config/DefaultGameplayTags.ini. Build & run via Tools/gen_tags.py.
#include "../Source/BoldInBeautyDemo/BIBRules/BIBKitData.h"

#include <cstdio>

int main()
{
	for (const std::string& Tag : BIB::Kits::AllGameplayTags())
	{
		std::printf("%s\n", Tag.c_str());
	}
	return 0;
}
