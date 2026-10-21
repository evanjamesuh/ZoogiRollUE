#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiAPFXBase.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UDecalComponent;
class UPointLightComponent;
class UPrimitiveComponent;

/**
 * Animations & Powers (Episode Engine spec priorities 9 + 10): shared base for the presentation-only FX actors
 * (AZoogiTitanMeteorFX, AZoogiPowerFX).
 *
 * RULE ZERO: everything an APFX actor owns is visual / audio only. No collision, no physics, no overlap events, never
 * writes to a gameplay actor (it may READ an anchor actor's location to follow it). It ticks while the game is paused
 * (the Episode core plays archetypes inside a paused presentation hold) and runs on real, undilated time, so slow-mo or
 * a hold never stretches or freezes it. Materials are runtime MIDs of the existing /Game/VFX/AwakenFX and KriptoFX
 * masters (no new assets).
 */
UCLASS(Abstract, NotPlaceable)
class ZOOGIROLL_API AZoogiAPFXBase : public AActor
{
	GENERATED_BODY()
public:
	AZoogiAPFXBase();
	virtual void Tick(float DeltaSeconds) override;

	/** Billboards face this point when set (scene-capture review); otherwise the player camera. */
	static TOptional<FVector> ViewOverride;
	/** Real seconds since Play. */
	float GetAge() const { return Age; }
	/** Total length in real seconds (0 = not playing yet). */
	float GetSeconds() const { return Seconds; }
	/** 0..1 normalized progress. */
	float GetProgress() const { return Seconds > 0.f ? FMath::Clamp(Age / Seconds, 0.f, 1.f) : 0.f; }

protected:
	/** Per real-time frame update (Dt = undilated frame time, U = 0..1 progress). Return false to finish (destroy). */
	virtual bool TickFX(float Dt, float U) { return U < 1.f; }

	float Age = 0.f;
	float Seconds = 0.f;
	bool bPlaying = false;
	FRandomStream Rng;

	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Quads;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;

	// ---- assets / components
	static UObject* LoadAsset(const TCHAR* Path);
	/** MID of a material path ("/Game/..."); short names resolve to /Game/VFX/AwakenFX/Materials/<Name>. */
	UMaterialInstanceDynamic* MID(const TCHAR* Path);
	UStaticMeshComponent* MakeMesh(const TCHAR* MeshPath, UMaterialInterface* Mat, bool bShadow);
	UDecalComponent* MakeDecal(UMaterialInterface* Mat, float Depth);
	UPointLightComponent* MakeLight(const FLinearColor& Color, float Radius);
	static void SetDecal(UDecalComponent* D, const FVector& At, float Half, float Yaw = 0.f);
	static void SetScalar(UPrimitiveComponent* C, FName Name, float V, int32 Slot = 0);
	static void SetDecalScalar(UDecalComponent* D, FName Name, float V);

	// ---- real time view
	FVector ViewPos() const;
	/** Floor height under P (visibility trace, read only); Fallback if nothing is hit. */
	float FloorZ(const FVector& P, float Fallback) const;

	// ---- quad batch (per mesh section; positions in world space)
	struct FBatch
	{
		TArray<FVector> V; TArray<int32> Tri; TArray<FVector2D> UV0, UV1; TArray<FLinearColor> Col; TArray<FVector> Nrm;
		void Reset() { V.Reset(); Tri.Reset(); UV0.Reset(); UV1.Reset(); Col.Reset(); Nrm.Reset(); }
	};
	void Billboard(FBatch& B, const FVector& C, float W, float H, const FLinearColor& Color, float Rand, bool bUpright, float Roll = 0.f) const;
	/** Flat quad lying on a plane with normal N (e.g. a floor ring card). */
	void FlatQuad(FBatch& B, const FVector& C, const FVector& N, float Half, const FLinearColor& Color, float Yaw = 0.f, float Rand = 0.f) const;
	/** Camera-facing ribbon through Pts. bFlat: lies in the plane of FlatN instead of facing the camera. */
	void Ribbon(FBatch& B, const TArray<FVector>& Pts, float Width, const FLinearColor& Color, bool bFlat = false, const FVector& FlatN = FVector::UpVector) const;
	/** Flat soft disc (centre colour fading to rim colour; use vertex alpha for a soft edge), both windings. */
	void Disc(FBatch& B, const FVector& C, float Radius, const FLinearColor& Center, const FLinearColor& Rim, int32 Segments = 24) const;
	/** Duplicate every triangle with the opposite winding (one-sided translucent materials seen from either side). */
	static void DoubleSide(FBatch& B);
	void Commit(int32 Section, const FBatch& B);

	static float Ease(float X) { X = FMath::Clamp(X, 0.f, 1.f); return 1.f - FMath::Square(1.f - X); }
	static float EaseIn(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X; }
	static float Smooth(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }
	static float EaseBack(float X, float C = 1.7f) { X = FMath::Clamp(X, 0.f, 1.f); return 1.f + (C + 1.f) * FMath::Pow(X - 1.f, 3.f) + C * FMath::Square(X - 1.f); }
	/** 0 before A, 1 after B, smooth in between. */
	static float Window(float U, float A, float B) { return Smooth((U - A) / FMath::Max(1e-4f, B - A)); }
	/** Rises over [A,B], holds, falls over [C,D]. */
	static float Envelope(float U, float A, float B, float C, float D) { return Window(U, A, B) * (1.f - Window(U, C, D)); }
};
