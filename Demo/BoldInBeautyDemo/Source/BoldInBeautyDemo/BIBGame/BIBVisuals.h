// Shared tint material lookup. Blocks, enemies, the team, doors and the story NPC are coloured through a
// vector parameter named "Color". The project material /Game/BIB/Materials/M_BIBTint (built by Tools/build_assets.py)
// is used when it exists; otherwise the engine's BasicShapeMaterial.
#pragma once

#include "CoreMinimal.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;

namespace BIBVisuals
{
	BOLDINBEAUTYDEMO_API UMaterialInterface* TintBase(UMaterialInterface* Fallback);
	BOLDINBEAUTYDEMO_API UMaterialInstanceDynamic* MakeTint(UMaterialInterface* Fallback, UObject* Outer);
	BOLDINBEAUTYDEMO_API FName ColorParameter();
}
