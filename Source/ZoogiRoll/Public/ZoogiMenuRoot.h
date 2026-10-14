// menu (Oct 14, Evan approved the mockups 8:45 AM CT): the new main menu. One C++ UMG root with a screen stack
// (Home, Play setup, Zoogi select, Powers, Settings, Collection, Story, Events, Shop, Social, Create) that replaces
// UZoogiMenuWidget's role (that file stays in the tree, unused, for an easy revert). Layout is authored in the mockup's
// 2400x1080 landscape space and fitted to any viewport; touch targets >= 120 px, the primary action bottom right.
// Every button that starts or configures a match calls the existing AZoogiGameMode::MenuAction entry points.
#pragma once

#include "CoreMinimal.h"
#include "ZoogiUIStyle.h"
#include "Blueprint/UserWidget.h"
#include "ZoogiMenuRoot.generated.h"

class UCanvasPanel;
class UOverlay;
class UTexture2D;
class UMediaPlayer;
class UMediaTexture;
class UFileMediaSource;
class AZoogiGameMode;
class UButton;

/** One live menu button + its animation state (not a UPROPERTY: weak refs, rebuilt with the screen). */
struct FZoogiMenuAnimBtn { TWeakObjectPtr<UButton> B; TWeakObjectPtr<class UWidget> Gloss; ZoogiUIStyle::FButtonAnim A; };

enum class EZoogiMenuAnchor : uint8 { Left, Center, Right };

UENUM()
enum class EZoogiScreen : uint8
{
	Home, Play, ZoogiSelect, Powers, Settings, Collection, Story, Events, Shop, Social, Create
};

/** One per button: runs a bound lambda. */
UCLASS()
class ZOOGIROLL_API UZoogiMenuRootClick : public UObject
{
	GENERATED_BODY()
public:
	TFunction<void()> Fn;
	UFUNCTION() void Click() { if (Fn) Fn(); }
};

UCLASS()
class ZOOGIROLL_API UZoogiMenuRoot : public UUserWidget
{
	GENERATED_BODY()
public:
	// ---- same surface the HUD used on UZoogiMenuWidget
	/** Called by the HUD every frame while the menu is up: back keys, tour, rebuild when dirty. */
	void Refresh(float DeltaSeconds);
	/** HUD calls this when the menu is shown again (start or return from a match): back to HOME. */
	void MarkDirty();
	void SetBackdropPlaying(bool bPlay);

	// ---- screen stack
	void Push(EZoogiScreen S);
	void Pop();
	void PopToHome();
	/** Top-left BACK, Android back, Esc. */
	void HandleBack();
	EZoogiScreen Top() const { return Stack.Num() ? Stack.Last() : EZoogiScreen::Home; }

	/** Forward to AZoogiGameMode::MenuAction (unchanged match logic). */
	void Fire(FName Action, int32 Index);
	void ShowComingSoon(const FString& What);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	void Rebuild();
	AZoogiGameMode* GM() const;
	UTexture2D* Art(const FString& Name);
	FString PortraitFor(FName ZoogiId) const;

	// layout helpers (coordinates in the 1024x461 mockup preview space; see .cpp)
	using EAnchor = EZoogiMenuAnchor;
	UZoogiMenuRootClick* BindClick(UButton* B, TFunction<void()> Fn);
	UWidget* Place(UWidget* W, float X, float Y, float Wd, float Ht, EAnchor A = EAnchor::Center);
	class UTextBlock* Txt(const FString& S, int32 Size, const FLinearColor& C, float Outline = 3.f, bool bWrap = false);
	UWidget* Label(const FString& S, float X, float Y, float Wd, float Ht, int32 Size, const FLinearColor& C, EAnchor A = EAnchor::Center, int32 Justify = 0, bool bWrap = false);
	UWidget* Panel(float X, float Y, float Wd, float Ht, const FLinearColor& Fill, EAnchor A = EAnchor::Center);
	UWidget* Btn(const FString& Label, const FLinearColor& Fill, TFunction<void()> Fn, float X, float Y, float Wd, float Ht, int32 FontSize = 30,
		bool bSelected = false, bool bDisabled = false, EAnchor A = EAnchor::Center, const FString& Icon = FString(), const FString& Sub = FString());
	UWidget* ImgTile(UTexture2D* Tex, const FString& Caption, TFunction<void()> Fn, float X, float Y, float Wd, float Ht, bool bSelected, bool bLocked,
		const FLinearColor& Frame, bool bCaptionBottomLeft = true, int32 CaptionSize = 22, const FLinearColor& ImgTint = FLinearColor::White);
	UWidget* Image(UTexture2D* Tex, float X, float Y, float Wd, float Ht, const FLinearColor& Tint = FLinearColor::White, EAnchor A = EAnchor::Center, float Radius = 0.f);
	UWidget* Bar(float X, float Y, float Wd, float Ht, float Frac, const FLinearColor& A, const FLinearColor& B);
	UWidget* Tabs(const TArray<FString>& Names, int32& Current, float X, float Y, float TabW, float Gap, bool bVertical = false);

