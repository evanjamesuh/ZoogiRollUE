# Zoogi Roll Arena UI: Unreal handoff

The app holds the Title screen, Deck, Arena Selection, Story Episodes, Shop, Settings, a Loading screen for each of the 8 arenas, the 4-player Board Battle, and the Battle HUD.
Everything is laid out on a **941 × 1672** portrait canvas, the same size as the reference art.

```
index.html   markup for all screens (each <section> = one UMG widget, see data-umg)
styles.css   theme + components (gold frames, CTA, toggles, sliders…)
script.js    data tables, interactions, Unreal bridge
assets/      bg/ cards/ arenas/ story/ shop/ settings/ icons/ ui/ battle/ fonts/
unreal/layout.json   every interactive element's rect in canvas pixels, per screen
```

Preview: open `index.html` in a browser. Add `?dev` for a screen switcher, or press keys 1–6.

---

## Option A: run the HTML inside Unreal (fastest)

1. Enable the **Web Browser** plugin. Copy this folder to `Content/Movies/ZoogiUI/` (that folder is staged in packaged builds; also add it under *Project Settings → Packaging → Additional Non-Asset Directories to Copy*).
2. Create `WBP_ZoogiUI` with a full-screen **WebBrowser** widget, *Initial URL* empty, **Supports Transparency** on.
3. On Construct, load `file:///` + `FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / "Movies/ZoogiUI/index.html")`.
4. Bind a bridge object so the UI can talk to the game (C++):

```cpp
// ZoogiUIBridge.h
UCLASS(BlueprintType)
class UZoogiUIBridge : public UObject {
  GENERATED_BODY()
public:
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUIEvent, const FString&, Name, const FString&, JsonPayload);
  UPROPERTY(BlueprintAssignable) FOnUIEvent OnUIEvent;
  UFUNCTION() void Broadcast(const FString& Name, const FString& Json) { OnUIEvent.Broadcast(Name, Json); }
};

// after the browser widget is created (SWebBrowser / UWebBrowser subclass):
WebBrowserWidget->BindUObject(TEXT("interface"), Bridge, /*bIsPermanent*/ true);
```

`script.js` calls `ue.interface.broadcast(name, json)`. Handle `OnUIEvent` in Blueprint with a *Switch on String*.

5. To send data to the UI, call `ExecuteJavascript` on the browser:

```
ZoogiUI.show('battle')
ZoogiUI.setCurrency({gems: 1240, coins: 56320})
ZoogiUI.setPlayer({name: 'Player123', level: 28, xp: 0.7})
ZoogiUI.setNotifications(3)
ZoogiUI.setDecks([{name:'My Deck 1', cards:['storm','guard', …]}])
ZoogiUI.setUnlockedEpisodes(2)
ZoogiUI.purchaseResult({sku:'gems_500', ok:true, gems: 1740})
ZoogiUI.accountLinked({provider:'apple', linked:true})
ZoogiUI.setBattle({energy:4, combo:15, awaken:2, time:60})
ZoogiUI.setUnitHP(2, 35)          // unit index 0–3, percent
ZoogiUI.toast('Victory!')
ZoogiUI.startLoading('volcanic', 'board')   // shows that arena's loading screen, then the board
ZoogiUI.setBoard({round: 3, phase: 2, deck: 18})
ZoogiUI.setBoardPlayer(1, {score: 4, ep: 16, dots: 3, pos: [600, 800]})
```

When `ue.interface` exists, real-money purchases and account linking **wait for the engine** to call `purchaseResult` / `accountLinked`. In a plain browser they complete instantly, so you can preview them.

### Events the UI sends (Name → payload)

| Screen | Event | Payload |
|---|---|---|
| all | `UIReady`, `ScreenChanged` | `{screen}` |
| top bar | `OpenProfile`, `OpenNotifications`, `OpenStore` | `{unread}` / `{tab}` |
| deck | `CardSelected` `DeckCardAdded` `DeckCardRemoved` `DeckFilter` `DeckSwitched` `DeckRenamed` `DeckSaved` | `{id, from}` / `{deck, id}` / `{category}` / `{deck, name, cards}` |
| arena | `ArenaSelected`, `ArenaConfirmed` | `{id}` / `{id, deck}` |
| story | `EpisodeSelected` `EpisodeLocked` `WatchEpisode` `PlayReplay` `PlayHighlight` `PlayCinematic` `SeeAll` | `{id}` / `{section}` |
| shop | `ShopTab` `Purchase` `IAPRequest` `InsufficientFunds` | `{tab}` / `{sku, price, currency}` / `{sku, price}` / `{sku, need, have}` |
| settings | `SettingChanged` `SettingsSaved` `AccountLinkRequest` `AccountLinkChanged` | `{key, value}` / full settings / `{provider}` / `{provider, linked}` |
| title / loading | `TitleTapped` `LoadingStarted` `LoadingComplete` | `{}` / `{arena, next}` / `{arena}` |
| board | `PhaseChanged` `PhaseTapped` `BoardAim` `BoardCardSelected` `BoardActivate` `BoardScore` `BoardAwaken` `RoundEnded` `MatchEnded` | `{phase, round}` / `{x, y}` / `{card, aim}` / `{player, points, total}` / `{round, scores}` / `{winner, scores}` |
| battle | `BattleCardSelected` `AbilityActivated` `AwakenActivated` `BattlePaused` `BattleResumed` `BattleQuit` `BattleTimeUp` | `{id}` / `{id, cost, energyLeft}` / `{}` |

