#include "BIBGame/BIBAccount.h"

#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogBIBAccount, Log, All);

namespace BIBAccountInternal
{
	FString ToF(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	std::string ToStd(const FString& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text));
	}

	bool LooksLikeEmail(const FString& Email)
	{
		int32 At = INDEX_NONE;
		return Email.Len() >= 6 && Email.FindChar(TEXT('@'), At) && At > 0 && Email.Mid(At + 1).Contains(TEXT(".")) && !Email.Contains(TEXT(" "));
	}
}

void UBIBAccountSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Url;
	if (!FParse::Value(FCommandLine::Get(), TEXT("BIBApi="), Url) || Url.IsEmpty())
	{
		Url.Reset();
		if (GConfig)
		{
			GConfig->GetString(TEXT("BoldInBeauty"), TEXT("ApiBaseUrl"), Url, GGameIni);
		}
	}
	Url.TrimStartAndEndInline();
	while (Url.EndsWith(TEXT("/")))
	{
		Url.LeftChopInline(1);
	}
	if (Url.IsEmpty())
	{
		Url = TEXT("http://127.0.0.1:8080");
	}
	ApiBaseUrl = Url;
	if (ApiBaseUrl.StartsWith(TEXT("http://")) && !ApiBaseUrl.Contains(TEXT("127.0.0.1")) && !ApiBaseUrl.Contains(TEXT("localhost")))
	{
		UE_LOG(LogBIBAccount, Warning, TEXT("The account server address %s is not HTTPS: codes and sessions would travel unencrypted. Use https:// for anything but local testing."), *ApiBaseUrl);
	}
	UE_LOG(LogBIBAccount, Display, TEXT("Account server: %s"), *ApiBaseUrl);
}

FString UBIBAccountSubsystem::GetStageName() const
{
	switch (Stage)
	{
	case EBIBAuthStage::SignedOut: return TEXT("SignedOut");
	case EBIBAuthStage::AwaitingCode: return TEXT("AwaitingCode");
	case EBIBAuthStage::Busy: return TEXT("Busy");
	case EBIBAuthStage::SignedIn: return TEXT("SignedIn");
	case EBIBAuthStage::Guest: return TEXT("Guest");
	}
	return TEXT("?");
}

FString UBIBAccountSubsystem::GetUserId() const
{
	return BIBAccountInternal::ToF(Account.UserId);
}

FString UBIBAccountSubsystem::GetUsername() const
{
	return BIBAccountInternal::ToF(Account.Username);
}

FString UBIBAccountSubsystem::GetEmail() const
{
	return BIBAccountInternal::ToF(Account.Email);
}

void UBIBAccountSubsystem::SetMode(EBIBAuthMode NewMode)
{
	Mode = NewMode;
	SetMessage(FString(), false);
}

void UBIBAccountSubsystem::SetMessage(const FString& Text, bool bError)
{
	Message = Text;
	bMessageIsError = bError;
}

// ============================================================================ HTTP
void UBIBAccountSubsystem::Request(const FString& Verb, const FString& Path, const std::string& Body, bool bWithToken,
	TFunction<void(int32, const BIB::Json::FValue&, bool)> Done)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Http = FHttpModule::Get().CreateRequest();
	Http->SetURL(ApiBaseUrl + Path);
	Http->SetVerb(Verb);
	Http->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Http->SetHeader(TEXT("Accept"), TEXT("application/json"));
	if (bWithToken && !Token.IsEmpty())
	{
		Http->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
	}
	if (!Body.empty())
	{
		Http->SetContentAsString(BIBAccountInternal::ToF(Body));
	}
	Http->SetTimeout(15.f);
	TWeakObjectPtr<UBIBAccountSubsystem> WeakThis(this);
	Http->OnProcessRequestComplete().BindLambda([WeakThis, Done](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		if (!WeakThis.IsValid() || !Done)
		{
			return;
		}
		BIB::Json::FValue Reply;
		int32 Status = 0;
		const bool bNetworkError = !bConnected || !Response.IsValid();
		if (!bNetworkError)
		{
			Status = Response->GetResponseCode();
			std::string Error;
			BIB::Json::Parse(BIBAccountInternal::ToStd(Response->GetContentAsString()), Reply, Error);
		}
		Done(Status, Reply, bNetworkError);
	});
	if (!Http->ProcessRequest())
	{
		Done(0, BIB::Json::FValue(), true);
	}
}