	void BuildTopBar(const FString& Title);
	void BuildHome();
	void BuildPlay();
	void BuildZoogiSelect();
	void BuildPowers();
	void BuildSettings();
	void BuildCollection();
	void BuildStory();
	void BuildEvents();
	void BuildShop();
	void BuildSocial();
	void BuildCreate();
	void BuildModal();
	void BuildAnimeView();

	void ApplyPlayMode(int32 Mode);
	void CycleSlot(int32 Slot);
	int32 CurrentPlayMode() const;
	void SetLocalSetting(const TCHAR* Key, int32 Value);
	int32 GetLocalSetting(const TCHAR* Key, int32 Default) const;
	void TickTour(float Dt);
	/** Content/Data/PowerArt/<id>.png, else a procedural placeholder card (bOutPlaceholder = true). */
	UTexture2D* PowerArt(const struct FZoogiPowerDef& D, bool& bOutPlaceholder);
	void PowerPlaceholderText(const struct FZoogiPowerDef& D, float X, float Y, float Wd, float Ht);
	TSet<FName> PlaceholderPowers;
	void OnUnlocksChanged() { bDirty = true; }

	UPROPERTY() TObjectPtr<UOverlay> Root;
	UPROPERTY() TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY() TArray<TObjectPtr<UZoogiMenuRootClick>> Clicks;
	UPROPERTY() TMap<FString, TObjectPtr<UTexture2D>> ArtCache;

	TArray<EZoogiScreen> Stack;
	bool bDirty = true;
	int32 BuiltRevision = -1;
	FVector2D BuiltViewport = FVector2D::ZeroVector;
	float DesignW = 2400.f;
	float Time = 0.f;
	// button animation (ZoogiUIStyle::FButtonAnim per live button)
	TArray<FZoogiMenuAnimBtn> AnimBtns;
	int32 AnimScreen = -1;
	bool bAnimEnter = false;
	void AddAnim(class UButton* B, UWidget* Gloss, bool bPrimary, bool bDisabled);
	void TickAnims(float Dt);
	// -ZoogiMenuClip=<dir>: per-frame captures of the button animations
	FString ClipDir; int32 ClipFrame = -1; float ClipClock = 0.f; bool ClipHover = false, ClipPress = false;
	void TickClip(float Dt);
	FDelegateHandle UnlockHandle;

	// screen state
	int32 SelectSlot = 0, PendingZoogi = -1;
	FName PowerSel;
	int32 PowerFilter = 0;           // 0 Void, 1 all types
	int32 SettingsTab = 0, CollectionTab = 0, CollectionSel = 3, EventsTab = 0, ShopTab = 0, ShopSel = 0, SocialTab = 0, StorySel = 1, CreateSel = 0;
	// modal
	bool bModal = false, bModalQuit = false;
	FString ModalTitle, ModalBody;
	// WATCH ANIME
	bool bAnimeView = false;
	FString AnimeClip;
	// backdrop video (ported from UZoogiMenuWidget; off unless [ZoogiRoll.Menu] bMenuVideo or -ZoogiMenuVideo)
	bool EnsureBackdrop();
	UPROPERTY() TObjectPtr<UMediaPlayer> BackdropPlayer;
	UPROPERTY() TObjectPtr<UMediaTexture> BackdropTexture;
	UPROPERTY() TObjectPtr<UFileMediaSource> BackdropSource;
	int32 BackdropState = 0;
	// -ZoogiMenuTour=<dir>: automated screen captures (engine viewport screenshots), then starts a match from PLAY.
	FString TourDir;
	int32 TourStep = -1;
	float TourClock = 0.f;
	bool bTourSetupDone = false;
};
