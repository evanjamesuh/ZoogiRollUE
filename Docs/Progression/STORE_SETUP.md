# Gem shop: what is real, what Evan must set up

**Status (Oct 14, 2026): no real purchase is possible yet.** The purchase pipeline is real and tested with an
automation-only test backend (`ZoogiRoll.Store.PurchasePipeline_TestBackendOnly`), which is **not** a payment method. Evan's definition
of done needs real verified purchases on supported platforms, so the shop is **not done**.

## What is built
- `GemPackages.csv`: 4 packages (100 / 550+50 / 1200+200 / 2600+600 gems). Product ids are placeholders.
- `IZoogiStoreBackend`, selected by platform (`FZoogiPurchaseService::MakePlatformBackend`): iOS -> Apple (`OnlineSubsystemIOS`, StoreKit),
  Android -> Google Play (`OnlineSubsystemGooglePlay`), Windows -> `[ZoogiRoll.Store] DesktopStore` / `-ZoogiStore=` steam or epic (`OnlineSubsystemEOS`).
  Apple, Google and Epic use UE's `IOnlinePurchase` (Checkout / FinalizePurchase / QueryReceipts). Steam has a placeholder (needs a server).
- Pipeline: receipt -> Pending record -> Verify -> grant **purchased** gems (transaction id derived from store + store transaction id) -> Finalize/consume -> Consumed.
  Replays, restarts and crashes between grant and consume never double-grant. Every record is persisted with timestamps.
- Apple: StoreKit 2 signed-transaction (JWS) claim checks (ES256 header, x5c present, bundleId, productId, transactionId, Consumable, not revoked, sandbox vs production).
- **Not built:** the ES256 signature + Apple Root CA G3 chain check and the server call. Every real backend returns **Pending** (no gems) until
  `[ZoogiRoll.Store] VerifyUrl` points at a verification server that Evan hosts. That server is required for trusted verification and for online sync (Evan Q6).
- The OnlineSubsystemIOS / GooglePlay / EOS plugins are **not enabled** in `ZoogiRoll.uproject` yet. They are enabled per platform once the accounts exist.

## Apple App Store (first)
1. **Apple Developer Program** membership (USD 99/year), as an individual or an organization (an organization needs a D-U-N-S number).
2. **Bundle ID**: register an explicit App ID (for example `com.<company>.zoogiroll`) with the In-App Purchase capability. Put it in `[ZoogiRoll.Store] AppleBundleId` and in UE Project Settings > iOS.
3. **App Store Connect app record** for that bundle ID.
4. **Paid Apps agreement** accepted under Business, with **banking and tax** forms completed. IAP doesn't work in sandbox or production without it.
5. **Consumable IAP products**, one per gem package. The product IDs must equal `apple_product_id` in `GemPackages.csv` (placeholders today: `com.zoogiroll.gems.small|medium|large|huge`). Each needs a price tier, a display name, a review screenshot and review notes.
6. **Sandbox tester accounts** (App Store Connect > Users and Access > Sandbox).
7. **App Store Server API key** (In-App Purchase key: Key ID, Issuer ID, .p8) for the verification server. Optionally set an App Store Server Notifications V2 URL for refunds.
8. **Mac with Xcode** to build, sign and run on iOS. Evan's PC is Windows, so the options are a Mac (or a Mac mini) as a UE remote-build host, a cloud Mac (for example MacStadium or AWS EC2 Mac), or a CI service with macOS runners (GitHub Actions macOS, Codemagic, Bitrise). He also needs signing certificates and a provisioning profile with In-App Purchase, and an iPhone or iPad for sandbox purchases. TestFlight is used for wider testing.
9. Then: enable `OnlineSubsystemIOS` for iOS, set `[OnlineSubsystemIOS.Store] bSupportsInAppPurchasing=True`, set `[OnlineSubsystem] DefaultPlatformService=IOS` in `Config/IOS/IOSEngine.ini`, host the verifier and set `VerifyUrl`.

## Google Play (brief)
- Google Play Console developer account (USD 25, one time), with identity verification and a payments profile (merchant).
- App created with the package name. A signed AAB uploaded to at least the internal testing track (Billing only works for uploaded builds).
- Managed in-app products (consumable) with ids = `google_product_id`, and license tester accounts.
- A Google Cloud service account linked in Play Console (Play Developer API) for server verification (`purchases.products.get`), and Real-time developer notifications (Pub/Sub) for refunds.
- Enable `OnlineSubsystemGooglePlay` and the Google Play Billing settings in Project Settings > Android.

## Steam (brief)
- Steamworks partner account, the app fee (USD 100 per app) and the app id. Tax and banking forms done in Steamworks.
- Microtransactions are server driven: a server holding the **publisher Web API key** calls `ISteamMicroTxn/InitTxn`. The user authorizes in the overlay, then the server calls `FinalizeTxn` (and `QueryTxn` for recovery). Item ids = `steam_item_id`.
- Enable OnlineSubsystemSteam (or the Steamworks SDK) for the Windows Steam build, and implement `FZoogiSteamStoreBackend` against that server.

## Epic Games Store (brief)
- Epic Games Store publishing agreement and a Developer Portal organization with the product, sandbox and deployment ids, and a client id / secret.
- Offers created in the Developer Portal (consumable entitlements) with ids = `epic_offer_id`.
- Enable `OnlineSubsystemEOS` (+ EOS plus ecom) for the Epic build and configure the artifact. The server verifies or redeems entitlements with the EOS Ecom web APIs.
