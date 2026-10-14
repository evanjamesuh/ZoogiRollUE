#include "ZoogiStore.h"
#include "ZoogiRoll.h"
#include "Misc/Base64.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlinePurchaseInterface.h"
#include "Interfaces/OnlineStoreInterfaceV2.h"

// ------------------------------------------------------------------ Apple JWS (StoreKit 2 signed transaction)

namespace ZoogiAppleJws
{
	static bool B64Url(const FString& In, FString& OutUtf8)
	{
		FString S = In.Replace(TEXT("-"), TEXT("+")).Replace(TEXT("_"), TEXT("/"));
		while (S.Len() % 4) S += TEXT("=");
		TArray<uint8> Bytes;
		if (!FBase64::Decode(S, Bytes)) return false;
		Bytes.Add(0);
		OutUtf8 = UTF8_TO_TCHAR((const ANSICHAR*)Bytes.GetData());
		return true;
	}
	bool Decode(const FString& Jws, FPayload& Out, FString& OutWhy)
	{
		TArray<FString> Parts; Jws.ParseIntoArray(Parts, TEXT("."), false);
		if (Parts.Num() != 3) { OutWhy = TEXT("JWS must have 3 parts"); return false; }
		FString H, P;
		if (!B64Url(Parts[0], H) || !B64Url(Parts[1], P)) { OutWhy = TEXT("bad base64url"); return false; }
		TSharedPtr<FJsonObject> HJ, PJ;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(H), HJ) || !HJ || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(P), PJ) || !PJ) { OutWhy = TEXT("bad JSON"); return false; }
		if (HJ->GetStringField(TEXT("alg")) != TEXT("ES256")) { OutWhy = TEXT("alg must be ES256"); return false; }
		const TArray<TSharedPtr<FJsonValue>>* X5c = nullptr;
		if (!HJ->TryGetArrayField(TEXT("x5c"), X5c) || X5c->Num() < 3) { OutWhy = TEXT("x5c chain missing"); return false; }
		auto Str = [&](const TCHAR* K) { FString V; if (!PJ->TryGetStringField(K, V)) { double D; if (PJ->TryGetNumberField(K, D)) V = FString::Printf(TEXT("%.0f"), D); } return V; };
		Out.TransactionId = Str(TEXT("transactionId")); Out.OriginalTransactionId = Str(TEXT("originalTransactionId")); Out.BundleId = Str(TEXT("bundleId"));
		Out.ProductId = Str(TEXT("productId")); Out.Type = Str(TEXT("type")); Out.Environment = Str(TEXT("environment"));
		Out.bRevoked = PJ->HasField(TEXT("revocationDate"));
		return true;
	}
	EZoogiVerifyResult CheckClaims(const FString& Jws, const FZoogiStoreReceipt& R, const FString& ExpectedBundleId, FString& OutWhy)
	{
		FPayload P;
		if (!Decode(Jws, P, OutWhy)) return EZoogiVerifyResult::Invalid;
		if (P.BundleId != ExpectedBundleId) { OutWhy = TEXT("bundleId mismatch"); return EZoogiVerifyResult::Invalid; }
		if (P.ProductId != R.ProductId) { OutWhy = TEXT("productId mismatch"); return EZoogiVerifyResult::Invalid; }
		if (P.TransactionId != R.TransactionId) { OutWhy = TEXT("transactionId mismatch"); return EZoogiVerifyResult::Invalid; }
		if (P.Type != TEXT("Consumable")) { OutWhy = TEXT("not a consumable"); return EZoogiVerifyResult::Invalid; }
		if (P.bRevoked) { OutWhy = TEXT("revoked / refunded"); return EZoogiVerifyResult::Invalid; }
		if (P.Environment == TEXT("Sandbox") && !R.bSandbox) { OutWhy = TEXT("sandbox transaction in production"); return EZoogiVerifyResult::Invalid; }
		return EZoogiVerifyResult::Valid; // claims only: signature/chain still need the server (CheckClaims is never the only gate in production)
	}
}

// ------------------------------------------------------------------ OSS backend (Apple / Google / Epic)

static FString StoreCfg(const TCHAR* Key)
{
	FString V; if (GConfig) GConfig->GetString(TEXT("ZoogiRoll.Store"), Key, V, GGameIni); return V;
}

