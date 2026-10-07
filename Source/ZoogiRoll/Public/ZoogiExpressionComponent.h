#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZoogiExpressionComponent.generated.h"

class AZoogiBall;
class AZoogiGameMode;
class UMaterialInstanceDynamic;

/** Facial expressions. The value is the cell index in the character's 4x4 eye atlas (T_<Name>_EyesAtlas_BC/E, row-major from top-left). */
UENUM(BlueprintType)
enum class EZoogiExpr : uint8
{
	Neutral = 0,
	Blink = 1,
	Focused = 2,     // Battle: Aim
	Determined = 3,  // Battle: PowerUp, Awaken: Infuse, Partner Follow-Up
	Excited = 4,     // Battle: Rolling, Ally Bump, landing a hit
	Dizzy = 5,       // hard impact, stunned
	Worried = 6,     // near the edge, a rolling shooter bearing down
	KO = 7,          // falling out / knocked out
	Happy = 8,       // knock-out credit, orb scored, match won
	Sad = 9,         // match lost, bound (curse)
	Guard = 10,      // Awaken: Guard, braced while an attacker rolls
	Flourish = 11,   // personality beat: Draw hero beat + idle (Vex smug, Tavi wink, Orin meditation, Sera serene, ...)
	Num UMETA(Hidden)
};

/**
 * One coordinator for a Zoogi's face and light character animation (Evan Oct 6: "expressions for certain phases").
 * Polls the turn state every frame (stage / battle step / phase, artifact Infuse / Guard, statuses, edge distance, combo callouts,
 * match result) plus one push from AZoogiBall's hit handler, picks an expression, and drives:
 *  - the eye material (ExprIndex into the eye atlas, blinks, eye / core glow; Dark Zoogy's visor optic pulse),
 *  - the flipper nubs (flap / raise / droop / tremble / retract) and small body beats through the existing visual hooks,
 *  - the face orientation: once the ball settles the model turns its face toward the camera (FaceAlign on AZoogiBall).
 * Visual only: never touches physics, collision or the turn logic.
 * Log: "Expr P1 Blue -> focused (aim)". -ZoogiExprDemo (with -ZoogiShots) freezes the first Draw beat and photographs every
 * expression of every Zoogi in a gameplay-angle close-up ("exprdemo_<id>_<NN>_<expr>").
 */
UCLASS(ClassGroup = (Zoogi), meta = (BlueprintSpawnableComponent))
class ZOOGIROLL_API UZoogiExpressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZoogiExpressionComponent();

	/** Called by AZoogiBall::InitZoogi once the mesh is set: finds the Eyes / Core material slots and the persona. */
	void Setup();
	/** Hard contact (from AZoogiBall::OnSphereHit). */
	void OnImpact(float Impulse, AActor* Other);
	/** Short override (seconds). */
	void Trigger(EZoogiExpr E, float Duration, const TCHAR* Why);
	/** Debug / demo: forces an expression until cleared (EZoogiExpr::Num clears). */
	void ForceExpr(EZoogiExpr E) { Forced = E; }

	UPROPERTY(BlueprintReadOnly) EZoogiExpr Current = EZoogiExpr::Neutral;
	static const TCHAR* ExprName(EZoogiExpr E);
	/** [ZoogiRoll.Characters] FaceCamera (default true) / FaceMaxTiltDeg (default 32). */
	static bool FaceCamera();
	static float FaceMaxTilt();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	struct FPersona
	{
		float BlinkMin = 2.4f, BlinkMax = 4.6f; // seconds between blinks
		float IdleFlourishMin = 8.f, IdleFlourishMax = 14.f;
		float Energy = 1.f;     // nub / bounce amplitude
		float Tempo = 1.f;      // animation speed
		bool bVisor = false;    // Dark Zoogy: no blink, optic pulse
	};
	FPersona Persona;
	AZoogiBall* Ball() const;
	EZoogiExpr ChooseBase(AZoogiGameMode* GM, FString& Why) const;
	void PollEvents(AZoogiGameMode* GM);
	void Apply(EZoogiExpr Shown, float Dt);
	void TickFace(AZoogiGameMode* GM, float Dt);
	void TickDemo(AZoogiGameMode* GM, float Dt);
	bool IsFalling() const;
	float EdgeDistance(AZoogiGameMode* GM) const;

	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> EyeMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CoreMID;
	float CoreBoost0 = 1.f;
	float EyeBoost0 = 1.f;
	bool bSetup = false;

	EZoogiExpr Forced = EZoogiExpr::Num;
	EZoogiExpr EventExpr = EZoogiExpr::Num;
	float EventLeft = 0.f;
	FString EventWhy;
	EZoogiExpr Logged = EZoogiExpr::Num;
	float Clock = 0.f;
	float StateAge = 0.f;
	float NextBlink = 3.f, BlinkLeft = 0.f;
	float NextIdleFlourish = 10.f, IdleFlourishLeft = 0.f;
	float NeutralTime = 0.f;
	float BeatClock = 0.f;
	float LastImpact = -10.f;
	int32 LastKOs = 0, LastOrbs = 0, LastStun = 0, LastBound = 0, LastScore = 0;
	TMap<FString, float> SeenCallouts;
	float SpawnZ = 0.f;

	// -ZoogiExprDemo
	int32 DemoStep = -1;     // -1 idle, >= 0 running, -2 done
	float DemoClock = 0.f;
	int32 DemoBall = 0;
	UPROPERTY() TObjectPtr<class ACameraActor> DemoCam;
	TWeakObjectPtr<AActor> DemoPrevView;
};
