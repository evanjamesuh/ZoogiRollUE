// b207: the RAID / CLASSIC loading screens - see ZoogiLoadScreen.h
#include "ZoogiLoadScreen.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiTypes.h"
#include "ZoogiRaid.h"
#include "ZoogiArena.h"
#include "ZoogiParty.h"
#include "ZoogiRoyaleMix.h"
#include "ZoogiTrickShot.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
FVector2D ZoogiFunCanvasMeasure(const FString& S, float Px, int32 Which);
void ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col);
void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);

namespace ZoogiLoadScreen
{
	namespace
	{
		struct FState
		{
			int32 Mode = 0; // b283: EMode
			bool bArmed = false, bRaid = false, bHold = false, bTapPrev = true, bPress = false, bShotA = false, bShotB = false, bHooked = false;
			FString Map, Art, Boss, Sub;
			EZoogiArena Arena = EZoogiArena::Cosmic;
			float T = 0.f; double GoneAt = -1.0;
			TWeakObjectPtr<AZoogiGameMode> G;
		};
		FState S;
		// b283 (Evan: "add loading screens for every mode with updated instructions")
		enum EMode : int32 { MClassic, MRaid, MRoyale, MMulti, MTraining, MKoth, MHotOrb, MOrbStorm, MBossRush, MTrickCourse, MTrickShot, MCount };
		const TCHAR* ModeKey(int32 M) { static const TCHAR* K[] = { TEXT("classic"), TEXT("raid"), TEXT("royale"), TEXT("multi"), TEXT("training"), TEXT("koth"), TEXT("hotorb"), TEXT("orbstorm"), TEXT("bossrush"), TEXT("trickcourse"), TEXT("trickshot") }; return (M >= 0 && M < MCount) ? K[M] : K[0]; }
		const TCHAR* ModeTitle(int32 M) { static const TCHAR* K[] = { TEXT("CLASSIC"), TEXT("RAID"), TEXT("BATTLE ROYALE"), TEXT("MULTIPLAYER"), TEXT("TRAINING"), TEXT("KING OF THE HILL"), TEXT("HOT ORB"), TEXT("ORB STORM"), TEXT("BOSS RUSH"), TEXT("TRICK COURSE"), TEXT("DAILY TRICK SHOT") }; return (M >= 0 && M < MCount) ? K[M] : K[0]; }
		TMap<FString, UTexture2D*> GTex;
		const FLinearColor Ink(0.03f, 0.025f, 0.06f, 0.96f), Gold(1.f, 0.82f, 0.25f), Red(0.93f, 0.12f, 0.18f), Cyan(0.3f, 0.82f, 1.f), Orange(1.f, 0.52f, 0.15f);
		constexpr float LoadSec = 2.4f;