FString UBIBAccountSubsystem::FriendlyError(const FString& Code, const BIB::Json::FValue& Reply, int32 Status, bool bNetworkError)
{
	if (bNetworkError)
	{
		return TEXT("Cannot reach the account server. Check your connection, or play as a guest.");
	}
	if (Code == TEXT("invalid_email")) return TEXT("That does not look like an e-mail address.");
	if (Code == TEXT("invalid_username")) return TEXT("Usernames are 3-20 letters, digits or _, and start with a letter.");
	if (Code == TEXT("reserved_username")) return TEXT("That username is reserved. Please pick another.");
	if (Code == TEXT("email_taken")) return TEXT("An account with this e-mail already exists. Use Log in instead.");
	if (Code == TEXT("username_taken")) return TEXT("That username is already taken.");
	if (Code == TEXT("too_soon")) return TEXT("Please wait a minute before asking for another code.");
	if (Code == TEXT("rate_limited")) return TEXT("Too many tries. Please wait a minute and try again.");
	if (Code == TEXT("too_many_attempts")) return TEXT("Too many wrong codes. Ask for a new code.");
	if (Code == TEXT("mail_failed")) return TEXT("The e-mail could not be sent. Please try again later.");
	if (Code == TEXT("invalid_code"))
	{
		const BIB::Json::FValue* Left = Reply.Find("attemptsLeft");
		if (Left && Left->IsNumber())
		{
			return Left->AsInt() > 0 ? FString::Printf(TEXT("Wrong or expired code. %d tries left."), Left->AsInt()) : FString(TEXT("Wrong code. Ask for a new code."));
		}
		return TEXT("Wrong or expired code.");
	}
	return FString::Printf(TEXT("Something went wrong (%d). Please try again."), Status);
}

// ============================================================================ session file
FString UBIBAccountSubsystem::SessionPath() const
{
	return FPaths::ProjectSavedDir() / TEXT("BIB") / TEXT("session.json");
}

void UBIBAccountSubsystem::WriteSession() const
{
	BIB::Json::FValue Doc = BIB::Json::FValue::MakeObject();
	Doc.Set("api", BIB::Json::FValue::MakeString(BIBAccountInternal::ToStd(ApiBaseUrl)));
	Doc.Set("token", BIB::Json::FValue::MakeString(BIBAccountInternal::ToStd(Token)));
	Doc.Set("userId", BIB::Json::FValue::MakeString(Account.UserId));
	Doc.Set("username", BIB::Json::FValue::MakeString(Account.Username));
	Doc.Set("email", BIB::Json::FValue::MakeString(Account.Email));
	const std::string Text = BIB::Json::Write(Doc);
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(Text.data()), static_cast<int32>(Text.size()));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SessionPath()), true);
	FFileHelper::SaveArrayToFile(Bytes, *SessionPath());
}

void UBIBAccountSubsystem::DeleteSession() const
{
	IFileManager::Get().Delete(*SessionPath(), false, true, true);
}

// ============================================================================ flow
void UBIBAccountSubsystem::RestoreSession()
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *SessionPath()) || Bytes.Num() == 0)
	{
		Stage = EBIBAuthStage::SignedOut;
		return;
	}
	BIB::Json::FValue Doc;
	std::string Error;
	const std::string Text(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<size_t>(Bytes.Num()));
	if (!BIB::Json::Parse(Text, Doc, Error) || Doc.GetString("token").empty() || Doc.GetString("api") != BIBAccountInternal::ToStd(ApiBaseUrl))
	{
		DeleteSession(); // unreadable, or it belongs to another server
		Stage = EBIBAuthStage::SignedOut;
		return;
	}
	Token = BIBAccountInternal::ToF(Doc.GetString("token"));
	BIB::FAccountInfo Cached;
	Cached.UserId = Doc.GetString("userId");
	Cached.Username = Doc.GetString("username");
	Cached.Email = Doc.GetString("email");
	Cached.bSignedIn = true;
	Stage = EBIBAuthStage::Busy;
	SetMessage(TEXT("Checking your saved sign-in..."), false);
	TWeakObjectPtr<UBIBAccountSubsystem> WeakThis(this);
	Request(TEXT("GET"), TEXT("/v1/me"), std::string(), true, [WeakThis, Cached](int32 Status, const BIB::Json::FValue& Reply, bool bNetworkError)
	{
		UBIBAccountSubsystem* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (!bNetworkError && Status == 200 && Reply.Find("account"))
		{
			Self->CompleteSignIn(*Reply.Find("account"), Self->Token, false);
			return;
		}
		if (bNetworkError && !Cached.UserId.empty())
		{
			// Offline: keep playing as the saved account, but never as an admin (only the server can say so), and no cloud save.
			BIB::Json::FValue Offline = BIB::Json::FValue::MakeObject();
			Offline.Set("userId", BIB::Json::FValue::MakeString(Cached.UserId));
			Offline.Set("username", BIB::Json::FValue::MakeString(Cached.Username));
			Offline.Set("email", BIB::Json::FValue::MakeString(Cached.Email));
			Offline.Set("isAdmin", BIB::Json::FValue::MakeBool(false));
			Self->CompleteSignIn(Offline, Self->Token, true);
			return;
		}
		if (!bNetworkError)
		{
			Self->DeleteSession(); // expired or revoked
		}
		Self->Token.Reset();
		Self->Stage = EBIBAuthStage::SignedOut;
		Self->SetMessage(bNetworkError ? FString(TEXT("Cannot reach the account server.")) : FString(TEXT("Your sign-in expired. Please log in again.")), true);
	});
}