Settings keys: `music sfx voice sensitivity` (0–100), `vibration invertY aimAssist updates events friends marketing` (bool), `quality` (`low|medium|high|ultra`), `fps` (`30|60|120`), `language` (BCP-47), `linked.{google,apple,facebook}`.

---

## Option B: rebuild natively in UMG

- **Root:** each screen is a `Canvas Panel` 941 × 1672 inside a `Scale Box` (Stretch: *Scale to Fit*). Set *Project Settings → User Interface → DPI Scale Rule* to *Shortest Side*.
- **Positions:** `unreal/layout.json` lists every button, panel, slider and toggle as `{name, type, x, y, w, h, text}` in canvas pixels. Type these straight into the Canvas slot (anchor top-left). Names match `data-ui` in the HTML, for example `Deck.Save`, `Shop.Tab.gems`, `Settings.Account.apple`.
- **Backgrounds:** `assets/bg/<screen>.jpg` is the full-canvas plate with the UI areas removed. Use it as the bottom `Image`.
- **Art:** card, arena, episode, shop and settings images and all icons are separate files. Import them as `Texture2D` with *TextureGroup UI*, *No Mipmaps*, *Compression: UserInterface2D*.
- **Frames:** the gold borders are CSS. In UMG use a 9-slice `Border`/`Image` with *Draw As: Box*, margin 0.12. Screenshot one frame from the browser at 2× for the source, or paint your own to match:
  - Gold gradient (135°): `#FBE39A → #D9A23F → #F6D47A → #B07424 → #EEC266 → #9A6420 → #F3D27C`
  - Panel fill: `#1A2133 → #121827 → #0E131F` (top → bottom)
  - Active blue: `#2C7DFF → #0F4FF0 → #0634C8`, glow `#3A9BFF`
  - Slider fill: `#0A5BD8 → #1EA5FC → #6FE0FF`
- **Fonts:** `assets/fonts/` holds Cinzel (titles and labels, uppercase, weight 700–800), Lora (top-bar numbers and name, 700) and Open Sans (body text, 500–600). All three are SIL OFL; licenses are included. Import them as Font Faces.
- **Data:** the tables at the top of `script.js` (`CARDS`, `ARENAS`, `EPISODES`, `REPLAYS`, `HIGHLIGHTS`, `CINEMATICS`, `SHOP_FEATURED`, `POPULAR`, `BUNDLES`, `SETTINGS_DEFAULT`) map one-to-one to DataTable row structs.

## Interaction rules (both options)

- **Deck:** tap a card to show its details. Tapping a selected collection card adds it to the deck (max 15, no duplicates). Tapping a selected deck card removes it. Arrows switch between the 3 decks. The pencil renames (16 characters max). Save stores the decks.
- **Arena:** tap a card to select it and update the Featured panel. *Select Arena* sends `ArenaConfirmed` and opens the battle HUD. The back arrow returns to the previous screen.
- **Story:** locked episodes show a toast. Selecting an unlocked episode updates the chapter banner and the *Watch Episode N* button.
- **Shop:** tabs change the featured item. Thumbnails swap the hero image. Gem prices check the balance, ask for confirmation, deduct, and mark the item Owned. `$` prices go through `IAPRequest`.
- **Settings:** sliders support drag and arrow keys. Toggles, segmented controls, language and account linking all work. *Save Settings* stores everything.
- **Battle:** energy refills 1 every 4 s (max 6). A card costing more than your energy can't be activated. Activating adds to the combo and charges Awaken. Awaken fires when 4/4 charged. Pause opens Resume/Quit.

Browser preview data is saved in localStorage under `zoogi.*`. In-engine, the game is the source of truth; push state in with the `ZoogiUI.set*` calls.

## Title, loading and board screens

- **Title:** tap anywhere to go to the Deck menu. The "Tap to Start" label is real text that pulses.
- **Loading:** one widget. The arena art is `assets/load/<arena>.jpg`. The bar is a real progress fill placed over each arena's own frame, and the rect for each arena is in `Loading.bars` in script.js (x, y, w, h). Feed real progress with `Loading.set(0..1)` or let it run. *Select Arena* runs it, then opens the Board Battle (Skyfall Isles opens the original Battle HUD).
- **Board Battle:** turn phases Draw → Awaken → Battle → Resolve → End over 8 rounds. Tap the arena to aim P1, pick a card, then press *Activate* in the Battle phase. Your marble rolls there, rival marbles get pushed by Storm and Heavy Roll, rings around the centre crystal score 3/2/1, and rolling off the ring costs a life dot. Rivals take their rolls in Resolve. Awaken is worth +3 when the 4 pips are full. After round 8 a result box offers a rematch or exit. The board art is the Frozen Ring; other arenas use the same board with their name on the plate until you have their board art.
