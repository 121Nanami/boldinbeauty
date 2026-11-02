#include "BIBGame/BIBVisuals.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

UMaterialInterface* BIBVisuals::TintBase(UMaterialInterface* Fallback)
{
	// Looked up again while missing, so a material created by the asset build is picked up without a restart.
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (!Cached.IsValid())
	{
		Cached = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BIB/Materials/M_BIBTint.M_BIBTint"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	return Cached.IsValid() ? Cached.Get() : Fallback;
}

UMaterialInstanceDynamic* BIBVisuals::MakeTint(UMaterialInterface* Fallback, UObject* Outer)
{
	UMaterialInterface* Base = TintBase(Fallback);
	return Base ? UMaterialInstanceDynamic::Create(Base, Outer) : nullptr;
}

FName BIBVisuals::ColorParameter()
{
	return FName(TEXT("Color"));
}
