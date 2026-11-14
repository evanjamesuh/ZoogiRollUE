// Nov 1 b52 (Evan: "Look good build"): the new front end - the Marble Clash menu set (HOME / INVENTORY / SHOP, chest slots,
// chest opening, DAILY LOGIN) drawn in the straight P5 look (ink / paper / magenta, hard shadows, nothing tilted) over the
// LIVE Zoogi Roll arena (Ancient Collision Meadows, a scenic camera per screen) with the player's Zoogi standing in it as the
// Mecha Aurex in its own colours. Mockups: C:\dev\art\mc_menu_refs\b52_p5_arena_v2.jpg, b52_p5_meta_screens.jpg.
// BATTLE opens the existing P5 MODE SELECT; its BACK returns here. -ZoogiNoFront = straight to MODE SELECT (build 51 flow).
// -ZoogiFrontTest = scripted capture run (frames front_*.png in -ZoogiShots).
#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "GameFramework/Actor.h"
#include "ZoogiFront.generated.h"

class UTexture2D;
class ACameraActor;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UZoogiMenuRoot;

class AZoogiBall;
namespace ZoogiFront
{
	ZOOGIROLL_API bool On();
	/** The 8 Aurex Zoogis of the new roster, in menu order: display name, roster id (ZoogiTypes Roster), ball skin (T_Ball_<Ball>). */
	struct FHero { const TCHAR* Name; const TCHAR* RosterId; const TCHAR* Ball; const TCHAR* Element; int32 Rarity; const TCHAR* Blurb; };
	ZOOGIROLL_API const TArray<FHero>& Heroes();
	ZOOGIROLL_API const FHero* HeroByName(const FString& Name);
	/** b100 (Evan: "balls are now ZOOGIS - the player's Awaken collection, a unique power each"): Skill empty = METEOR STRIKE (the ultimate). */
	struct FAwaken { const TCHAR* Ball; const TCHAR* Name; const TCHAR* Skill; const TCHAR* Desc; };
	ZOOGIROLL_API const FAwaken* AwakenForBall(const FString& Ball);
	/** The Awaken power this Zoogi ball rolls with: the human's equipped Zoogi, a CPU's roster pick. */
	ZOOGIROLL_API const FAwaken* AwakenFor(const AZoogiBall* B, const UObject* Ctx);
	/** The equipped hero (profile), Blue when nothing is saved. */
	ZOOGIROLL_API const FHero& Equipped(const UObject* Ctx);
	/** Applies the equipped hero to the game's P1 setup (MenuPlayers[0].ZoogiIndex). */
	ZOOGIROLL_API void ApplyEquipped(UObject* Ctx);

	/** What the widget wants the 3D stage to show this frame (screen space 0..1). */
	struct FView
	{
		int32 Screen = 0;                 // 0 home, 1 inventory, 2 shop
		bool bRobot = true;               // Aurex visible
		FVector2D Feet01 = FVector2D(0.5f, 0.8f);
		float Height01 = 0.56f;           // robot height as a fraction of the screen height
		float Spin = 0.f;                 // extra yaw from dragging
		double AliveT = 0.0;              // FPlatformTime of the last widget tick (the stage switches off when it goes stale)
		FString Hero;                     // equipped hero name
		int32 CheerSerial = 0;            // bumped to play a cheer
	};
	ZOOGIROLL_API FView& View();

	/** Nov 1 b52 (Evan: "fix the cog on battle royale ... add a menu when tapped like on classic"): the Royale pause / settings
	 *  panel (P5 look). Pauses the match, hides the Apple controller; RESUME / RESTART / MAIN MENU + control and audio settings. */
	ZOOGIROLL_API void OpenRoyalePause(UWorld* World);
	ZOOGIROLL_API void CloseRoyalePause(bool bResume);
	ZOOGIROLL_API bool RoyalePauseOpen();
}

/** Nov 1 b52 (Evan: "adjust sensitivity to the joysticks so I can tune them ... toggle inverted aim"): saved control settings
 *  (GameUserSettings [ZoogiRoll.Controls]), tuned from the Royale pause panel. */
namespace ZoogiControls
{
	ZOOGIROLL_API float LookSens();    // screen-swipe look, x (0.3 .. 3.0, default 1)
	ZOOGIROLL_API float StickSens();   // Apple right stick look, x (0.3 .. 3.0, default 1)
	ZOOGIROLL_API float MoveSens();    // move stick response, x (0.5 .. 2.0, default 1: full speed just short of the rim)
	ZOOGIROLL_API bool InvertY();      // up / down look flipped (default off)
	ZOOGIROLL_API int32 StickHide();   // Nov 1 b53: Royale sticks hidden: 0 none, 1 left, 2 right, 3 both (that side then works by touch, invisible)
	ZOOGIROLL_API bool CleanAim();     // Nov 1 b53: classic aim view without the dotted path / reticle / arrow / first-bounce marks (default on)
	ZOOGIROLL_API int32 TrainBots();   // b96c (Evan): Settings > TRAINING BOTS - CPU Zoogis that join you in TRAINING (0..3)
	ZOOGIROLL_API bool WatchCPU();     // b93 (Evan): Settings > WATCH CPU PLAY - every Zoogi is a CPU (from the next match)
	ZOOGIROLL_API bool DarkArena();    // b243 (Evan): Settings > DARK ARENA - the showtime dim for the whole match
	ZOOGIROLL_API bool ReplayCam();    // b70 (Evan): the replay camera, off by default (Settings > REPLAY CAM)
	ZOOGIROLL_API float FlashMul();    // b66 (playtest): full-screen white/teal flash strength: 0.45 normally, 0 with REDUCE FLASHES on
	ZOOGIROLL_API bool AimHintDone();  // the "touch + drag back" slash hint was dismissed this aim step
	ZOOGIROLL_API void SetAimHintDone(bool b);
	ZOOGIROLL_API void Set(const TCHAR* Key, float V);
}

UCLASS()
class ZOOGIROLL_API UZoogiFrontWidget : public UWidget
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<UZoogiMenuRoot> Menu;
	UPROPERTY() TArray<TObjectPtr<UTexture2D>> Keep;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	TSharedPtr<SWidget> MySlate;
};

/** The live 3D part of the front end: a scenic camera over the arena, the Aurex (equipped colours) on a P5 disc, a key light. */
UCLASS()
class ZOOGIROLL_API AZoogiFrontStage : public AActor
{
	GENERATED_BODY()
public:
	AZoogiFrontStage();
	virtual void Tick(float DeltaSeconds) override;
	static AZoogiFrontStage* Ensure(UWorld* World);
private:
	void Activate(bool bOn);
	void ApplyHero(const FString& Hero);
	void PlaceRobot(float Dt);
	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Disc;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> DiscRim;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> DiscRim2;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Model; // b203: a Zoogi with its own Tripo model (Dark) stands here instead of the Aurex
	bool bStaticModel = false;
	UPROPERTY() TObjectPtr<UPointLightComponent> Key;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;
	UPROPERTY() TObjectPtr<class UAnimSequence> Idle;
	UPROPERTY() TArray<TObjectPtr<class UAnimSequence>> Cheers;
	TWeakObjectPtr<AActor> PrevView;
	bool bActive = false;
	FString HeroShown;
	int32 CheerSeen = 0;
	float CheerLeft = 0.f;
	float T = 0.f;
	int32 ScreenShown = -1;
	float ScreenBlend = 1.f;
	FTransform CamFrom, CamTo;
	float FovFrom = 80.f, FovTo = 80.f;
};