bool FZoogiOSSStoreBackend::IsAvailable(FString& OutWhy) const
{
	IOnlineSubsystem* S = IOnlineSubsystem::Get(OSS);
	if (!S) { OutWhy = FString::Printf(TEXT("OnlineSubsystem %s not loaded on this platform (plugin / platform / config not set up)"), *OSS.ToString()); return false; }
	if (!S->GetPurchaseInterface().IsValid()) { OutWhy = TEXT("no IOnlinePurchase"); return false; }
	if (StoreCfg(TEXT("VerifyUrl")).IsEmpty()) { OutWhy = TEXT("[ZoogiRoll.Store] VerifyUrl (receipt verification server) not configured"); return false; }
	return true;
}

void FZoogiOSSStoreBackend::QueryProducts(const TArray<FString>& Ids, TFunction<void(bool, const TArray<FZoogiStoreProduct>&)> Done)
{
	IOnlineSubsystem* S = IOnlineSubsystem::Get(OSS);
	IOnlineStoreV2Ptr St = S ? S->GetStoreV2Interface() : nullptr;
	IOnlineIdentityPtr Id = S ? S->GetIdentityInterface() : nullptr;
	FUniqueNetIdPtr User = Id ? Id->GetUniquePlayerId(0) : nullptr;
	if (!St || !User) { Done(false, {}); return; }
	TArray<FUniqueOfferId> Offers(Ids);
	St->QueryOffersById(*User, Offers, FOnQueryOnlineStoreOffersComplete::CreateLambda([St, Done](bool bOk, const TArray<FUniqueOfferId>& Got, const FString&)
	{
		TArray<FZoogiStoreProduct> Out;
		for (const FUniqueOfferId& O : Got)
			if (TSharedPtr<FOnlineStoreOffer> Off = St->GetOffer(O)) Out.Add({ O, NAME_None, Off->Title.ToString(), Off->GetDisplayPrice().ToString() });
		Done(bOk, Out);
	}));
}

void FZoogiOSSStoreBackend::Purchase(const FString& ProductId, TFunction<void(bool, const FZoogiStoreReceipt&, const FString&)> Done)
{
	IOnlineSubsystem* S = IOnlineSubsystem::Get(OSS);
	IOnlinePurchasePtr P = S ? S->GetPurchaseInterface() : nullptr;
	IOnlineIdentityPtr Id = S ? S->GetIdentityInterface() : nullptr;
	FUniqueNetIdPtr User = Id ? Id->GetUniquePlayerId(0) : nullptr;
	if (!P || !User) { Done(false, FZoogiStoreReceipt(), TEXT("store unavailable")); return; }
	FPurchaseCheckoutRequest Req; Req.AddPurchaseOffer(FString(), ProductId, 1, true);
	const FString StoreName = Store;
	P->Checkout(*User, Req, FOnPurchaseCheckoutComplete::CreateLambda([Done, StoreName, ProductId](const FOnlineError& E, const TSharedRef<FPurchaseReceipt>& Rc)
	{
		FZoogiStoreReceipt R; R.Store = StoreName; R.ProductId = ProductId; R.TransactionId = Rc->TransactionId;
		for (const FPurchaseReceipt::FReceiptOfferEntry& O : Rc->ReceiptOffers) for (const FPurchaseReceipt::FLineItemInfo& L : O.LineItems) if (!L.ValidationInfo.IsEmpty()) { R.Payload = L.ValidationInfo; break; }
		const bool bOk = E.bSucceeded && (Rc->TransactionState == EPurchaseTransactionState::Purchased || Rc->TransactionState == EPurchaseTransactionState::Restored);
		Done(bOk, R, bOk ? FString() : FString(LexToString(Rc->TransactionState)));
	}));
}

EZoogiVerifyResult FZoogiOSSStoreBackend::Verify(const FZoogiStoreReceipt& R, FString& OutWhy)
{
	if (R.TransactionId.IsEmpty() || R.Payload.IsEmpty()) { OutWhy = TEXT("empty receipt"); return EZoogiVerifyResult::Invalid; }
	if (Store == TEXT("apple") && R.Payload.Contains(TEXT(".")))
	{
		const EZoogiVerifyResult C = ZoogiAppleJws::CheckClaims(R.Payload, R, StoreCfg(TEXT("AppleBundleId")), OutWhy);
		if (C != EZoogiVerifyResult::Valid) return C;
	}
	// Trust requires the server (App Store Server API / Play Developer API purchases.products / EOS ecom verify).
	// Until VerifyUrl is implemented by Evan's backend, the receipt stays Pending and NO gems are granted.
	OutWhy = TEXT("awaiting server verification (VerifyUrl)");
	return EZoogiVerifyResult::Pending;
}

