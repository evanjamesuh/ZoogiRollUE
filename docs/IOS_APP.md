# Zoogi Roll on iPhone

This wraps the game you already have in an iPhone app shell. A match on the phone does not need your PC. Scores, accounts, and other online features call a server address only when you set one.

The website on your PC is unchanged. `npm run dev` and `npm run build` still use the same relative addresses they use today.

## What you need from Apple

To put the app on your own iPhone through TestFlight, and later on the App Store, you need an **Apple Developer Program** membership.

- It costs **$99 per year**.
- You enroll yourself at [developer.apple.com/programs](https://developer.apple.com/programs).
- Use the Apple ID you want on the App Store. An organization account needs a company, so a personal account is the usual start.
- Nobody else can enroll for you. This project does not sign you up, and it does not store a certificate or a password.

Until you enroll, you can still look at the Xcode project. You cannot install the app on a phone or upload it to TestFlight.

## What is already in this repo

- The app name on the home screen is **Zoogi Roll**.
- The bundle id is `com.evanjames.zoogiroll`. That is a placeholder. Change it in one file, `capacitor.config.ts`, on the `appId` line. Then run `npm run cap:sync`.
- The icon is Wolfgang, the wolf face already drawn in the game when a portrait photo is missing. The launch picture shows that same face and the name Zoogi Roll. It is not a new character.
- The app opens in landscape and also allows portrait. It is full screen. The buttons already sit inside the phone's safe areas. The page does not rubber-band when you drag past the edge.
- The Xcode project is the `ios/` folder. On a Mac, open `ios/App/App.xcodeproj` after the steps below. `npx cap open ios` opens it for you.

## Build it on a Mac

You need a Mac with Xcode from the Mac App Store. The free Xcode is enough. The $99 is the Apple Developer Program, not Xcode.

The character models, portraits, and stage pictures are stored with Git LFS. On the Mac, or on a cloud Mac, pull those files before you pack the app:

```sh
git lfs pull
```

If you skip that, the app still builds, but it ships without the models. A match would show plain marbles instead of the Zoogi.

A check that does not need the art, such as `npm run check` or `npm test`, can skip the download:

```sh
GIT_LFS_SKIP_SMUDGE=1 git pull
```

On that Mac, from this project folder:

```sh
git lfs pull
npm install
npm run build:ios
npx cap open ios
```

Run those commands before you open Xcode. They copy the game into the app and write the bundle id. `npm run build:ios` does both. `npm run cap:sync` only refreshes Xcode after you change `capacitor.config.ts` or the icon. Run `build:ios` when the game itself changed.

In Xcode:

1. Open the **App** target and pick your Team (the Apple Developer account).
2. If Xcode says the bundle id is already taken, change `appId` in `capacitor.config.ts` and run `npm run cap:sync`.
3. Choose **Any iOS Device** (or your plugged-in iPhone) from the device menu.
4. Use **Product → Archive**.
5. In the organizer window, choose **Distribute App → App Store Connect → Upload**.

The first archive asks Xcode to create a signing certificate. Let Xcode do that. Do not commit the certificate.

### If you do not own a Mac

You cannot create the installable app on a Windows PC. A cloud Mac does the same Xcode steps. People use services such as MacinCloud, or a Mac build in a service such as Codemagic or GitHub Actions. Those are optional and paid by you if you choose them. This repo does not sign you up for any of them.

Whoever runs the Mac still needs your Apple Developer login to upload the build. Do not put that password in this repo.

## The server address

Single-player works with no server. That is the build you get from `npm run build:ios` when you have not set an address.

When you have a hosted game server (the Render service described in the README is one option), point the phone app at it:

```sh
VITE_API_BASE_URL=https://your-game.onrender.com npm run build:ios
```

Or copy `.env.ios.example` to `.env.ios`, put the address in `VITE_API_BASE_URL`, and run `npm run build:ios`. `.env.ios` stays on your machine.

Use the site address only, with `https://`, and no `/api` on the end. Example: `https://zoogi-roll.onrender.com`.

If the address is missing, or the phone cannot reach it, the match still starts. Scores, sign-in, clans, and the other online screens say that online features are off. The app should not go blank.

On the hosted server, set this so the phone is allowed to call it:

```
API_CORS_ORIGIN=capacitor://localhost
```

Leave that unset on your PC. Your PC does not need it.

## TestFlight

TestFlight is Apple's way to install a build before the App Store.

1. Upload the archive (the Xcode steps above).
2. Open [App Store Connect](https://appstoreconnect.apple.com) and create the app **Zoogi Roll** with the same bundle id.
3. Wait until the build finishes processing. Apple emails you.
4. Add yourself as a tester. Install Apple's TestFlight app on your iPhone, then install Zoogi Roll from there.

Internal testers need to be on your developer team. A public TestFlight link needs a short beta review.

## What the App Store asks for

When you are ready to release, App Store Connect asks for a few things you have to provide yourself:

- **Privacy policy.** A web page that says what the game collects. If online features are off, say the match stays on the phone. If you turn on accounts, say that the server stores the username and scores. Apple wants a URL. This repo does not host that page.
- **Screenshots.** Pictures of the game on an iPhone, in landscape. Take them from TestFlight. Apple rejects fake marketing art that is not the real game.
- **Age rating.** You answer a questionnaire. A marble battle with no real-money shop is often low. Chat and names typed by players can raise it, because strangers might type something rude. Answer for the features you actually ship.
- **Support URL and a contact email.**
- A short description and the app icon. The icon in this project is a starting point. You can replace it later with a sharper export of the same Zoogi.

Apple also reviews the binary. Common notes for a game like this: the app must do something without an account (a match does), and buttons must be tappable on a real phone.

## Commands

| Command | What it does |
| --- | --- |
| `git lfs pull` | Download the models and portraits before an iPhone build |
| `GIT_LFS_SKIP_SMUDGE=1` | Skip that download for a check that does not need the art |
| `npm run build:ios` | Pack the game for the phone and copy it into Xcode |
| `npm run cap:sync` | Refresh Xcode after a bundle id or icon change |
| `npm run dev` | Play on this PC, same as before |
| `npm run build` | Website bundle, same as before |

Regenerate the icon and splash from Wolfgang's drawn portrait with:

```sh
node script/ios-art.mjs
```

That writes the pictures under `assets/` and copies every iPhone icon and launch size into `ios/`, including the dark launch picture. Commit those pictures if you want them in the Xcode project.
