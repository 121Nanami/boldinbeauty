// The game's side of the account service (Demo/BoldInBeautyDemo/Server): sign up with a username and e-mail, log in with an
// e-mail, both confirmed by a 6-digit code the server e-mails. After that the player is signed in; the profile (name, e-mail,
// user id, admin flag) comes from the server's reply. The session token is kept in Saved/BIB/session.json so the next launch
// signs in again without a code. Everything here is asynchronous HTTP; the sign-in screen (BIBUI) only reads this state.
//
// The server address comes from -BIBApi=<url> on the command line, else [BoldInBeauty] ApiBaseUrl in Config/DefaultGame.ini.
// No secret is ever stored in the game: the database and e-mail credentials live on the server.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BIBRules/BIBCore.h"
#include "BIBRules/BIBJson.h"
#include "BIBAccount.generated.h"

enum class EBIBAuthStage : uint8
{
	SignedOut,     // choose: log in / create an account / play as a guest
	AwaitingCode,  // a code was e-mailed; the player types it
	Busy,          // waiting for the server
	SignedIn,
	Guest
};

enum class EBIBAuthMode : uint8
{
	LogIn,
	SignUp
};

DECLARE_MULTICAST_DELEGATE(FBIBAccountEvent);

UCLASS()
class BOLDINBEAUTYDEMO_API UBIBAccountSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// ------------------------------------------------------------- state for the UI
	EBIBAuthStage GetStage() const { return Stage; }
	EBIBAuthMode GetMode() const { return Mode; }
	void SetMode(EBIBAuthMode NewMode);
	const FString& GetStatusMessage() const { return Message; } // not GetMessage: that is a Windows macro
	bool IsMessageError() const { return bMessageIsError; }
	const FString& GetPendingEmail() const { return PendingEmail; }
	const FString& GetApiBaseUrl() const { return ApiBaseUrl; }
	const BIB::FAccountInfo& GetAccount() const { return Account; }
	/** Signed in from the saved session while the server could not be reached (no admin rights, no cloud save until it can). */
	bool IsOffline() const { return bOffline; }
	bool HasToken() const { return !Token.IsEmpty(); }

	// ------------------------------------------------------------------ actions
	void RestoreSession();                                        // launch: use the saved session, if any
	void RequestSignup(const FString& Username, const FString& Email);
	void RequestLogin(const FString& Email);
	void SubmitCode(const FString& Code);
	void ResendCode(const FString& UsernameForSignup);
	void BackToStart();                                           // from the code step
	void ContinueAsGuest();
	void SignOut();

	/** JSON request to the account service. Done(Status, Reply, bNetworkError) runs on the game thread. */
	void Request(const FString& Verb, const FString& Path, const std::string& Body, bool bWithToken,
		TFunction<void(int32, const BIB::Json::FValue&, bool)> Done);

	// ---- the same flow for Python / Blueprint tests (Tools/verify_accounts.py)
	UFUNCTION(BlueprintCallable, Category = "BIB|Account") void BeginSignup(const FString& Username, const FString& Email) { RequestSignup(Username, Email); }
	UFUNCTION(BlueprintCallable, Category = "BIB|Account") void BeginLogin(const FString& Email) { RequestLogin(Email); }
	UFUNCTION(BlueprintCallable, Category = "BIB|Account") void EnterCode(const FString& Code) { SubmitCode(Code); }
	UFUNCTION(BlueprintCallable, Category = "BIB|Account") void BeginGuest() { ContinueAsGuest(); }
	UFUNCTION(BlueprintPure, Category = "BIB|Account") FString GetStageName() const;
	UFUNCTION(BlueprintPure, Category = "BIB|Account") FString GetMessageText() const { return Message; }
	UFUNCTION(BlueprintPure, Category = "BIB|Account") FString GetUserId() const;
	UFUNCTION(BlueprintPure, Category = "BIB|Account") FString GetUsername() const;
	UFUNCTION(BlueprintPure, Category = "BIB|Account") FString GetEmail() const;
	UFUNCTION(BlueprintPure, Category = "BIB|Account") bool IsAdmin() const { return Account.bAdmin; }

	// A session is ready (signed in or guest): the game instance loads the save. Signed out: it saves and resets.
	FBIBAccountEvent OnSessionReady;
	FBIBAccountEvent OnSignedOut;

	static FString FriendlyError(const FString& Code, const BIB::Json::FValue& Reply, int32 Status, bool bNetworkError);

private:
	void SetMessage(const FString& Text, bool bError);
	void SendCodeRequest(const FString& Path, const BIB::Json::FValue& Body, const FString& Email, bool bSignUp);
	void CompleteSignIn(const BIB::Json::FValue& AccountJson, const FString& NewToken, bool bOfflineMode);
	FString SessionPath() const;
	void WriteSession() const;
	void DeleteSession() const;

	EBIBAuthStage Stage = EBIBAuthStage::SignedOut;
	EBIBAuthMode Mode = EBIBAuthMode::LogIn;
	FString Message;
	bool bMessageIsError = false;
	FString PendingEmail;
	FString PendingUsername;
	FString ApiBaseUrl;
	FString Token;
	BIB::FAccountInfo Account;
	bool bOffline = false;
};
