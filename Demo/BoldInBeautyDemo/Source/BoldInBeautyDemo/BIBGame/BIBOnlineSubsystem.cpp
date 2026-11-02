#include "BIBGame/BIBOnlineSubsystem.h"

#include "BIBGame/BIBAccount.h"
#include "BIBGame/BIBGameInstance.h"
#include "BIBRules/BIBPagesExtra.h"

DEFINE_LOG_CATEGORY_STATIC(LogBIBOnline, Log, All);

namespace BIBOnlineInternal
{
	FString ToF(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}
}

UBIBGameInstance* UBIBOnlineSubsystem::GameInstance() const
{
	return Cast<UBIBGameInstance>(GetGameInstance());
}

bool UBIBOnlineSubsystem::IsOnline() const
{
	const UBIBGameInstance* Instance = GameInstance();
	return Instance && Instance->UiState.Net.bOnline;
}

int32 UBIBOnlineSubsystem::GetBusy() const
{
	const UBIBGameInstance* Instance = GameInstance();
	return Instance ? Instance->UiState.Net.Busy : 0;
}

bool UBIBOnlineSubsystem::ConsumeOpenNotices()
{
	const bool bValue = bOpenNotices;
	bOpenNotices = false;
	return bValue;
}

void UBIBOnlineSubsystem::Send(const std::vector<BIB::Online::FCall>& Calls)
{
	for (const BIB::Online::FCall& Call : Calls)
	{
		Send(Call);
	}
}

void UBIBOnlineSubsystem::Send(const BIB::Online::FCall& Call)
{
	UBIBGameInstance* Instance = GameInstance();
	UBIBAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBIBAccountSubsystem>() : nullptr;
	if (!Instance || !Accounts || !Instance->UiState.Net.bOnline)
	{
		return;
	}
	++Instance->UiState.Net.Busy;
	TWeakObjectPtr<UBIBOnlineSubsystem> WeakThis(this);
	Accounts->Request(BIBOnlineInternal::ToF(Call.Verb), BIBOnlineInternal::ToF(Call.Path), Call.Body, true,
		[WeakThis, Call](int32 Status, const BIB::Json::FValue& Reply, bool bNetworkError)
		{
			UBIBOnlineSubsystem* Self = WeakThis.Get();
			UBIBGameInstance* Owner = Self ? Self->GameInstance() : nullptr;
			if (!Owner || !Owner->IsProfileReady())
			{
				return;
			}
			const BIB::Online::FReplyResult Result = BIB::Online::ApplyReply(Owner->UiState.Net, Owner->GetProfile(), Call, Status, Reply, bNetworkError);
			if (!Result.Message.empty() && (!Call.bQuiet || !Result.bOk))
			{
				if (!(Call.bQuiet && bNetworkError))
				{
					Owner->UiState.Status = Result.Message;
				}
			}
			if (!Result.bOk)
			{
				UE_LOG(LogBIBOnline, Warning, TEXT("%s failed (%d): %s"), *BIBOnlineInternal::ToF(BIB::Online::CallName(Call.Kind)), Status,
					*BIBOnlineInternal::ToF(Result.Message));
			}
			if (Result.bOpenNotices)
			{
				Self->bOpenNotices = true;
			}
			Owner->SaveNow();   // a claim, an import or a purchase changed the profile
			Self->Send(Result.FollowUps);
		});
}

void UBIBOnlineSubsystem::Tick1s()
{
	UBIBGameInstance* Instance = GameInstance();
	UBIBAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBIBAccountSubsystem>() : nullptr;
	if (!Instance || !Accounts)
	{
		return;
	}
	BIB::Online::FCache& Net = Instance->UiState.Net;
	const bool bWasOnline = Net.bOnline;
	const BIB::FProfile& Profile = Instance->GetProfile();
	Net.bOnline = Instance->IsProfileReady() && !Instance->IsDevProfile() && Profile.Account.bSignedIn && !Accounts->IsOffline() && Accounts->HasToken();
	Net.bAdmin = Net.bOnline && Profile.Account.bAdmin;
	if (!Net.bOnline)
	{
		if (bWasOnline)
		{
			// Signed out: forget everything the server said.
			Net = BIB::Online::FCache();
		}
		bSessionStarted = false;
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (!bSessionStarted)
	{
		bSessionStarted = true;
		Net.bLoginNoticesShown = false;
		NextRefresh = Now + 120.0;
		NextProfilePush = Now + 300.0;
		const std::vector<BIB::Online::FCall> Calls = BIB::Pages::Extra::SessionCalls(Profile);
		LastProfileBody = Calls.empty() ? std::string() : Calls[0].Body;
		LastDefenceBody = Calls.size() > 1 ? Calls[1].Body : std::string();
		Send(Calls);
		UE_LOG(LogBIBOnline, Display, TEXT("Online session started: %d requests."), static_cast<int32>(Calls.size()));
		return;
	}
	if (Now >= NextRefresh)
	{
		NextRefresh = Now + 120.0;
		Send(BIB::Online::FetchMail());
		Send(BIB::Online::FetchNotices());
		if (Net.bInGang || Instance->GetProfile().Meta.Gang.PendingActivity > 0)
		{
			Send(BIB::Online::MyGang());
		}
	}
	if (Now >= NextProfilePush)
	{
		NextProfilePush = Now + 300.0;
		const BIB::Online::FCall Push = BIB::Online::PushProfile(Profile);
		if (Push.Body != LastProfileBody)
		{
			LastProfileBody = Push.Body;
			Send(Push);
		}
		const BIB::Online::FCall Defence = BIB::Online::PutDefence(Profile);
		if (Defence.Body != LastDefenceBody)
		{
			LastDefenceBody = Defence.Body;
			Send(Defence);
		}
	}
}