		bool Flag(const TCHAR* F) { return FParse::Param(FCommandLine::Get(), F); }
		UTexture2D* Tex(const FString& Rel)
		{
			if (UTexture2D** Hit = GTex.Find(Rel)) return *Hit;
			UTexture2D* T = nullptr;
			const FString P = FPaths::ProjectContentDir() / TEXT("Data/MenuArt") / Rel;
			if (FPaths::FileExists(P)) T = FImageUtils::ImportFileAsTexture2D(P);
			if (T) T->AddToRoot();
			GTex.Add(Rel, T);
			return T;
		}
		/** the picture fills the box (cropped, never stretched) */
		void Cover(UCanvas* C, UTexture2D* T, float X, float Y, float W, float H, const FLinearColor& Tint = FLinearColor::White)
		{
			if (!T || !T->GetResource() || W <= 1.f || H <= 1.f) return;
			const float Ta = (float)T->GetSizeX() / FMath::Max(1, T->GetSizeY()), Ba = W / H;
			FVector2D Uv0(0.f, 0.f), Uv1(1.f, 1.f);
			if (Ta > Ba) { const float K = Ba / Ta; Uv0.X = 0.5f - K * 0.5f; Uv1.X = 0.5f + K * 0.5f; }
			else { const float K = Ta / Ba; Uv0.Y = 0.5f - K * 0.5f; Uv1.Y = 0.5f + K * 0.5f; }
			FCanvasTileItem It(FVector2D(X, Y), T->GetResource(), FVector2D(W, H), Uv0, Uv1, Tint); It.BlendMode = SE_BLEND_Opaque; C->DrawItem(It);
		}
		TArray<FString> Wrap(const FString& Txt, float Px, float MaxW, int32 Which)
		{
			TArray<FString> Words; Txt.ParseIntoArray(Words, TEXT(" "));
			TArray<FString> Lines; FString Cur;
			for (const FString& Wd : Words)
			{
				const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
				if (!Cur.IsEmpty() && ZoogiFunCanvasMeasure(Try, Px, Which).X > MaxW) { Lines.Add(Cur); Cur = Wd; }
				else Cur = Try;
			}
			if (!Cur.IsEmpty()) Lines.Add(Cur);
			return Lines;
		}
		FString BossFor(EZoogiArena A)
		{
			switch (A)
			{
			case EZoogiArena::Frozen: return TEXT("DRACORIA");
			case EZoogiArena::Meadow: return TEXT("GROVE TITAN");
			case EZoogiArena::Volcanic: return TEXT("MAGMA TITAN");
			case EZoogiArena::Arabian: return TEXT("DJINN TITAN");
			case EZoogiArena::Tomb: return TEXT("PHARAOH TITAN");
			case EZoogiArena::Neon: return TEXT("NEON TITAN");
			default: return TEXT("VOID TITAN");
			}
		}
		FString ArtFor(EZoogiArena A)
		{
			switch (A)
			{
			case EZoogiArena::Frozen: return TEXT("ModeSelect/ms_map_frozen");
			case EZoogiArena::Meadow: return TEXT("ModeSelect/ms_map_meadow");
			case EZoogiArena::Volcanic: return TEXT("ModeSelect/ms_map_volcanic");
			case EZoogiArena::Arabian: return TEXT("ModeSelect/ms_map_arabian");
			case EZoogiArena::Tomb: return TEXT("ModeSelect/ms_map_tomb");
			case EZoogiArena::Neon: return TEXT("ModeSelect/ms_map_neon");
			case EZoogiArena::Ancient: return TEXT("ModeSelect/ms_map_ancient");
			case EZoogiArena::Training: return TEXT("ModeSelect/ms_map_training");
			default: return ZoogiUnderrealm::Picked() ? TEXT("ModeSelect/ms_map_darkrealm") : TEXT("ModeSelect/ms_map_cosmic");
			}
		}
		/** the map card art: the landscape card or the tall phone shot, whichever fits the box better */
		UTexture2D* MapArt(float BoxAspect)
		{
			UTexture2D* A = Tex(S.Art + TEXT(".png"));
			UTexture2D* B = Tex(S.Art + TEXT("_p.jpg"));
			if (!A) return B; if (!B) return A;
			auto Fit = [&](UTexture2D* T) { const float Ta = (float)T->GetSizeX() / FMath::Max(1, T->GetSizeY()); return FMath::Abs(FMath::Loge(Ta / BoxAspect)); };
			return Fit(A) <= Fit(B) ? A : B;
		}

		struct FTip { const TCHAR* Title; FString Body; FLinearColor Col; };

		void DrawTip(UCanvas* C, float U, float X, float Y, float W, float H, int32 N, const FTip& Tp, float In)
		{	// a P5 slanted card: the number badge, the title in its colour, the white body
			const float Sl = 14.f * U;
			const float Ox = (1.f - In) * 260.f * U; const float A = FMath::Clamp(In * 1.4f, 0.f, 1.f);
			const FVector2D Ctr(X + W * 0.5f + Ox, Y + H * 0.5f);
			ZoogiFunCanvasQuad(C, Ctr + FVector2D(9.f, 9.f) * U, W, H, 0.f, Sl, FLinearColor(Tp.Col.R * 0.6f, Tp.Col.G * 0.15f, Tp.Col.B * 0.2f, 0.9f * A));
			ZoogiFunCanvasQuad(C, Ctr, W + 6.f * U, H + 6.f * U, 0.f, Sl, FLinearColor(1.f, 1.f, 1.f, 0.95f * A));
			ZoogiFunCanvasQuad(C, Ctr, W, H, 0.f, Sl, FLinearColor(Ink.R, Ink.G, Ink.B, 0.97f * A));
			const float Bw = FMath::Min(H * 0.82f, 96.f * U);
			const FVector2D Bc(X + Ox + Bw * 0.5f + 12.f * U, Y + H * 0.5f);
			ZoogiFunCanvasQuad(C, Bc, Bw, H - 14.f * U, 0.f, Sl * 0.8f, FLinearColor(Tp.Col.R, Tp.Col.G, Tp.Col.B, A));
			ZoogiFunCanvasText(C, FString::FromInt(N), Bc.X, Bc.Y, H * 0.62f, FLinearColor(0.02f, 0.02f, 0.04f, A), 1, 0);
			const float Tx = Bc.X + Bw * 0.5f + 22.f * U, Tw = X + Ox + W - Tx - 20.f * U;
			const float TPx = FMath::Min(40.f * U, H * 0.3f), BPx = FMath::Min(25.f * U, H * 0.19f);
			ZoogiFunCanvasText(C, Tp.Title, Tx, Y + H * 0.25f, TPx, FLinearColor(Tp.Col.R, Tp.Col.G, Tp.Col.B, A), 0, 0);
			const TArray<FString> L = Wrap(Tp.Body, BPx, Tw, 0);
			for (int32 i = 0; i < FMath::Min(3, L.Num()); ++i)
				ZoogiFunCanvasText(C, L[i], Tx, Y + H * 0.52f + i * BPx * 1.32f, BPx, FLinearColor(1.f, 1.f, 1.f, A), 0, 0);
		}