void UBIBAccountSubsystem::SendCodeRequest(const FString& Path, const BIB::Json::FValue& Body, const FString& Email, bool bSignUp)
{
	Stage = EBIBAuthStage::Busy;
	SetMessage(TEXT("Sending the code..."), false);
	TWeakObjectPtr<UBIBAccountSubsystem> WeakThis(this);
	Request(TEXT("POST"), Path, BIB::Json::Write(Body), false, [WeakThis, Email, bSignUp](int32 Status, const BIB::Json::FValue& Reply, bool bNetworkError)
	{
		UBIBAccountSubsystem* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (!bNetworkError && Status == 200 && Reply.GetBool("ok"))
		{
			Self->PendingEmail = Email;
			Self->Stage = EBIBAuthStage::AwaitingCode;
			Self->SetMessage(bSignUp ? FString::Printf(TEXT("We sent a 6-digit code to %s. Enter it to finish creating your account."), *Email)
									 : FString::Printf(TEXT("If %s has an account, we sent it a 6-digit code."), *Email), false);
			return;
		}
		Self->Stage = EBIBAuthStage::SignedOut;
		Self->SetMessage(FriendlyError(BIBAccountInternal::ToF(Reply.GetString("error")), Reply, Status, bNetworkError), true);
	});
}

void UBIBAccountSubsystem::RequestSignup(const FString& Username, const FString& Email)
{
	const FString CleanEmail = Email.TrimStartAndEnd();
	const FString CleanName = Username.TrimStartAndEnd();
	Mode = EBIBAuthMode::SignUp;
	if (CleanName.IsEmpty())
	{
		SetMessage(TEXT("Choose a username."), true);
		return;
	}
	if (!BIBAccountInternal::LooksLikeEmail(CleanEmail))
	{
		SetMessage(TEXT("Enter a valid e-mail address."), true);
		return;
	}
	PendingUsername = CleanName;
	BIB::Json::FValue Body = BIB::Json::FValue::MakeObject();
	Body.Set("username", BIB::Json::FValue::MakeString(BIBAccountInternal::ToStd(CleanName)));
	Body.Set("email", BIB::Json::FValue::MakeString(BIBAccountInternal::ToStd(CleanEmail)));
	SendCodeRequest(TEXT("/v1/auth/signup"), Body, CleanEmail, true);
}

void UBIBAccountSubsystem::RequestLogin(const FString& Email)
{
	const FString CleanEmail = Email.TrimStartAndEnd();
	Mode = EBIBAuthMode::LogIn;
	if (!BIBAccountInternal::LooksLikeEmail(CleanEmail))
	{
		SetMessage(TEXT("Enter a valid e-mail address."), true);
		return;
	}
	BIB::Json::FValue Body = BIB::Json::FValue::MakeObject();
	Body.Set("email", BIB::Json::FValue::MakeString(BIBAccountInternal::ToStd(CleanEmail)));
	SendCodeRequest(TEXT("/v1/auth/login"), Body, CleanEmail, false);
}

void UBIBAccountSubsystem::ResendCode(const FString& UsernameForSignup)
{
	if (Mode == EBIBAuthMode::SignUp)
	{
		RequestSignup(UsernameForSignup.IsEmpty() ? PendingUsername : UsernameForSignup, PendingEmail);
	}
	else
	{
		RequestLogin(PendingEmail);
	}
}