void FZoogiOSSStoreBackend::Finalize(const FZoogiStoreReceipt& R)
{
	IOnlineSubsystem* S = IOnlineSubsystem::Get(OSS);
	IOnlinePurchasePtr P = S ? S->GetPurchaseInterface() : nullptr;
	IOnlineIdentityPtr Id = S ? S->GetIdentityInterface() : nullptr;
	FUniqueNetIdPtr User = Id ? Id->GetUniquePlayerId(0) : nullptr;
	if (P && User) P->FinalizePurchase(*User, R.TransactionId, R.Payload);
}

void FZoogiOSSStoreBackend::QueryUnfinished(TFunction<void(const TArray<FZoogiStoreReceipt>&)> Done)
{
	IOnlineSubsystem* S = IOnlineSubsystem::Get(OSS);
	IOnlinePurchasePtr P = S ? S->GetPurchaseInterface() : nullptr;
	IOnlineIdentityPtr Id = S ? S->GetIdentityInterface() : nullptr;
	FUniqueNetIdPtr User = Id ? Id->GetUniquePlayerId(0) : nullptr;
	if (!P || !User) { Done({}); return; }
	const FString StoreName = Store;
	P->QueryReceipts(*User, false, FOnQueryReceiptsComplete::CreateLambda([P, User, Done, StoreName](const FOnlineError&)
	{
		TArray<FPurchaseReceipt> Rs; P->GetReceipts(*User, Rs);
		TArray<FZoogiStoreReceipt> Out;
		for (const FPurchaseReceipt& Rc : Rs)
			for (const FPurchaseReceipt::FReceiptOfferEntry& O : Rc.ReceiptOffers)
				for (const FPurchaseReceipt::FLineItemInfo& L : O.LineItems)
					Out.Add({ StoreName, Rc.TransactionId, L.ItemName, L.ValidationInfo, false });
		Done(Out);
	}));
}

// ------------------------------------------------------------------ pipeline

const TCHAR* FZoogiPurchaseService::OutcomeName(EZoogiPurchaseOutcome O)
{
	switch (O) { case EZoogiPurchaseOutcome::Granted: return TEXT("granted"); case EZoogiPurchaseOutcome::AlreadyGranted: return TEXT("already_granted");
	case EZoogiPurchaseOutcome::Rejected: return TEXT("rejected"); case EZoogiPurchaseOutcome::PendingVerification: return TEXT("pending_verification");
	case EZoogiPurchaseOutcome::UnknownProduct: return TEXT("unknown_product"); case EZoogiPurchaseOutcome::StoreUnavailable: return TEXT("store_unavailable"); }
	return TEXT("?");
}

FString FZoogiPurchaseService::ProductIdFor(const FZoogiGemPackageRow& P, const FString& Store)
{
	if (Store == TEXT("apple")) return P.AppleProductId;
	if (Store == TEXT("google")) return P.GoogleProductId;
	if (Store == TEXT("steam")) return P.SteamItemId;
	if (Store == TEXT("epic")) return P.EpicOfferId;
	if (Store == TEXT("test")) return P.PackageId.ToString();
	return FString();
}

const FZoogiGemPackageRow* FZoogiPurchaseService::FindPackage(const FZoogiProgressionConfig& Cfg, const FString& Store, const FString& ProductId)
{
	return Cfg.GemPackages.FindByPredicate([&](const FZoogiGemPackageRow& P) { return !ProductId.IsEmpty() && ProductIdFor(P, Store) == ProductId; });
}