		void DrawCb(AHUD* H, UCanvas* C)
		{
			if (!S.bHold && S.GoneAt < 0.0) return;
			if (!H || !C || !H->GetWorld()) return;
			AZoogiGameMode* G = H->GetWorld()->GetAuthGameMode<AZoogiGameMode>();
			if (!G || G != S.G.Get()) return;
			const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
			const bool bPort = Ht > Wd;
			const double Tn = FPlatformTime::Seconds();
			// fade out after the tap
			float Al = 1.f;
			if (!S.bHold) { Al = FMath::Clamp(1.f - (float)(Tn - S.GoneAt) / 0.35f, 0.f, 1.f); if (Al <= 0.f) { S.GoneAt = -1.0; return; } }
			const FLinearColor Acc = S.bRaid ? Red : (S.Mode == MRoyale ? Orange : (S.Mode >= MKoth && S.Mode <= MOrbStorm) ? Gold : Cyan);
			ZoogiFunCanvasRect(C, 0.f, 0.f, Wd, Ht, FLinearColor(0.07f, 0.035f, 0.13f, Al));
			for (int32 k = -2; k < 16; ++k) // P5 diagonal stripes
				ZoogiFunCanvasQuad(C, FVector2D(k * 180.f * U + FMath::Fmod((float)Tn * 20.f, 180.f) * U, Ht * 0.5f), Ht * 1.6f, 34.f * U, 70.f, 0.f, FLinearColor(0.16f, 0.09f, 0.26f, 0.55f * Al));
			if (Al < 0.999f)
			{	// the fade: a dark wash over a half-drawn screen would look broken - just fade the backdrop
				ZoogiFunCanvasText(C, TEXT("GO!"), Wd * 0.5f, Ht * 0.5f, 160.f * U, FLinearColor(Gold.R, Gold.G, Gold.B, Al), 1, 0);
				return;
			}
			// ---- the art panel (raid: the boss; classic: the map)
			float Ax, Ay, Aw, Ah;
			if (bPort) { Ax = 0.f; Ay = 0.f; Aw = Wd; Ah = Ht * 0.36f; }
			else if (S.bRaid) { Ax = 0.f; Ay = 0.f; Aw = Wd * 0.40f; Ah = Ht; }
			else { Ax = Wd * 0.62f; Ay = 0.f; Aw = Wd * 0.38f; Ah = Ht; }
			UTexture2D* Art = S.bRaid ? Tex(S.Arena == EZoogiArena::Frozen ? TEXT("Raid/raid_frozen.png") : TEXT("Raid/raid_generic.png")) : MapArt(Aw / Ah);
			if (S.bRaid && !Art) Art = MapArt(Aw / Ah);
			Cover(C, Art, Ax, Ay, Aw, Ah);
			if (!bPort)
			{	// the slashed edge between the art and the tips
				const float Ex = S.bRaid ? Ax + Aw : Ax;
				ZoogiFunCanvasQuad(C, FVector2D(Ex, Ht * 0.5f), Ht * 1.2f, 26.f * U, 84.f, 0.f, FLinearColor(1.f, 1.f, 1.f, 1.f));
				ZoogiFunCanvasQuad(C, FVector2D(Ex + (S.bRaid ? 14.f : -14.f) * U, Ht * 0.5f), Ht * 1.2f, 12.f * U, 84.f, 0.f, Acc);
				if (S.bRaid) ZoogiFunCanvasRect(C, 0.f, 0.f, Aw, Ht * 0.34f, FLinearColor(0.f, 0.f, 0.f, 0.35f)); // the title reads over the art
			}
			else
			{
				ZoogiFunCanvasRect(C, 0.f, Ah - 8.f * U, Wd, 8.f * U, Acc);
				ZoogiFunCanvasRect(C, 0.f, Ah * 0.55f, Wd, Ah * 0.45f - 8.f * U, FLinearColor(0.f, 0.f, 0.f, 0.4f));
			}
			// ---- the title
			{
				const bool bOverArt = bPort || S.bRaid;
				const float Tx = bPort ? 40.f * U : (S.bRaid ? 46.f * U : 60.f * U);
				const float Ty = bPort ? Ah * 0.62f : 74.f * U;
				const float MaxW = bPort ? Wd - 80.f * U : (S.bRaid ? Aw - 70.f * U : Ax - Tx - 70.f * U);
				const FString Kind = ModeTitle(S.Mode); // b283: every mode names itself
				const FString Big = S.bRaid ? S.Boss : S.Map;
				float BPx = (bPort ? 104.f : 118.f) * U; { const float Mw = ZoogiFunCanvasMeasure(Big, BPx, 0).X; if (Mw > MaxW) BPx *= MaxW / Mw; }
				ZoogiFunCanvasText(C, Kind, Tx + 4.f * U, Ty + 4.f * U, 54.f * U, FLinearColor(0.f, 0.f, 0.f, 0.8f), 0, -1);
				ZoogiFunCanvasText(C, Kind, Tx, Ty, 54.f * U, Acc, 0, -1);
				const float By = Ty + 40.f * U + BPx * 0.58f;
				ZoogiFunCanvasText(C, Big, Tx + 6.f * U, By + 6.f * U, BPx, FLinearColor(0.f, 0.f, 0.f, bOverArt ? 0.85f : 0.6f), 0, 0);
				ZoogiFunCanvasText(C, Big, Tx, By, BPx, FLinearColor::White, 0, 0);
				ZoogiFunCanvasText(C, S.Sub, Tx + 2.f * U, By + BPx * 0.66f + 26.f * U, 26.f * U, Gold, 0, 0);
			}
			// ---- the 3 tips
			TArray<FTip> Tips;
			switch (S.Mode)
			{	// b283: updated how-to for every mode
			case MRaid: case MBossRush:
			{
				const bool bDrag = S.Arena == EZoogiArena::Frozen;
				Tips.Add({ TEXT("KNOCK ORBS OFF = DAMAGE"), bDrag ? TEXT("Every orb you knock off the arena fires a volley at her. More orbs in one shot = more damage, gold star orbs hit harder.") : TEXT("Every orb you knock off the arena fires a volley at it. More orbs in one shot = more damage, gold star orbs hit harder."), Orange }); // b285 (Evan: "the loading screen doesn't mention knocking orbs off")
				Tips.Add({ TEXT("HIT THE WEAK POINT"), bDrag ? TEXT("Roll straight into her glowing heart crystal for big hits - bounce off walls to reach it.") : TEXT("Roll straight into its glowing weak point for big hits - bounce off walls to reach it."), Gold });
				Tips.Add({ TEXT("SURVIVE ITS ATTACKS"), TEXT("A red zone or rings = an attack after this turn. Get clear, hide behind cover, or hit the weak point to interrupt it."), Red });
				if (S.Mode == MBossRush) Tips.Add({ TEXT("3 TITANS, ONE POOL OF LIVES"), TEXT("Volcanic Pit, Frozen Ring, then the Tomb - back to back. Lives you lose carry over, so play safe."), Cyan });
				else Tips.Add({ TEXT("KEEP YOUR TEAM LIVES"), FString::Printf(TEXT("5 lives, 10 rounds. Every Zoogi that falls costs one. Beat %s before the rounds run out."), bDrag ? TEXT("her") : TEXT("it")), Cyan });
				break;
			}
			case MRoyale:
				Tips.Add({ TEXT("TAKE YOUR TURNS"), TEXT("Every round plays like Classic: drag back, let go, knock orbs and rivals off the arena for points."), Cyan });
				Tips.Add({ TEXT("NEW ARENA EVERY ROUND"), TEXT("Each round moves to the next arena with its own obstacles and boss - read the map before you shoot."), Orange });
				Tips.Add({ TEXT("POINTS CARRY OVER"), TEXT("Your points add up round after round. The highest total after the last round wins."), Gold });
				break;
			case MMulti:
				Tips.Add({ TEXT("PASS AND PLAY"), TEXT("Take your turn, then hand the phone on. YOUR TURN on a player card shows whose go it is."), Cyan });
				Tips.Add({ TEXT("KNOCK ORBS AND RIVALS OFF"), TEXT("Orbs off the arena score, gold star orbs score more. Knocking a rival over the edge is a big score."), Orange });
				Tips.Add({ TEXT("SKILLS AND REACTION"), TEXT("Pick skills in the menu before you roll. Mid-roll, tap REACTION to pause and redirect, or cast a COMBO skill."), Gold });
				break;
			case MTraining:
				Tips.Add({ TEXT("TRY EVERY SKILL"), TEXT("Open SKILL in the menu and tap one - how it works shows along the bottom. No score, no clock."), Cyan });
				Tips.Add({ TEXT("PRACTISE REACTION"), TEXT("Launch, then tap REACTION mid-roll to pause and redirect into a glowing target."), Gold });
				Tips.Add({ TEXT("ADD RIVALS"), TEXT("Settings - TRAINING BOTS adds CPU Zoogis so you can practise duels and knockouts."), Orange });
				break;
			case MKoth:
				Tips.Add({ TEXT("BREAK INTO THE FORT"), TEXT("The centre ring starts walled in. Three knocks shatter a wall - breaking one you didn't build = +5."), Cyan });
				Tips.Add({ TEXT("HOLD THE RING"), TEXT("Every Zoogi inside the centre ring when the round ends scores 10. Knock rivals out of it."), Gold });
				Tips.Add({ TEXT("BUILD YOUR BARRIER"), TEXT("End your turn in or near the ring and a wall rises between you and the nearest rival (2 walls each)."), Orange });
				break;
			case MHotOrb:
				Tips.Add({ TEXT("THE BOMB ORB"), TEXT("One orb glows hot. After 4 turns it blows up - whoever touched it last is blasted toward the edge."), Red });
				Tips.Add({ TEXT("PASS IT ON"), TEXT("Touched it? Knock it into a rival so they touch it last before it blows."), Gold });
				Tips.Add({ TEXT("SCORE AS USUAL"), TEXT("Every other orb you knock off still scores, and knockouts still count."), Cyan });
				break;
			case MOrbStorm:
				Tips.Add({ TEXT("90 SECONDS"), TEXT("The clock ends the match, not the rounds. Shoot fast - every turn counts."), Gold });
				Tips.Add({ TEXT("ORBS KEEP RAINING"), TEXT("New orbs keep dropping onto the court. Aim where they pile up for big chains."), Cyan });
				Tips.Add({ TEXT("MOST POINTS WINS"), TEXT("Orbs off the arena score, gold star orbs score more. Highest score at 0:00 wins."), Orange });
				break;
			case MTrickCourse:
				Tips.Add({ TEXT("FIND THE MARKED ORB"), TEXT("Each hole has one marked orb. Knock it into the hole - nothing else counts."), Gold });
				Tips.Add({ TEXT("FEWEST SHOTS"), TEXT("Every launch is a shot. Use walls and the BANK! preview to line it up."), Cyan });
				Tips.Add({ TEXT("3 HOLES"), TEXT("Solo, no rivals. Your total over all 3 holes is your score - beat your best."), Orange });
				break;
			case MTrickShot:
				Tips.Add({ TEXT("ONE SHOT A TRY"), TEXT("Today's layout is the same for everyone. You get 3 tries - your best one counts."), Gold });
				Tips.Add({ TEXT("STARS"), TEXT("Knock 1 rival off = 1 star, 2 in one shot = 2 stars, all 3 in one shot = 3 stars. Falling off = 0."), Cyan });
				Tips.Add({ TEXT("COME BACK TOMORROW"), TEXT("A new shot every day. Play days in a row - every 7th day pays gems."), Orange });
				break;
			default:
				Tips.Add({ TEXT("KNOCK ORBS OUT"), TEXT("Drag back and let go to roll. Every orb you knock off the arena scores - gold star orbs score more."), Cyan });
				Tips.Add({ TEXT("KNOCK RIVALS OFF"), TEXT("Hit rivals over the edge - a knockout is a big score and they lose their turn."), Orange });
				Tips.Add({ TEXT("SKILLS AND REACTION"), TEXT("Pick skills in the menu before you roll. Mid-roll, tap REACTION to pause and redirect, or cast a COMBO skill."), Gold });
				break;
			}
			float Tx0, Ty0, Tw, Th, Gap;
			if (bPort) { Tx0 = 34.f * U; Tw = Wd - 68.f * U; Th = 150.f * U; Gap = 22.f * U; Ty0 = Ah + 40.f * U; }
			else if (S.bRaid) { Tx0 = Wd * 0.465f; Tw = Wd * 0.49f; Th = FMath::Min(150.f * U, Ht * (Tips.Num() > 3 ? 0.155f : 0.19f)); Gap = Ht * (Tips.Num() > 3 ? 0.025f : 0.035f); Ty0 = Ht * (Tips.Num() > 3 ? 0.07f : 0.12f); } // b285: 4 raid cards
			else { Tx0 = 60.f * U; Tw = Wd * 0.50f; Th = FMath::Min(140.f * U, Ht * 0.17f); Gap = Ht * 0.028f; Ty0 = Ht * 0.40f; }
			for (int32 i = 0; i < Tips.Num(); ++i)
			{
				const float In = FMath::Clamp((S.T - 0.15f - i * 0.18f) / 0.3f, 0.f, 1.f);
				const float E = 1.f - FMath::Pow(1.f - In, 3.f);
				if (In > 0.f) DrawTip(C, U, Tx0, Ty0 + i * (Th + Gap), Tw, Th, i + 1, Tips[i], E);
			}
			// ---- classic: the rotating TIP strip over the bottom of the map art
			if (!S.bRaid)
			{
				static const TCHAR* Rot[] = {
					TEXT("TIP: aim at a wall - the BANK! preview shows where you go after the bounce"),
					TEXT("TIP: gold star orbs score more - aim for them first"),
					TEXT("TIP: tap REACTION mid-roll to change direction"),
					TEXT("TIP: fill AWAKEN to unleash your Zoogi's ultimate"),
					TEXT("TIP: the leader fills SHOWTIME - tap it in your menu"),
					TEXT("TIP: one ITEM a turn - use it before you roll"),
					TEXT("TIP: tap a skill and how it works shows at the bottom of the screen"),
					TEXT("TIP: SUPERPOWER casts your armed COMBO skills - one per tap"),
					TEXT("TIP: after a COMBO skill mid-roll, REACTION is ready again"),
				};
				const int32 Ri = (int32)(S.T / 3.2f) % UE_ARRAY_COUNT(Rot);
				const float Sx = bPort ? 0.f : Ax, Sw = bPort ? Wd : Aw, Sy = bPort ? Ht * 0.92f - 120.f * U : Ht * 0.78f; // b284: portrait - above the loading bar, off the title
				ZoogiFunCanvasRect(C, Sx, Sy, Sw, 52.f * U, FLinearColor(0.f, 0.f, 0.f, 0.72f));
				float Px = 24.f * U; { const float Mw = ZoogiFunCanvasMeasure(Rot[Ri], Px, 0).X; if (Mw > Sw - 40.f * U) Px *= (Sw - 40.f * U) / Mw; }
				ZoogiFunCanvasText(C, Rot[Ri], Sx + Sw * 0.5f, Sy + 26.f * U, Px, Gold, 1, 0);
			}
			// ---- the loading bar, then TAP TO START
			{
				float Bx, By, Bw;
				if (bPort) { Bx = 60.f * U; Bw = Wd - 120.f * U; By = Ht * 0.92f; }
				else if (S.bRaid) { Bx = Tx0 + 30.f * U; Bw = Tw - 60.f * U; By = Ht * 0.90f; }
				else { Bx = Ax + 40.f * U; Bw = Aw - 80.f * U; By = Ht * 0.90f; }
				const float K = FMath::Clamp(S.T / LoadSec, 0.f, 1.f), Ke = 1.f - FMath::Pow(1.f - K, 2.2f);
				if (K < 1.f)
				{
					ZoogiFunCanvasText(C, FString::Printf(TEXT("LOADING  %d%%"), FMath::Min(99, (int32)(Ke * 100.f))), Bx + Bw * 0.5f, By - 30.f * U, 28.f * U, FLinearColor::White, 1, 0);
					ZoogiFunCanvasRect(C, Bx - 4.f * U, By - 4.f * U, Bw + 8.f * U, 22.f * U, FLinearColor(0.f, 0.f, 0.f, 0.85f));
					ZoogiFunCanvasRect(C, Bx, By, Bw * Ke, 14.f * U, Gold);
				}
				else
				{
					const float P = 1.f + 0.06f * FMath::Sin((float)Tn * 6.f);
					const FVector2D Bc(Bx + Bw * 0.5f, By);
					ZoogiFunCanvasQuad(C, Bc + FVector2D(10.f, 10.f) * U, 520.f * U * P, 86.f * U * P, -3.f, 18.f * U, FLinearColor(Acc.R * 0.7f, Acc.G * 0.5f, Acc.B * 0.5f, 1.f));
					ZoogiFunCanvasQuad(C, Bc, 528.f * U * P, 94.f * U * P, -3.f, 18.f * U, FLinearColor::White);
					ZoogiFunCanvasQuad(C, Bc, 512.f * U * P, 80.f * U * P, -3.f, 18.f * U, Gold);
					ZoogiFunCanvasText(C, TEXT("TAP TO START"), Bc.X, Bc.Y, 54.f * U * P, FLinearColor(0.03f, 0.02f, 0.05f, 1.f), 1, 0);
				}
			}
		}
		void Hook()
		{
			if (S.bHooked) return;
			S.bHooked = true;
			AHUD::OnHUDPostRender.AddStatic(&DrawCb);
		}
		bool TapDown(UWorld* W)
		{
			APlayerController* Pc = W ? W->GetFirstPlayerController() : nullptr;
			if (!Pc) return false;
			float X = 0.f, Y = 0.f; bool bT = false; Pc->GetInputTouchState(ETouchIndex::Touch1, X, Y, bT);
			return bT || Pc->IsInputKeyDown(EKeys::LeftMouseButton) || Pc->IsInputKeyDown(EKeys::SpaceBar) || Pc->IsInputKeyDown(EKeys::Enter) || Pc->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom);
		}
		void Release(AZoogiGameMode* G, const TCHAR* Why)
		{
			S.bHold = false; S.GoneAt = FPlatformTime::Seconds();
			ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 0.9f, 1.1f);
			UE_LOG(LogZoogi, Log, TEXT("LOADING SCREEN done (%s) after %.1f s - the match starts"), Why, S.T);
		}
	}

	void Arm(bool bRaid, const FString& MapName, const FString& Art)
	{
		if (Flag(TEXT("ZoogiNoLoadScreen"))) return;
		S.bArmed = true; S.bRaid = bRaid; S.Map = MapName.ToUpper(); S.Art = Art;
	}
	void ArmMode()
	{	// b283: any other mode card - the mode is worked out when the match starts
		if (Flag(TEXT("ZoogiNoLoadScreen"))) return;
		S.bArmed = true; S.bRaid = false; S.Map.Reset(); S.Art.Reset();
	}

	bool Holding() { return S.bHold; }

	bool Begin(AZoogiGameMode* G)
	{
		if (!G || Flag(TEXT("ZoogiNoLoadScreen"))) { S.bArmed = false; return false; }
		static bool bCmd = Flag(TEXT("ZoogiLoadScreen"));
		if (bCmd && !S.bArmed)
		{	// a command-line match (tests)
			S.bArmed = true; S.bRaid = ZoogiRaid::Armed() || Flag(TEXT("ZoogiRaid"));
			S.Map = ZoogiUnderrealm::Picked() ? FString(TEXT("DARK REALM")) : ZoogiUtil::ArenaName(G->MenuArena).ToUpper(); S.Art.Reset();
		}
		bCmd = false;
		if (!S.bArmed) return false;
		S.bArmed = false;
		S.Arena = G->MenuArena;
		{	// b283: which mode is this
			int32 Hum = 0; for (int32 i = 0; i < FMath::Min(G->MenuNumPlayers, G->MenuPlayers.Num()); ++i) if (!G->MenuPlayers[i].bCPU) ++Hum;
			const ZoogiParty::EKind Pk = ZoogiParty::Kind();
			S.Mode = Pk == ZoogiParty::EKind::Koth ? MKoth : Pk == ZoogiParty::EKind::HotOrb ? MHotOrb : Pk == ZoogiParty::EKind::OrbStorm ? MOrbStorm
				: Pk == ZoogiParty::EKind::BossRush ? MBossRush : Pk == ZoogiParty::EKind::TrickCourse ? MTrickCourse
				: (S.bRaid || ZoogiRaid::Armed()) ? MRaid : ZoogiRoyaleMix::Active() ? MRoyale : ZoogiTrickShot::Active() ? MTrickShot
				: G->bSoloTraining ? MTraining : Hum > 1 ? MMulti : MClassic;
			FString Fm; if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiLoadMode="), Fm)) for (int32 m = 0; m < MCount; ++m) if (Fm == ModeKey(m)) S.Mode = m; // previews
			S.bRaid = S.Mode == MRaid || S.Mode == MBossRush;
			if (S.Map.IsEmpty()) S.Map = ZoogiUnderrealm::Picked() ? FString(TEXT("DARK REALM")) : ZoogiUtil::ArenaName(G->MenuArena).ToUpper();
		}
		if (S.Art.IsEmpty()) S.Art = ArtFor(S.Arena);
		S.Boss = BossFor(S.Arena);
		switch (S.Mode)
		{	// b283
		case MRaid: S.Sub = FString::Printf(TEXT("CO-OP  -  YOU + %d ALLIES VS %s"), FMath::Max(0, G->MenuNumPlayers - 1), S.Arena == EZoogiArena::Frozen ? TEXT("THE DRAGON") : TEXT("THE TITAN")); break;
		case MBossRush: S.Sub = FString::Printf(TEXT("CO-OP  -  YOU + %d ALLIES  -  3 TITANS BACK TO BACK"), FMath::Max(0, G->MenuNumPlayers - 1)); break;
		case MRoyale: S.Sub = FString::Printf(TEXT("%d ZOOGIS  -  A NEW ARENA EVERY ROUND  -  MOST POINTS WINS"), G->MenuNumPlayers); break;
		case MMulti: S.Sub = FString::Printf(TEXT("%d PLAYERS  -  PASS THE PHONE  -  %d ROUNDS"), G->MenuNumPlayers, G->MenuRounds); break;
		case MTraining: S.Sub = TEXT("PRACTICE  -  NO SCORE, NO CLOCK"); break;
		case MKoth: S.Sub = FString::Printf(TEXT("%d ZOOGIS  -  HOLD THE CENTRE RING  -  %d ROUNDS"), G->MenuNumPlayers, G->MenuRounds); break;
		case MHotOrb: S.Sub = FString::Printf(TEXT("%d ZOOGIS  -  DON'T TOUCH THE BOMB LAST"), G->MenuNumPlayers); break;
		case MOrbStorm: S.Sub = FString::Printf(TEXT("%d ZOOGIS  -  90 SECONDS OF RAINING ORBS"), G->MenuNumPlayers); break;
		case MTrickCourse: S.Sub = TEXT("SOLO  -  3 HOLES  -  FEWEST SHOTS WINS"); break;
		case MTrickShot: S.Sub = TEXT("ONE SHOT  -  3 TRIES  -  UP TO 3 STARS"); break;
		default: S.Sub = FString::Printf(TEXT("%d ZOOGIS  -  %d ROUNDS  -  MOST POINTS WINS"), G->MenuNumPlayers, G->MenuRounds); break;
		}
		S.bHold = true; S.T = 0.f; S.GoneAt = -1.0; S.bTapPrev = true; S.bPress = false; S.bShotA = S.bShotB = false; S.G = G;
		Hook();
		UE_LOG(LogZoogi, Log, TEXT("LOADING SCREEN: %s %s (%s)"), ModeTitle(S.Mode), S.bRaid ? *S.Boss : *S.Map, *S.Art);
		return true;
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!S.bHold || !G) return;
		S.T += FMath::Min(RealDt, 0.1f);
		const bool bDown = TapDown(G->GetWorld());
		const bool bReady = S.T >= LoadSec;
		if (bReady && bDown && !S.bTapPrev) S.bPress = true; // a press that began once TAP TO START was up
		S.bTapPrev = bDown;
		if (S.bPress && !bDown) { Release(G, TEXT("tap")); return; }
		static const bool bAuto = Flag(TEXT("ZoogiAuto"));
		if (bAuto)
		{	// tests: frames of both states, then it starts by itself
			if (!S.bShotA && S.T >= 1.2f) { S.bShotA = true; G->TakeShot(FString::Printf(TEXT("load_%s_a"), ModeKey(S.Mode))); }
			if (!S.bShotB && S.T >= LoadSec + 1.0f) { S.bShotB = true; G->TakeShot(FString::Printf(TEXT("load_%s_b_tap"), ModeKey(S.Mode))); }
			if (S.T >= LoadSec + 2.6f) Release(G, TEXT("test auto start"));
		}
	}
}
