// Gem shop (Evan Q2, Oct 14): data-driven packages (Content/Data/Progression/GemPackages.csv) and a purchase pipeline behind
// IZoogiStoreBackend. Store priority (Evan, Oct 14): Apple App Store FIRST, then Google Play, Steam, Epic; selected by platform.
//
// STATUS (honest): the pipeline (pending -> verified -> granted -> consumed, idempotent by the store transaction id, ledger,
// persistence) is real and tested. NO real purchase has been made: every real backend needs Evan's developer accounts,
// product ids and a receipt-verification server (Docs/Progression/STORE_SETUP.md). FZoogiTestStoreBackend is for
// automation ONLY (it is compiled only WITH_DEV_AUTOMATION_TESTS and refuses to run in shipping); it is not a payment method.
#pragma once

#include "CoreMinimal.h"
#include "ZoogiProgression.h"

struct FZoogiStoreProduct { FString ProductId; FName PackageId; FString Title; FString DisplayPrice; };

/** What a store hands back for one completed transaction. */
struct FZoogiStoreReceipt
{
	FString Store;              // apple | google | steam | epic | test
	FString TransactionId;      // Apple transactionId / Google orderId / Steam orderid / EOS entitlement id  (idempotency key)
	FString ProductId;
	FString Payload;            // Apple: StoreKit 2 signed transaction (JWS) / Google: purchaseToken / Steam: orderid / EOS: receipt
	bool bSandbox = false;
};

enum class EZoogiVerifyResult : uint8 { Valid, Invalid, Pending, Unavailable };

class ZOOGIROLL_API IZoogiStoreBackend
{
public:
	virtual ~IZoogiStoreBackend() = default;
	virtual FString StoreName() const = 0;
	/** False (with the reason) until the platform, its OnlineSubsystem plugin, product ids and the account are configured. */
	virtual bool IsAvailable(FString& OutWhy) const = 0;
	virtual void QueryProducts(const TArray<FString>& ProductIds, TFunction<void(bool, const TArray<FZoogiStoreProduct>&)> Done) = 0;
	virtual void Purchase(const FString& ProductId, TFunction<void(bool, const FZoogiStoreReceipt&, const FString&)> Done) = 0;
	/** Receipt verification. Real stores: server-side (VerifyUrl) is required for trust; see each backend. */
	virtual EZoogiVerifyResult Verify(const FZoogiStoreReceipt& R, FString& OutWhy) = 0;
	/** Consume / finish the transaction with the store (only after the gems are durably granted). */
	virtual void Finalize(const FZoogiStoreReceipt& R) = 0;
	/** Unfinished transactions to resume at startup (crash between pay and grant). */
	virtual void QueryUnfinished(TFunction<void(const TArray<FZoogiStoreReceipt>&)> Done) = 0;
};

/** Apple StoreKit 2 signed-transaction (JWS) checks that don't need Apple's servers: structure, payload fields (bundleId,
 *  productId, transactionId, type Consumable, environment, revocation) and the x5c chain presence. The ES256 signature /
 *  Apple Root CA G3 chain check is delegated to the verification server (App Store Server API) - see STORE_SETUP.md. */
namespace ZoogiAppleJws
{
	struct FPayload { FString TransactionId, OriginalTransactionId, BundleId, ProductId, Type, Environment; bool bRevoked = false; };
	ZOOGIROLL_API bool Decode(const FString& Jws, FPayload& Out, FString& OutWhy);
	ZOOGIROLL_API EZoogiVerifyResult CheckClaims(const FString& Jws, const FZoogiStoreReceipt& R, const FString& ExpectedBundleId, FString& OutWhy);
}

/** Real backend over UE's OnlineSubsystem IOnlinePurchase: Apple (OSS "IOS", StoreKit), Google Play (OSS "GooglePlay"),
 *  Epic (OSS "EOS" ecom). Available only on that platform with the plugin enabled + configured. Not exercised yet (no accounts). */
class ZOOGIROLL_API FZoogiOSSStoreBackend : public IZoogiStoreBackend
{
public:
	FZoogiOSSStoreBackend(const FString& InStore, const FName& InOSS) : Store(InStore), OSS(InOSS) {}
	virtual FString StoreName() const override { return Store; }
	virtual bool IsAvailable(FString& OutWhy) const override;
	virtual void QueryProducts(const TArray<FString>& ProductIds, TFunction<void(bool, const TArray<FZoogiStoreProduct>&)> Done) override;
	virtual void Purchase(const FString& ProductId, TFunction<void(bool, const FZoogiStoreReceipt&, const FString&)> Done) override;
	virtual EZoogiVerifyResult Verify(const FZoogiStoreReceipt& R, FString& OutWhy) override;
	virtual void Finalize(const FZoogiStoreReceipt& R) override;
	virtual void QueryUnfinished(TFunction<void(const TArray<FZoogiStoreReceipt>&)> Done) override;
private:
	FString Store; FName OSS;
};

/** Steam microtransactions need a server (ISteamMicroTxn InitTxn/FinalizeTxn with the publisher Web API key); the client
 *  only shows the overlay authorization. Placeholder until that server exists: always unavailable. */
class ZOOGIROLL_API FZoogiSteamStoreBackend : public IZoogiStoreBackend
{
public:
	virtual FString StoreName() const override { return TEXT("steam"); }
	virtual bool IsAvailable(FString& OutWhy) const override { OutWhy = TEXT("Steam microtransactions need Evan's Steamworks app id, item definitions and a server holding the publisher Web API key (InitTxn/FinalizeTxn). Not set up."); return false; }
	virtual void QueryProducts(const TArray<FString>&, TFunction<void(bool, const TArray<FZoogiStoreProduct>&)> Done) override { Done(false, {}); }
	virtual void Purchase(const FString&, TFunction<void(bool, const FZoogiStoreReceipt&, const FString&)> Done) override { Done(false, FZoogiStoreReceipt(), TEXT("steam unavailable")); }
	virtual EZoogiVerifyResult Verify(const FZoogiStoreReceipt&, FString& OutWhy) override { OutWhy = TEXT("steam unavailable"); return EZoogiVerifyResult::Unavailable; }
	virtual void Finalize(const FZoogiStoreReceipt&) override {}
	virtual void QueryUnfinished(TFunction<void(const TArray<FZoogiStoreReceipt>&)> Done) override { Done({}); }
};

enum class EZoogiPurchaseOutcome : uint8 { Granted, AlreadyGranted, Rejected, PendingVerification, UnknownProduct, StoreUnavailable };

/** The purchase pipeline (store-agnostic). Idempotent by (store, transaction id): a replayed receipt never grants twice. */
class ZOOGIROLL_API FZoogiPurchaseService
{
public:
	/** Package for a store product id (GemPackages.csv). */
	static const FZoogiGemPackageRow* FindPackage(const FZoogiProgressionConfig& Cfg, const FString& Store, const FString& ProductId);
	static FString ProductIdFor(const FZoogiGemPackageRow& P, const FString& Store);
	/** pending record -> Verify -> grant purchased gems (txn id derived from store+transaction) -> Finalize -> consumed. */
	static EZoogiPurchaseOutcome ProcessReceipt(FZoogiProgression& Prog, IZoogiStoreBackend& Store, const FZoogiStoreReceipt& R, const FDateTime& Utc, FString& OutWhy);
	/** Platform default: iOS -> apple, Android -> google, Windows with -ZoogiStore=steam|epic -> that store, else none. */
	static TSharedPtr<IZoogiStoreBackend> MakePlatformBackend();
	static const TCHAR* OutcomeName(EZoogiPurchaseOutcome O);
};