EZoogiPurchaseOutcome FZoogiPurchaseService::ProcessReceipt(FZoogiProgression& Prog, IZoogiStoreBackend& Store, const FZoogiStoreReceipt& R, const FDateTime& Utc, FString& OutWhy)
{
	FZoogiProfile& P = Prog.Mutable();
	const FZoogiGemPackageRow* Pkg = FindPackage(Prog.Config(), Store.StoreName(), R.ProductId);
	if (!Pkg) { OutWhy = TEXT("unknown product ") + R.ProductId; return EZoogiPurchaseOutcome::UnknownProduct; }
	if (R.TransactionId.IsEmpty()) { OutWhy = TEXT("no transaction id"); return EZoogiPurchaseOutcome::Rejected; }
	FZoogiPurchaseRecord* Rec = P.Purchases.FindByPredicate([&](const FZoogiPurchaseRecord& X) { return X.Store == Store.StoreName() && X.StoreTransactionId == R.TransactionId; });
	if (Rec && Rec->State == EZoogiPurchaseState::Consumed) { Store.Finalize(R); OutWhy = TEXT("duplicate transaction"); return EZoogiPurchaseOutcome::AlreadyGranted; }
	if (Rec && Rec->State == EZoogiPurchaseState::Rejected) { OutWhy = TEXT("previously rejected"); return EZoogiPurchaseOutcome::Rejected; }
	if (!Rec)
	{
		Rec = &P.Purchases.AddDefaulted_GetRef();
		Rec->Store = Store.StoreName(); Rec->StoreTransactionId = R.TransactionId; Rec->ProductId = R.ProductId; Rec->PackageId = Pkg->PackageId;
		Rec->Gems = Pkg->Gems + Pkg->BonusGems; Rec->State = EZoogiPurchaseState::Pending; Rec->CreatedUtc = Utc;
		Rec->GrantTxnId = ZoogiProgress::DeterministicId(TEXT("purchase:") + Rec->Store + TEXT(":") + R.TransactionId);
	}
	Rec->UpdatedUtc = Utc;
	if (Rec->State == EZoogiPurchaseState::Pending)
	{
		const EZoogiVerifyResult V = Store.Verify(R, OutWhy);
		Rec->Note = OutWhy;
		if (V == EZoogiVerifyResult::Invalid) { Rec->State = EZoogiPurchaseState::Rejected; return EZoogiPurchaseOutcome::Rejected; }
		if (V == EZoogiVerifyResult::Pending) return EZoogiPurchaseOutcome::PendingVerification;
		if (V == EZoogiVerifyResult::Unavailable) return EZoogiPurchaseOutcome::StoreUnavailable;
		Rec->State = EZoogiPurchaseState::Verified;
	}
	// Verified: grant (idempotent by GrantTxnId even if we crash between grant and consume), then consume with the store.
	const EZoogiTxnResult G = Prog.GrantGems(Rec->GrantTxnId, EZoogiGemBucket::Purchased, Rec->Gems, FName(*(TEXT("store.") + Rec->Store)), Utc, Rec->Store + TEXT(":") + R.TransactionId);
	if (G != EZoogiTxnResult::Applied && G != EZoogiTxnResult::Duplicate) { OutWhy = ZoogiProgress::TxnResultName(G); return EZoogiPurchaseOutcome::Rejected; }
	Store.Finalize(R);
	Rec->State = EZoogiPurchaseState::Consumed;
	Rec->UpdatedUtc = Utc;
	return G == EZoogiTxnResult::Applied ? EZoogiPurchaseOutcome::Granted : EZoogiPurchaseOutcome::AlreadyGranted;
}

TSharedPtr<IZoogiStoreBackend> FZoogiPurchaseService::MakePlatformBackend()
{
#if PLATFORM_IOS
	return MakeShared<FZoogiOSSStoreBackend>(TEXT("apple"), FName(TEXT("IOS")));
#elif PLATFORM_ANDROID
	return MakeShared<FZoogiOSSStoreBackend>(TEXT("google"), FName(TEXT("GooglePlay")));
#else
	FString S; FParse::Value(FCommandLine::Get(), TEXT("ZoogiStore="), S);
	if (S.IsEmpty()) S = StoreCfg(TEXT("DesktopStore"));
	if (S.Equals(TEXT("steam"), ESearchCase::IgnoreCase)) return MakeShared<FZoogiSteamStoreBackend>();
	if (S.Equals(TEXT("epic"), ESearchCase::IgnoreCase)) return MakeShared<FZoogiOSSStoreBackend>(TEXT("epic"), FName(TEXT("EOS")));
	return nullptr; // no store on this build: the shop shows "purchases unavailable"
#endif
}
