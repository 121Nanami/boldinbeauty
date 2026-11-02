// The online features (friends, gangs, admin mail, notices, arena; owner request 2026-10-06) on top of the account client.
// The hub pages build requests (BIBRules/BIBOnline.h FCall); this subsystem sends them with the session token, hands each reply
// to BIB::Online::ApplyReply (which updates the hub's cache and the profile) and sends any follow-up requests. While a session is
// signed in it also refreshes mail and notices every two minutes and pushes the public profile when it changes.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BIBRules/BIBOnline.h"
#include "BIBOnlineSubsystem.generated.h"

class UBIBGameInstance;

UCLASS()
class BOLDINBEAUTYDEMO_API UBIBOnlineSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void Send(const std::vector<BIB::Online::FCall>& Calls);
	void Send(const BIB::Online::FCall& Call);

	/** Called once a second by the game instance: online state, the first requests of a session, periodic refreshes. */
	void Tick1s();

	/** Signed in with a reachable server (guests and the developer profile play offline). */
	UFUNCTION(BlueprintPure, Category = "BIB|Online")
	bool IsOnline() const;

	/** Number of requests in flight (a test hook). */
	UFUNCTION(BlueprintPure, Category = "BIB|Online")
	int32 GetBusy() const;

	/** Set when unread notices arrived at login and the player wants them shown (the hub opens the notice board). */
	bool ConsumeOpenNotices();

private:
	UBIBGameInstance* GameInstance() const;
	bool bSessionStarted = false;
	bool bOpenNotices = false;
	double NextRefresh = 0.0;
	double NextProfilePush = 0.0;
	std::string LastProfileBody;
	std::string LastDefenceBody;
};
