// Logical game inputs. Each maps to an Enhanced Input action asset (set on the player controller Blueprint),
// falling back to a default keyboard/mouse key when no asset is assigned.
#pragma once

#include "CoreMinimal.h"
#include "BIBInput.generated.h"

UENUM(BlueprintType)
enum class EBIBInput : uint8
{
	MoveForward,
	MoveBack,
	MoveLeft,
	MoveRight,
	Jump,
	Run,
	Attack,
	Block,
	Parry,
	Move1,
	Move2,
	Move3,
	Special,
	UltimateE,
	UltimateR,
	SwitchOperator,
	Talk,
	Pause,
	Count UMETA(Hidden)
};