void UBIBAccountSubsystem::SubmitCode(const FString& Code)
{
	FString Digits;
	for (const TCHAR Character : Code)
	{
		if (FChar::IsDigit(Character))
		{
			Digits.AppendChar(Character);
		}
	}
	if (Digits.Len() != 6)
	{
		SetMessage(TEXT("The code has 6 digits."), true);
		return;
	}
	Stage = EBIBAuthStage::Busy;
	SetMessage(TEXT("Checking the code..."), false);
	BIB::Json::FValue Body = BIB::Json::FValue::MakeObject();
	Body.Set("email", BIB::Json::FValue::MakeString(BIBAccountInternal::ToStd(PendingEmail)));
	Body.Set("code", BIB::Json::FValue::MakeString(BIBAccountInternal::ToStd(Digits)));
	TWeakObjectPtr<UBIBAccountSubsystem> WeakThis(this);
	Request(TEXT("POST"), TEXT("/v1/auth/verify"), BIB::Json::Write(Body), false, [WeakThis](int32 Status, const BIB::Json::FValue& Reply, bool bNetworkError)
	{
		UBIBAccountSubsystem* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		const BIB::Json::FValue* AccountJson = Reply.Find("account");
		if (!bNetworkError && Status == 200 && AccountJson && !Reply.GetString("token").empty())
		{
			Self->CompleteSignIn(*AccountJson, BIBAccountInternal::ToF(Reply.GetString("token")), false);
			return;
		}
		const FString ErrorCode = BIBAccountInternal::ToF(Reply.GetString("error"));
		// A burnt code (too many tries, or the last try was wrong) means starting over; otherwise the player may try again.
		const BIB::Json::FValue* Left = Reply.Find("attemptsLeft");
		const bool bBurnt = ErrorCode == TEXT("too_many_attempts") || (Left && Left->IsNumber() && Left->AsInt() <= 0)
			|| ErrorCode == TEXT("username_taken") || ErrorCode == TEXT("email_taken");
		Self->Stage = (bBurnt && !bNetworkError) ? EBIBAuthStage::SignedOut : EBIBAuthStage::AwaitingCode;
		Self->SetMessage(FriendlyError(ErrorCode, Reply, Status, bNetworkError), true);
	});
}

void UBIBAccountSubsystem::BackToStart()
{
	Stage = EBIBAuthStage::SignedOut;
	SetMessage(FString(), false);
}

void UBIBAccountSubsystem::CompleteSignIn(const BIB::Json::FValue& AccountJson, const FString& NewToken, bool bOfflineMode)
{
	Token = NewToken;
	bOffline = bOfflineMode;
	Account = BIB::FAccountInfo();
	Account.UserId = AccountJson.GetString("userId");
	Account.Username = AccountJson.GetString("username");
	Account.Email = AccountJson.GetString("email");
	Account.bSignedIn = true;
	Account.bAdmin = !bOfflineMode && AccountJson.GetBool("isAdmin");
	WriteSession();
	Stage = EBIBAuthStage::SignedIn;
	SetMessage(FString(), false);
	UE_LOG(LogBIBAccount, Display, TEXT("Signed in as %s (user id %s)%s%s"), *BIBAccountInternal::ToF(Account.Username), *BIBAccountInternal::ToF(Account.UserId),
		Account.bAdmin ? TEXT(" [admin]") : TEXT(""), bOfflineMode ? TEXT(" [offline]") : TEXT(""));
	OnSessionReady.Broadcast();
}

void UBIBAccountSubsystem::ContinueAsGuest()
{
	Token.Reset();
	bOffline = false;
	Account = BIB::FAccountInfo();
	Account.Username = "Guest";
	Account.bGuest = true;
	Stage = EBIBAuthStage::Guest;
	SetMessage(FString(), false);
	OnSessionReady.Broadcast();
}

void UBIBAccountSubsystem::SignOut()
{
	if (!Token.IsEmpty())
	{
		Request(TEXT("POST"), TEXT("/v1/auth/logout"), std::string("{}"), true, [](int32, const BIB::Json::FValue&, bool) {});
	}
	DeleteSession();
	Token.Reset();
	bOffline = false;
	Account = BIB::FAccountInfo();
	Stage = EBIBAuthStage::SignedOut;
	Mode = EBIBAuthMode::LogIn;
	SetMessage(TEXT("Signed out."), false);
	OnSignedOut.Broadcast();
}
