// Dual-mode holographic spire (see ZoogiHoloSpire.h). Visual only.
#include "ZoogiHoloSpire.h"
#include "ZoogiFX.h"
#include "Engine/StaticMeshActor.h"
#include "HAL/PlatformTime.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiActionFun.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiAnimeSubsystem.h"
#include "MediaTexture.h"
#include "ZoogiHaptics.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "ImageUtils.h"
#include "UnrealClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"
#include "ZoogiHoloFontData.inl"

namespace ZoogiModeSel { FSlateFontInfo FontInfo(float Size, float Outline); } // ZoogiModeSelect.cpp: Lilita One, dark outline

namespace ZoogiHolo
{
	const FLinearColor Cyan(0.25f, 0.85f, 1.f), Gold(1.f, 0.76f, 0.2f), Navy(0.015f, 0.05f, 0.14f), White(1.f, 1.f, 1.f), Orange(1.f, 0.45f, 0.12f);
	constexpr float Split = 0.42f; // morph: 0..Split the scoreboard parts collapse, Split..1 the aim parts build

	using FTris = TArray<FCanvasUVTri>;
	FVector2D Rot(const FVector2D& V, float A) { const float c = FMath::Cos(A), s = FMath::Sin(A); return FVector2D(V.X * c - V.Y * s, V.X * s + V.Y * c); }
	void Tri(FTris& T, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Col, const FLinearColor* CB = nullptr, const FLinearColor* CC = nullptr)
	{
		FCanvasUVTri X;
		X.V0_Pos = A; X.V1_Pos = B; X.V2_Pos = C;
		X.V0_UV = X.V1_UV = X.V2_UV = FVector2D(0.5f, 0.5f);
		X.V0_Color = Col; X.V1_Color = CB ? *CB : Col; X.V2_Color = CC ? *CC : Col;
		T.Add(X);
	}
	void Poly(FTris& T, const TArray<FVector2D>& P, const FLinearColor& Col)
	{
		if (P.Num() < 3) return;
		FVector2D C(0.f, 0.f);
		for (const FVector2D& Q : P) C += Q;
		C /= P.Num();
		for (int32 i = 0; i < P.Num(); ++i) Tri(T, C, P[i], P[(i + 1) % P.Num()], Col);
	}
	void Disc(FTris& T, const FVector2D& C, float R, const FLinearColor& Col, int32 N = 40, const FLinearColor* Edge = nullptr)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const float a0 = 2.f * PI * i / N, a1 = 2.f * PI * (i + 1) / N;
			Tri(T, C, C + FVector2D(FMath::Cos(a0), FMath::Sin(a0)) * R, C + FVector2D(FMath::Cos(a1), FMath::Sin(a1)) * R, Col, Edge, Edge);
		}
	}
	void Ring(FTris& T, const FVector2D& C, float R0, float R1, const FLinearColor& Col, int32 N = 64, float A0 = 0.f, float A1 = 2.f * PI)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const float a0 = A0 + (A1 - A0) * i / N, a1 = A0 + (A1 - A0) * (i + 1) / N;
			const FVector2D d0(FMath::Cos(a0), FMath::Sin(a0)), d1(FMath::Cos(a1), FMath::Sin(a1));
			Tri(T, C + d0 * R0, C + d0 * R1, C + d1 * R1, Col);
			Tri(T, C + d0 * R0, C + d1 * R1, C + d1 * R0, Col);
		}
	}
	void Seg(FTris& T, const FVector2D& A, const FVector2D& B, float Th, const FLinearColor& Col, const FLinearColor* ColB = nullptr)
	{
		const FVector2D D = B - A;
		const float L = D.Size();
		if (L < 0.5f) return;
		const FVector2D N = FVector2D(-D.Y, D.X) / L * (Th * 0.5f);
		const FLinearColor Cb = ColB ? *ColB : Col;
		Tri(T, A - N, A + N, B + N, Col, &Col, &Cb);
		Tri(T, A - N, B + N, B - N, Col, &Cb, &Cb);
	}
	void Capsule(FTris& T, const FVector2D& A, const FVector2D& B, float R, const FLinearColor& Col)
	{
		Seg(T, A, B, R * 2.f, Col);
		Disc(T, A, R, Col, 20);
		Disc(T, B, R, Col, 20);
	}
	TArray<FVector2D> RRectPts(const FVector2D& Min, const FVector2D& Sz, float R)
	{
		TArray<FVector2D> P;
		R = FMath::Min(R, FMath::Min(Sz.X, Sz.Y) * 0.5f);
		const FVector2D C[4] = { FVector2D(Min.X + Sz.X - R, Min.Y + R), FVector2D(Min.X + Sz.X - R, Min.Y + Sz.Y - R), FVector2D(Min.X + R, Min.Y + Sz.Y - R), FVector2D(Min.X + R, Min.Y + R) };
		for (int32 c = 0; c < 4; ++c)
			for (int32 k = 0; k <= 5; ++k)
			{
				const float a = FMath::DegreesToRadians(-90.f + 90.f * c + 18.f * k);
				P.Add(C[c] + FVector2D(FMath::Cos(a), FMath::Sin(a)) * R);
			}
		return P;
	}
	/** Rounded panel: outer border colour, inner fill. */
	void Panel(FTris& T, const FVector2D& Min, const FVector2D& Sz, float R, float Border, const FLinearColor& Edge, const FLinearColor& Fill)
	{
		Poly(T, RRectPts(Min, Sz, R), Edge);
		Poly(T, RRectPts(Min + FVector2D(Border, Border), Sz - FVector2D(Border, Border) * 2.f, FMath::Max(1.f, R - Border)), Fill);
	}
	void Star(FTris& T, const FVector2D& C, float R, float Rot0, const FLinearColor& Col)
	{
		TArray<FVector2D> P;
		for (int32 i = 0; i < 10; ++i)
		{
			const float a = Rot0 - PI * 0.5f + PI * i / 5.f;
			P.Add(C + FVector2D(FMath::Cos(a), FMath::Sin(a)) * (i % 2 == 0 ? R : R * 0.45f));
		}
		for (int32 i = 0; i < 10; ++i) Tri(T, C, P[i], P[(i + 1) % 10], Col);
	}
	/** Solid shapes: written straight into the render target, colour AND alpha (the panel material shows alpha). */
	void Flush(UCanvas* Cv, FTris& T, ESimpleElementBlendMode Mode = SE_BLEND_Opaque)
	{
		const FTexture* Wt = (Cv && Cv->DefaultTexture) ? Cv->DefaultTexture->GetResource() : nullptr;
		if (!T.Num() || !Wt) { T.Reset(); return; }
		FCanvasTriangleItem It(T, Wt);
		It.BlendMode = Mode;
		Cv->DrawItem(It);
		T.Reset();
	}
	/** Light on top of solid shapes: colour is added (pre-multiplied by its alpha), the alpha channel is left alone. */
	void Glow(UCanvas* Cv, FTris& T)
	{
		for (FCanvasUVTri& X : T)
		{
			X.V0_Color = FLinearColor(X.V0_Color.R * X.V0_Color.A, X.V0_Color.G * X.V0_Color.A, X.V0_Color.B * X.V0_Color.A, 0.f);
			X.V1_Color = FLinearColor(X.V1_Color.R * X.V1_Color.A, X.V1_Color.G * X.V1_Color.A, X.V1_Color.B * X.V1_Color.A, 0.f);
			X.V2_Color = FLinearColor(X.V2_Color.R * X.V2_Color.A, X.V2_Color.G * X.V2_Color.A, X.V2_Color.B * X.V2_Color.A, 0.f);
		}
		Flush(Cv, T, SE_BLEND_Additive);
	}
	// Lilita One glyph atlas (Slate fonts do not draw into canvas render targets): ZoogiHoloFontData.inl
	UTexture2D* FontTex()
	{
		static TStrongObjectPtr<UTexture2D> Tex;
		static bool bTried = false;
		if (!Tex.IsValid() && !bTried)
		{
			bTried = true;
			TArray<uint8> Png(ZoogiHoloFontData::AtlasPng, UE_ARRAY_COUNT(ZoogiHoloFontData::AtlasPng));
			if (UTexture2D* T = FImageUtils::ImportBufferAsTexture2D(Png)) Tex.Reset(T);
			UE_LOG(LogZoogi, Log, TEXT("HOLOSPIRE font atlas %s"), Tex.IsValid() ? TEXT("ready") : TEXT("FAILED"));
		}
		return Tex.Get();
	}
	const ZoogiHoloFontData::FG* Glyph(TCHAR C)
	{
		C = FChar::ToUpper(C);
		for (const ZoogiHoloFontData::FG& G : ZoogiHoloFontData::Glyphs) if (G.C == C) return &G;
		return nullptr;
	}
	float MeasureW(const FString& S, float Size)
	{
		const float K = Size / ZoogiHoloFontData::BaseSize;
		float W = 0.f;
		for (const TCHAR C : S) { const ZoogiHoloFontData::FG* G = Glyph(C); W += (G ? G->Adv : ZoogiHoloFontData::SpaceAdv) * K; }
		return W;
	}
	/** Text, added as light (no alpha): Size = roughly the cap height in pixels. Align 0 left of X, 1 centred, 2 right of X. */
	float Text(UCanvas* Cv, const FString& S, float Cx, float Top, float Size, const FLinearColor& Col, int32 Align = 1)
	{
		UTexture2D* Tex = FontTex();
		const float W = MeasureW(S, Size);
		if (!Cv || !Tex || !Tex->GetResource()) return W;
		const float K = Size / ZoogiHoloFontData::BaseSize;
		float X = Align == 1 ? Cx - W * 0.5f : (Align == 0 ? Cx : Cx - W);
		const float Y = Top - 0.12f * Size;
		const FLinearColor C(Col.R * Col.A, Col.G * Col.A, Col.B * Col.A, 0.f);
		const float IW = 1.f / ZoogiHoloFontData::AtlasW, IH = 1.f / ZoogiHoloFontData::AtlasH;
		FTris T;
		for (const TCHAR Ch : S)
		{
			const ZoogiHoloFontData::FG* G = Glyph(Ch);
			if (!G) { X += ZoogiHoloFontData::SpaceAdv * K; continue; }
			const FVector2D P0(X + G->Off * K, Y), P1(P0.X + G->W * K, Y + G->H * K);
			const FVector2D U0(G->X * IW, G->Y * IH), U1((G->X + G->W) * IW, (G->Y + G->H) * IH);
			FCanvasUVTri A, B;
			A.V0_Pos = P0; A.V1_Pos = FVector2D(P1.X, P0.Y); A.V2_Pos = P1;
			A.V0_UV = U0; A.V1_UV = FVector2D(U1.X, U0.Y); A.V2_UV = U1;
			B.V0_Pos = P0; B.V1_Pos = P1; B.V2_Pos = FVector2D(P0.X, P1.Y);
			B.V0_UV = U0; B.V1_UV = U1; B.V2_UV = FVector2D(U0.X, U1.Y);
			A.V0_Color = A.V1_Color = A.V2_Color = B.V0_Color = B.V1_Color = B.V2_Color = C;
			T.Add(A); T.Add(B);
			X += G->Adv * K;
		}
		FCanvasTriangleItem It(T, Tex->GetResource());
		It.BlendMode = SE_BLEND_Additive;
		Cv->DrawItem(It);
		return W;
	}
	/** Adds a whole render target (the text layer) on top, colour only. */
	void Composite(UCanvas* Cv, UTextureRenderTarget2D* Src, const FVector2D& Size)
	{
		if (!Cv || !Src || !Src->GetResource()) return;
		FCanvasTileItem It(FVector2D(0.f, 0.f), Src->GetResource(), Size, FLinearColor(1.f, 1.f, 1.f, 0.f));
		It.BlendMode = SE_BLEND_Additive;
		Cv->DrawItem(It);
	}
	void PortraitDisc(UCanvas* Cv, UTexture2D* Tex, const FVector2D& C, float R, float Alpha = 1.f)
	{
		if (!Tex || !Tex->GetResource()) return;
		FTris T;
		const int32 N = 40;
		for (int32 i = 0; i < N; ++i)
		{
			const float a0 = 2.f * PI * i / N, a1 = 2.f * PI * (i + 1) / N;
			const FVector2D d0(FMath::Cos(a0), FMath::Sin(a0)), d1(FMath::Cos(a1), FMath::Sin(a1));
			FCanvasUVTri X;
			X.V0_Pos = C; X.V1_Pos = C + d0 * R; X.V2_Pos = C + d1 * R;
			X.V0_UV = FVector2D(0.5f, 0.5f); X.V1_UV = FVector2D(0.5f, 0.5f) + d0 * 0.5f; X.V2_UV = FVector2D(0.5f, 0.5f) + d1 * 0.5f;
			X.V0_Color = X.V1_Color = X.V2_Color = FLinearColor(Alpha, Alpha, Alpha, 0.f);
			T.Add(X);
		}
		FCanvasTriangleItem It(T, Tex->GetResource());
		It.BlendMode = SE_BLEND_Additive;
		Cv->DrawItem(It);
	}
	FLinearColor Bright(const FLinearColor& C, float K) { return FLinearColor(FMath::Min(C.R * K, 1.f), FMath::Min(C.G * K, 1.f), FMath::Min(C.B * K, 1.f), C.A); }
	float HoloEase(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }
	FString PortraitKey(FName Id)
	{
		const FString S = Id.ToString();
		if (S == TEXT("orin")) return TEXT("master_orin");
		if (S == TEXT("darkzoogy")) return TEXT("dark_zoogy");
		return S;
	}

	struct FDraw
	{
		UObject* Ctx = nullptr;
		UCanvas* Cv = nullptr;
		FVector2D Size = FVector2D::ZeroVector;
		FDrawToRenderTargetContext RC;
		bool Begin(UObject* InCtx, UTextureRenderTarget2D* RT)
		{
			if (!RT || !InCtx) return false;
			Ctx = InCtx;
			UKismetRenderingLibrary::ClearRenderTarget2D(Ctx, RT, FLinearColor(0.f, 0.f, 0.f, 0.f));
			UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(Ctx, RT, Cv, Size, RC);
			return Cv != nullptr;
		}
		void End() { if (Ctx) UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(Ctx, RC); Cv = nullptr; }
	};
}

// ======================================================================================================== spire actor

AZoogiHoloSpire::AZoogiHoloSpire()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UTextureRenderTarget2D* AZoogiHoloSpire::NewRT(int32 W, int32 H)
{
	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
	RT->RenderTargetFormat = RTF_RGBA8;
	RT->ClearColor = FLinearColor::Black;
	RT->bAutoGenerateMips = false;
	RT->InitAutoFormat(W, H);
	RT->UpdateResourceImmediate(true);
	RTs.Add(RT);
	return RT;
}

static UMaterialInterface* HoloBaseMat()
{
	static TWeakObjectPtr<UMaterialInterface> M;
	if (!M.IsValid()) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/AwakenFX/Materials/M_ZAFX_Add.M_ZAFX_Add"), nullptr, LOAD_NoWarn);
	return M.Get();
}

static UMaterialInterface* HoloColorMat()
{
	// unlit translucent, full colour from the texture, opacity from its alpha (Frozen floor decal material, always cooked)
	static TWeakObjectPtr<UMaterialInterface> M;
	if (!M.IsValid()) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/M_FrozenDecal.M_FrozenDecal"), nullptr, LOAD_NoWarn);
	return M.Get();
}

static UProceduralMeshComponent* NewQuadComp(AActor* Owner, USceneComponent* Parent, const TArray<FVector>& V, const TArray<FVector2D>& UV, const TArray<int32>& Tri, const FVector& Nrm)
{
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Owner);
	P->SetupAttachment(Parent);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->SetCastShadow(false);
	P->bUseAsyncCooking = false;
	P->RegisterComponent();
	TArray<FVector> N; N.Init(Nrm, V.Num());
	TArray<FLinearColor> C; C.Init(FLinearColor::White, V.Num());
	TArray<FProcMeshTangent> Tg;
	P->CreateMeshSection_LinearColor(0, V, Tri, N, UV, C, Tg, false);
	return P;
}

UProceduralMeshComponent* AZoogiHoloSpire::MakeFlat(USceneComponent* Parent, float W, float H, UTextureRenderTarget2D* Tex, const FLinearColor& Tint, float Glow, int32& OutIdx)
{
	// vertical quad facing +X (the camera side of Front); U runs to the viewer's right (-Y), V down
	const TArray<FVector> V = { FVector(0, W * 0.5f, H * 0.5f), FVector(0, -W * 0.5f, H * 0.5f), FVector(0, -W * 0.5f, -H * 0.5f), FVector(0, W * 0.5f, -H * 0.5f) };
	const TArray<FVector2D> UV = { FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1) };
	UProceduralMeshComponent* P = NewQuadComp(this, Parent, V, UV, { 0, 2, 1, 0, 3, 2 }, FVector(1, 0, 0));
	OutIdx = AddPart(P, Tex, Tint, Glow);
	return P;
}

UProceduralMeshComponent* AZoogiHoloSpire::MakeDisc(USceneComponent* Parent, float D, UTextureRenderTarget2D* Tex, const FLinearColor& Tint, float Glow, int32& OutIdx)
{
	const float h = D * 0.5f;
	const TArray<FVector> V = { FVector(-h, -h, 0), FVector(h, -h, 0), FVector(h, h, 0), FVector(-h, h, 0) };
	const TArray<FVector2D> UV = { FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1) };
	UProceduralMeshComponent* P = NewQuadComp(this, Parent, V, UV, { 0, 2, 1, 0, 3, 2 }, FVector(0, 0, 1));
	OutIdx = AddPart(P, Tex, Tint, Glow);
	return P;
}

UProceduralMeshComponent* AZoogiHoloSpire::MakeArc(USceneComponent* Parent, float R, float ArcDeg, float H, UTextureRenderTarget2D* Tex, const FLinearColor& Tint, float Glow, int32& OutIdx)
{
	// curved band around the beam, centred on +X (toward the camera), texture wraps left -> right as the viewer sees it
	TArray<FVector> V; TArray<FVector2D> UV; TArray<int32> T;
	const int32 N = 28;
	const float A = FMath::DegreesToRadians(ArcDeg);
	for (int32 i = 0; i <= N; ++i)
	{
		const float th = -A * 0.5f + A * i / N;
		const FVector P(R * FMath::Cos(th), -R * FMath::Sin(th), 0.f);
		V.Add(P + FVector(0, 0, H * 0.5f)); UV.Add(FVector2D((float)i / N, 0.f));
		V.Add(P - FVector(0, 0, H * 0.5f)); UV.Add(FVector2D((float)i / N, 1.f));
		if (i < N) { const int32 b = i * 2; T.Append({ b, b + 1, b + 2, b + 1, b + 3, b + 2 }); } // front faces toward +X (the camera)
	}
	UProceduralMeshComponent* P = NewQuadComp(this, Parent, V, UV, T, FVector(1, 0, 0));
	OutIdx = AddPart(P, Tex, Tint, Glow);
	return P;
}

int32 AZoogiHoloSpire::AddPart(UProceduralMeshComponent* P, UTextureRenderTarget2D* Tex, const FLinearColor& Tint, float Glow)
{
	// colour panels (Tint.A < 0 marks them) use the colour hologram material, glow shapes the additive one
	const bool bColor = Tint.A < 0.f;
	UMaterialInterface* Base = bColor ? HoloColorMat() : HoloBaseMat();
	UMaterialInstanceDynamic* M = nullptr;
	int32 Kind = 2;
	if (Base)
	{
		M = UMaterialInstanceDynamic::Create(Base, this);
		if (Tex) M->SetTextureParameterValue(TEXT("Tex"), Tex);
		if (bColor)
		{
			Kind = 1;
			M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(1.f, 1.f, 1.f));
			M->SetScalarParameterValue(TEXT("Bright"), 1.1f);
			M->SetVectorParameterValue(TEXT("EmissiveTint"), FLinearColor(0.55f, 0.85f, 1.f));
			M->SetScalarParameterValue(TEXT("Emissive"), 0.45f);
			M->SetScalarParameterValue(TEXT("Opacity"), 0.95f);
		}
		else { Kind = 0; M->SetVectorParameterValue(TEXT("Tint"), Tint); M->SetScalarParameterValue(TEXT("Glow"), Glow); }
	}
	else M = ZoogiUtil::FxMID(this, FLinearColor(FMath::Abs(Tint.R), FMath::Abs(Tint.G), FMath::Abs(Tint.B)), Glow);
	P->SetMaterial(0, M);
	Mids.Add(M);
	MatKind.Add(Kind);
	BaseGlow.Add(FMath::Max(Glow, 0.01f));
	return Quads.Add(P);
}

void AZoogiHoloSpire::SetGlow(int32 Idx, float Glow)
{
	if (!Mids.IsValidIndex(Idx) || !Quads.IsValidIndex(Idx)) return;
	if (UMaterialInstanceDynamic* M = Mids[Idx])
	{
		const int32 K = MatKind.IsValidIndex(Idx) ? MatKind[Idx] : 0;
		if (K == 1) M->SetScalarParameterValue(TEXT("Opacity"), 0.95f * FMath::Clamp(Glow / BaseGlow[Idx], 0.f, 1.f));
		else M->SetScalarParameterValue(K == 0 ? FName(TEXT("Glow")) : FName(TEXT("Strength")), Glow);
	}
	if (UProceduralMeshComponent* Q = Quads[Idx]) Q->SetVisibility(Glow > 0.02f);
}

void AZoogiHoloSpire::ExportDebug(const FString& Dir)
{
	const TPair<const TCHAR*, UTextureRenderTarget2D*> L[] = { { TEXT("rt_score"), RT_Score }, { TEXT("rt_band"), RT_Band }, { TEXT("rt_cardl"), RT_CardL }, { TEXT("rt_aim"), RT_Aim }, { TEXT("rt_ring"), RT_Ring }, { TEXT("rt_scoretxt"), RT_ScoreTxt }, { TEXT("rt_aimtxt"), RT_AimTxt } };
	for (const auto& E : L) if (E.Value) UKismetRenderingLibrary::ExportRenderTarget(this, E.Value, Dir, FString(E.Key) + TEXT(".png"));
	UE_LOG(LogZoogi, Log, TEXT("HOLOSPIRE exported render targets to %s"), *Dir);
}

void AZoogiHoloSpire::SetTex(int32 Idx, UTextureRenderTarget2D* Tex)
{
	if (Mids.IsValidIndex(Idx) && Mids[Idx] && Tex) Mids[Idx]->SetTextureParameterValue(TEXT("Tex"), Tex);
}

UTexture2D* AZoogiHoloSpire::Portrait(FName Id)
{
	if (TObjectPtr<UTexture2D>* Hit = Portraits.Find(Id)) return Hit->Get();
	UTexture2D* T = nullptr;
	for (const TCHAR* E : { TEXT(".png"), TEXT(".jpg") })
	{
		const FString P = FPaths::ProjectContentDir() / TEXT("Data/MenuArt") / (ZoogiHolo::PortraitKey(Id) + E);
		if (!T && FPaths::FileExists(P)) T = FImageUtils::ImportFileAsTexture2D(P);
	}
	Portraits.Add(Id, T);
	return T;
}

void AZoogiHoloSpire::Build(float InScale)
{
	using namespace ZoogiHolo;
	S = InScale;
	SetActorScale3D(FVector(S));
	RT_Base = NewRT(512, 512); RT_Ring = NewRT(512, 512); RT_Chevron = NewRT(256, 256); RT_Star = NewRT(128, 128);
	RT_Score = NewRT(1024, 512); RT_Band = NewRT(1024, 512); RT_CardL = NewRT(256, 384); RT_CardR = NewRT(256, 384); RT_Aim = NewRT(1024, 704);
	RT_ScoreTxt = NewRT(1024, 512); RT_CardLTxt = NewRT(256, 384); RT_CardRTxt = NewRT(256, 384); RT_AimTxt = NewRT(1024, 704);

	// emitter on the floor
	int32 I = -1;
	MakeDisc(Root, 340.f, RT_Base, Cyan, 1.4f, QBase)->SetRelativeLocation(FVector(0, 0, 4.f));
	MakeDisc(Root, 430.f, RT_Ring, Gold, 0.9f, QBase2)->SetRelativeLocation(FVector(0, 0, 6.f));
	// beam + sheath
	Beam = NewObject<UStaticMeshComponent>(this);
	Beam->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cylinder")));
	Beam->SetupAttachment(Root);
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetCastShadow(false);
	Beam->RegisterComponent();
	Beam->SetRelativeLocation(FVector(0, 0, 400.f));
	Beam->SetRelativeScale3D(FVector(0.2f, 0.2f, 7.9f));
	BeamMID = ZoogiUtil::FxMID(this, Cyan, 2.f);
	Beam->SetMaterial(0, BeamMID);
	Sheath = NewObject<UStaticMeshComponent>(this);
	Sheath->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cylinder")));
	Sheath->SetupAttachment(Root);
	Sheath->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sheath->SetCastShadow(false);
	Sheath->RegisterComponent();
	Sheath->SetRelativeLocation(FVector(0, 0, 330.f));
	Sheath->SetRelativeScale3D(FVector(0.75f, 0.75f, 6.4f));
	SheathMID = ZoogiUtil::FxMID(this, Cyan, 0.12f);
	Sheath->SetMaterial(0, SheathMID);
	// rotating energy rings up the beam (+ two small ones in the aim mode)
	struct FR { float Z, D; FLinearColor C; float G, Sp; };
	const FR Rs[] = { { 165.f, 250.f, Gold, 1.1f, 40.f }, { 300.f, 190.f, Cyan, 1.2f, -65.f }, { 395.f, 230.f, Gold, 0.8f, 30.f }, { 610.f, 300.f, Cyan, 0.9f, -25.f }, { 700.f, 210.f, Gold, 0.9f, 55.f } };
	for (const FR& R : Rs) { MakeDisc(Root, R.D, RT_Ring, R.C, R.G, I)->SetRelativeLocation(FVector(0, 0, R.Z)); QRings.Add(I); RingSpeed.Add(R.Sp); }
	// top orb + tilted orbit rings
	TopOrb = NewObject<UStaticMeshComponent>(this);
	TopOrb->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
	TopOrb->SetupAttachment(Root);
	TopOrb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TopOrb->SetCastShadow(false);
	TopOrb->RegisterComponent();
	TopOrb->SetRelativeLocation(FVector(0, 0, 850.f));
	TopOrb->SetRelativeScale3D(FVector(0.95f));
	TopOrb->SetMaterial(0, ZoogiUtil::GlowMID(this, Cyan, 3.f));
	for (int32 k = 0; k < 2; ++k)
	{
		USceneComponent* Pv = NewObject<USceneComponent>(this);
		Pv->SetupAttachment(Root);
		Pv->RegisterComponent();
		Pv->SetRelativeLocation(FVector(0, 0, 850.f));
		OrbRingPivots.Add(Pv);
		MakeDisc(Pv, k == 0 ? 190.f : 160.f, RT_Ring, k == 0 ? Gold : Cyan, 1.1f, I);
		Quads[I]->SetRelativeRotation(FRotator(k == 0 ? 18.f : -12.f, 0.f, k == 0 ? 12.f : -20.f));
		QOrbRings.Add(I);
	}
	// camera-facing group
	Front = NewObject<USceneComponent>(this);
	Front->SetupAttachment(Root);
	Front->RegisterComponent();
	for (int32 k = 0; k < 3; ++k) { MakeFlat(Front, 70.f, 56.f, RT_Chevron, Cyan, 1.4f, I); Quads[I]->SetRelativeLocation(FVector(26.f, 0, 200.f)); QChev.Add(I); }
	MakeFlat(Front, 60.f, 60.f, RT_Star, Gold, 1.6f, QStar);
	DrumPivot = NewObject<USceneComponent>(this);
	DrumPivot->SetupAttachment(Front);
	DrumPivot->RegisterComponent();
	DrumPivot->SetRelativeLocation(FVector(0, 0, 505.f));
	MakeArc(DrumPivot, 150.f, 150.f, 196.f, RT_Score, FLinearColor(1.f, 1.f, 1.f, -1.f), 1.f, QDrum);
	CardLPivot = NewObject<USceneComponent>(this); CardLPivot->SetupAttachment(Front); CardLPivot->RegisterComponent();
	CardRPivot = NewObject<USceneComponent>(this); CardRPivot->SetupAttachment(Front); CardRPivot->RegisterComponent();
	MakeFlat(CardLPivot, 96.f, 144.f, RT_CardL, FLinearColor(1.f, 1.f, 1.f, -1.f), 1.f, QCardL);
	MakeFlat(CardRPivot, 96.f, 144.f, RT_CardR, FLinearColor(1.f, 1.f, 1.f, -1.f), 1.f, QCardR);
	AimPivot = NewObject<USceneComponent>(this);
	AimPivot->SetupAttachment(Front);
	AimPivot->RegisterComponent();
	AimPivot->SetRelativeLocation(FVector(40.f, 0, 790.f));
	MakeFlat(AimPivot, 540.f, 371.f, RT_Aim, FLinearColor(1.f, 1.f, 1.f, -1.f), 1.f, QAim);
	DrawStatic();
	DrawAimText(); // warm the title glyphs
	bBuilt = true;
	UE_LOG(LogZoogi, Log, TEXT("HOLOSPIRE built at (%.0f,%.0f,%.0f) scale %.2f, %d hologram parts, material %s"), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, S, Quads.Num(), HoloBaseMat() ? TEXT("M_ZAFX_Add") : TEXT("FX fallback"));
}

// ------------------------------------------------------------------------------------------------ textures

void AZoogiHoloSpire::DrawStatic()
{
	using namespace ZoogiHolo;
	FDraw D; FTris T;
	if (D.Begin(this, RT_Base))
	{
		const FVector2D C(256.f, 256.f);
		Disc(T, C, 200.f, FLinearColor(0.08f, 0.25f, 0.45f, 1.f), 64, &Navy);
		Ring(T, C, 240.f, 250.f, Cyan); Ring(T, C, 196.f, 203.f, Cyan); Ring(T, C, 120.f, 126.f, Bright(Cyan, 0.8f));
		for (int32 i = 0; i < 36; ++i) { const float a = 2.f * PI * i / 36.f; const FVector2D d(FMath::Cos(a), FMath::Sin(a)); Seg(T, C + d * 210.f, C + d * (i % 3 == 0 ? 236.f : 226.f), i % 3 == 0 ? 6.f : 3.f, Cyan); }
		Star(T, C, 96.f, 0.f, Gold);
		Star(T, C, 60.f, 0.f, FLinearColor(1.f, 0.95f, 0.7f));
		Flush(D.Cv, T);
		D.End();
	}
	if (D.Begin(this, RT_Ring))
	{
		const FVector2D C(256.f, 256.f);
		for (int32 k = 0; k < 8; ++k) { const float a0 = 2.f * PI * k / 8.f + 0.06f, a1 = 2.f * PI * (k + 1) / 8.f - 0.06f; Ring(T, C, 228.f, 250.f, White, 12, a0, a1); }
		Ring(T, C, 210.f, 215.f, FLinearColor(0.7f, 0.7f, 0.7f));
		for (int32 i = 0; i < 48; ++i) { const float a = 2.f * PI * i / 48.f; Disc(T, C + FVector2D(FMath::Cos(a), FMath::Sin(a)) * 198.f, 3.f, FLinearColor(0.8f, 0.8f, 0.8f), 8); }
		Flush(D.Cv, T);
		D.End();
	}
	if (D.Begin(this, RT_Chevron))
	{
		for (int32 k = 0; k < 2; ++k)
		{
			const float y = 92.f + 86.f * k;
			Seg(T, FVector2D(40.f, y + 50.f), FVector2D(128.f, y - 20.f), 26.f, White);
			Seg(T, FVector2D(128.f, y - 20.f), FVector2D(216.f, y + 50.f), 26.f, White);
		}
		Flush(D.Cv, T);
		D.End();
	}
	if (D.Begin(this, RT_Star))
	{
		Disc(T, FVector2D(64.f, 64.f), 60.f, FLinearColor(0.35f, 0.25f, 0.05f), 32, &FLinearColor::Black);
		Star(T, FVector2D(64.f, 64.f), 56.f, 0.f, White);
		Flush(D.Cv, T);
		D.End();
	}
}

FString AZoogiHoloSpire::ScoreSig(AZoogiGameMode* G) const
{
	FString Sg = FString::Printf(TEXT("%d/%d a%d o%d:"), G->RoundNumber, G->MenuRounds, G->ActivePlayer, G->Orbs.Num());
	for (const AZoogiBall* B : G->Balls) if (B) Sg += FString::Printf(TEXT("%s,%d,%d,%d,%d;"), *B->Def.Id.ToString(), B->OrbsScored, B->Score, B->bOut ? 1 : 0, B->bKnockedOut ? 1 : 0);
	if (const UZoogiFunSys* Fun = UZoogiFunSys::Get(G)) Sg += Fun->ClashOn() ? TEXT("C") : TEXT("-"); // build 27: redraw for the POWER CLASH banner
	return Sg;
}

void AZoogiHoloSpire::DrawScore(AZoogiGameMode* G)
{
	using namespace ZoogiHolo;
	const int32 N = FMath::Max(1, G->Balls.Num());
	const float Gap = 16.f, X0 = 24.f, Wd = (1024.f - 2.f * X0 - Gap * (N - 1)) / N, Y0 = 122.f, Ht = 372.f;
	const int32 TotalOrbs = FMath::Max(1, G->Orbs.Num());
	const bool bWide = Wd > 300.f;
	auto Geo = [&](int32 i, float& X, float& PR, FVector2D& PCn)
	{
		X = X0 + i * (Wd + Gap);
		PR = bWide ? FMath::Min(80.f, Ht * 0.24f) : FMath::Min(Wd * 0.3f, 66.f);
		PCn = bWide ? FVector2D(X + 30.f + PR, Y0 + 40.f + PR) : FVector2D(X + Wd * 0.5f, Y0 + 26.f + PR);
	};
	// text layer
	FDraw Tx;
	if (Tx.Begin(this, RT_ScoreTxt))
	{
		const UZoogiFunSys* ClashFun = UZoogiFunSys::Get(G);
		if (ClashFun && ClashFun->ClashOn()) Text(Tx.Cv, TEXT("POWER CLASH!"), 512.f, 18.f, 44.f, FLinearColor(1.f, 0.5f, 0.95f)); // build 27: live on the scoreboard
		else Text(Tx.Cv, FString::Printf(TEXT("ROUND %d / %d"), FMath::Clamp(G->RoundNumber, 1, FMath::Max(1, G->MenuRounds)), G->MenuRounds), 512.f, 18.f, 40.f, White);
		for (int32 i = 0; i < G->Balls.Num(); ++i)
		{
			const AZoogiBall* B = G->Balls[i];
			if (!B) continue;
			float X, PR; FVector2D PCn; Geo(i, X, PR, PCn);
			const bool bOut = B->bOut || B->bKnockedOut;
			const FString PctS = FString::Printf(TEXT("%d%%"), FMath::RoundToInt(100.f * B->OrbsScored / TotalOrbs));
			const FString Nm = bOut ? FString(TEXT("OUT")) : B->Def.Name.ToUpper();
			const FLinearColor NC = bOut ? FLinearColor(1.f, 0.4f, 0.4f) : Bright(Cyan, 1.1f);
			if (bWide) { Text(Tx.Cv, PctS, X + Wd - 40.f, Y0 + 40.f, 84.f, White, 2); Text(Tx.Cv, Nm, X + Wd - 40.f, Y0 + 160.f, 30.f, NC, 2); }
			else { Text(Tx.Cv, PctS, X + Wd * 0.5f, PCn.Y + PR + 18.f, 60.f, White); Text(Tx.Cv, Nm, X + Wd * 0.5f, PCn.Y + PR + 100.f, 24.f, NC); }
		}
		Tx.End();
	}
	FDraw D; FTris T;
	if (!D.Begin(this, RT_Score)) return;
	UCanvas* Cv = D.Cv;
	// solid: ROUND tab, divider, one panel per Zoogi (gold rim on the shooter)
	Panel(T, FVector2D(322.f, 8.f), FVector2D(380.f, 80.f), 24.f, 4.f, Cyan, Navy);
	Seg(T, FVector2D(40.f, 104.f), FVector2D(984.f, 104.f), 3.f, Bright(Cyan, 0.6f));
	for (int32 i = 0; i < G->Balls.Num(); ++i)
	{
		const AZoogiBall* B = G->Balls[i];
		if (!B) continue;
		const bool bActive = i == G->ActivePlayer;
		float X, PR; FVector2D PCn; Geo(i, X, PR, PCn);
		Panel(T, FVector2D(X, Y0), FVector2D(Wd, Ht), 28.f, bActive ? 7.f : 4.f, bActive ? Gold : Cyan, FLinearColor(0.02f, 0.07f, 0.18f));
		Disc(T, PCn, PR + 16.f, FLinearColor(0.03f, 0.08f, 0.18f));
		const float BX = X + 26.f, BW = Wd - 52.f, BY = Y0 + Ht - 64.f;
		Panel(T, FVector2D(BX, BY), FVector2D(BW, 30.f), 15.f, 3.f, Bright(Cyan, 0.6f), FLinearColor(0.03f, 0.08f, 0.16f));
	}
	Flush(Cv, T);
	// light: portrait rings, bars, portraits, text
	for (int32 i = 0; i < G->Balls.Num(); ++i)
	{
		const AZoogiBall* B = G->Balls[i];
		if (!B) continue;
		float X, PR; FVector2D PCn; Geo(i, X, PR, PCn);
		const FLinearColor PC = Bright(B->Def.Color, 1.15f);
		Ring(T, PCn, PR + 2.f, PR + 10.f, PC); Ring(T, PCn, PR + 12.f, PR + 15.f, Bright(Cyan, 0.7f));
		const float F = FMath::Clamp((float)B->OrbsScored / TotalOrbs, 0.f, 1.f);
		const float BX = X + 26.f, BW = Wd - 52.f, BY = Y0 + Ht - 64.f;
		if (F > 0.f) Poly(T, RRectPts(FVector2D(BX + 4.f, BY + 4.f), FVector2D(FMath::Max(16.f, (BW - 8.f) * F), 22.f), 11.f), PC);
	}
	Glow(Cv, T);
	for (int32 i = 0; i < G->Balls.Num(); ++i)
	{
		const AZoogiBall* B = G->Balls[i];
		if (!B) continue;
		float X, PR; FVector2D PCn; Geo(i, X, PR, PCn);
		PortraitDisc(Cv, Portrait(B->Def.Id), PCn, PR, (B->bOut || B->bKnockedOut) ? 0.4f : 1.f);
	}
	Composite(Cv, RT_ScoreTxt, D.Size);
	D.End();
}

void AZoogiHoloSpire::DrawCards(AZoogiGameMode* G, float Tm)
{
	using namespace ZoogiHolo;
	const int32 N = G->Balls.Num();
	const int32 TotalOrbs = FMath::Max(1, G->Orbs.Num());
	int32 MaxScore = 1, Leader = -1;
	for (int32 i = 0; i < N; ++i) if (const AZoogiBall* B = G->Balls[i]) if (B->Score >= MaxScore) { MaxScore = B->Score; Leader = i; }
	const FString Left = FString::Printf(TEXT("%d LEFT"), G->OrbsLeft());
	if (Left != CardTxtSig) { CardTxtSig = Left; CardTxtRedraw = 4; }
	if (CardTxtRedraw > 0)
	{
		--CardTxtRedraw;
		FDraw Tx;
		if (Tx.Begin(this, RT_CardLTxt)) { Text(Tx.Cv, Left, 128.f, 168.f, 30.f, White); Tx.End(); }
		if (Tx.Begin(this, RT_CardRTxt)) { Text(Tx.Cv, TEXT("LEADER"), 128.f, 168.f, 30.f, Gold); Tx.End(); }
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FDraw D; FTris T;
		if (!D.Begin(this, Side == 0 ? RT_CardL : RT_CardR)) continue;
		Panel(T, FVector2D(6.f, 6.f), FVector2D(244.f, 372.f), 22.f, 4.f, Cyan, Navy);
		Flush(D.Cv, T);
		const FVector2D IC(128.f, 100.f);
		if (Side == 0)
		{
			Ring(T, IC, 52.f, 60.f, Cyan); Ring(T, IC, 26.f, 31.f, Cyan);
			Seg(T, IC - FVector2D(76.f, 0.f), IC - FVector2D(36.f, 0.f), 6.f, Cyan); Seg(T, IC + FVector2D(36.f, 0.f), IC + FVector2D(76.f, 0.f), 6.f, Cyan);
			Seg(T, IC - FVector2D(0.f, 76.f), IC - FVector2D(0.f, 36.f), 6.f, Cyan); Seg(T, IC + FVector2D(0.f, 36.f), IC + FVector2D(0.f, 76.f), 6.f, Cyan);
			Disc(T, IC, 8.f, White, 16);
		}
		else
		{
			TArray<FVector2D> Cr = { IC + FVector2D(-62.f, 40.f), IC + FVector2D(-70.f, -30.f), IC + FVector2D(-34.f, 4.f), IC + FVector2D(0.f, -50.f), IC + FVector2D(34.f, 4.f), IC + FVector2D(70.f, -30.f), IC + FVector2D(62.f, 40.f) };
			for (int32 k = 0; k + 1 < Cr.Num(); ++k) Seg(T, Cr[k], Cr[k + 1], 9.f, Gold);
			Seg(T, Cr.Last(), Cr[0], 9.f, Gold);
			for (const FVector2D& P : { Cr[1], Cr[3], Cr[5] }) Disc(T, P, 9.f, Gold, 16);
			Seg(T, IC + FVector2D(-60.f, 54.f), IC + FVector2D(60.f, 54.f), 8.f, Gold);
		}
		const float BW = FMath::Min(44.f, 190.f / FMath::Max(1, N) - 10.f), Base = 350.f;
		for (int32 i = 0; i < N; ++i)
		{
			const AZoogiBall* B = G->Balls[i];
			if (!B) continue;
			const float V = Side == 0 ? (float)B->OrbsScored / TotalOrbs : (float)B->Score / MaxScore;
			const float H = (24.f + 120.f * FMath::Clamp(V * (Side == 0 ? 3.f : 1.f), 0.f, 1.f)) * (0.92f + 0.08f * FMath::Sin(Tm * 3.f + i * 1.7f));
			const float X = 128.f - (N * (BW + 10.f) - 10.f) * 0.5f + i * (BW + 10.f);
			Poly(T, RRectPts(FVector2D(X, Base - H), FVector2D(BW, H), 6.f), Bright(B->Def.Color, 1.15f));
			if (Side == 1 && i == Leader && B->Score > 0) Poly(T, RRectPts(FVector2D(X - 3.f, Base - H - 12.f), FVector2D(BW + 6.f, 6.f), 3.f), Gold);
		}
		Seg(T, FVector2D(26.f, Base + 6.f), FVector2D(230.f, Base + 6.f), 3.f, Bright(Cyan, 0.7f));
		Glow(D.Cv, T);
		Composite(D.Cv, Side == 0 ? RT_CardLTxt : RT_CardRTxt, D.Size);
		D.End();
	}
}

void AZoogiHoloSpire::DrawBand(AZoogiGameMode* G)
{
	using namespace ZoogiHolo;
	FDraw D; FTris T;
	if (!D.Begin(this, RT_Band)) return;
	TArray<const AZoogiBall*> Who;
	for (int32 i = 0; i < G->Balls.Num(); ++i) if (G->Balls[i] && i != G->ActivePlayer) Who.Add(G->Balls[i]);
	if (Who.Num() == 0) for (const AZoogiBall* B : G->Balls) if (B) Who.Add(B);
	const int32 M = FMath::Max(1, Who.Num());
	const float R = FMath::Min(120.f, 1024.f / M * 0.34f);
	// solid: the energy lines and the portrait sockets
	Seg(T, FVector2D(20.f, 256.f), FVector2D(1004.f, 256.f), 6.f, Bright(Cyan, 0.8f));
	Seg(T, FVector2D(20.f, 92.f), FVector2D(1004.f, 92.f), 4.f, Bright(Cyan, 0.5f));
	Seg(T, FVector2D(20.f, 420.f), FVector2D(1004.f, 420.f), 4.f, Bright(Cyan, 0.5f));
	TArray<FVector2D> Cs;
	for (int32 k = 0; k < Who.Num(); ++k)
	{
		const FVector2D C(1024.f * (k + 0.5f) / M, 256.f);
		Cs.Add(C);
		Disc(T, C, R + 26.f, Cyan);
		Disc(T, C, R + 21.f, FLinearColor(0.01f, 0.04f, 0.1f));
		Disc(T, C, R + 16.f, Bright(Who[k]->Def.Color, 1.2f));
		Disc(T, C, R + 4.f, FLinearColor(0.02f, 0.05f, 0.1f));
	}
	Flush(D.Cv, T);
	for (int32 k = 0; k < Who.Num(); ++k) PortraitDisc(D.Cv, Portrait(Who[k]->Def.Id), Cs[k], R);
	D.End();
}

void AZoogiHoloSpire::DrawAimText()
{
	using namespace ZoogiHolo;
	FDraw Tx;
	if (!Tx.Begin(this, RT_AimTxt)) return;
	const float W1 = MeasureW(TEXT("AIM & "), 92.f), W2 = MeasureW(TEXT("LAUNCH"), 92.f);
	const float X = 512.f - (W1 + W2) * 0.5f;
	Text(Tx.Cv, TEXT("AIM & "), X, 38.f, 92.f, White, 0);
	Text(Tx.Cv, TEXT("LAUNCH"), X + W1, 38.f, 92.f, Gold, 0);
	Text(Tx.Cv, TEXT("DRAG TO LAUNCH"), 512.f, 168.f, 44.f, Bright(Cyan, 1.2f));
	Tx.End();
}

void AZoogiHoloSpire::DrawAim(float Tm, float Reveal)
{
	using namespace ZoogiHolo;
	if (AimTextRedraw > 0) { --AimTextRedraw; DrawAimText(); }
	FDraw D; FTris T;
	if (!D.Begin(this, RT_Aim)) return;
	UCanvas* Cv = D.Cv;
	// solid: the panel (cut corners), its rim, the pointer down to the beam
	const TArray<FVector2D> Fr = { FVector2D(56, 20), FVector2D(968, 20), FVector2D(1004, 56), FVector2D(1004, 548), FVector2D(968, 584), FVector2D(56, 584), FVector2D(20, 548), FVector2D(20, 56) };
	Poly(T, Fr, FLinearColor(0.015f, 0.05f, 0.13f));
	for (int32 k = 0; k < Fr.Num(); ++k) Seg(T, Fr[k], Fr[(k + 1) % Fr.Num()], 6.f, Cyan);
	for (int32 k = 0; k < Fr.Num(); ++k) Disc(T, Fr[k], 3.f, Cyan, 10);
	Poly(T, { FVector2D(462, 590), FVector2D(562, 590), FVector2D(512, 652) }, Bright(Cyan, 0.85f));
	Seg(T, FVector2D(512, 652), FVector2D(512, 704), 6.f, Cyan);
	Flush(Cv, T);
	// light: brackets, sparkles, title rules
	Seg(T, FVector2D(44, 76), FVector2D(44, 140), 4.f, Bright(Cyan, 0.6f)); Seg(T, FVector2D(980, 76), FVector2D(980, 140), 4.f, Bright(Cyan, 0.6f));
	Star(T, FVector2D(906.f, 74.f), 14.f + 4.f * FMath::Sin(Tm * 6.f), Tm, Gold);
	Star(T, FVector2D(118.f, 150.f), 9.f + 3.f * FMath::Sin(Tm * 5.f + 1.f), -Tm, White);
	Seg(T, FVector2D(170, 196), FVector2D(300, 196), 4.f, Bright(Cyan, 0.8f)); Seg(T, FVector2D(724, 196), FVector2D(854, 196), 4.f, Bright(Cyan, 0.8f));
	// demo: hand pulls the ball back, the arrow grows the other way, release, the ball flies
	const float U = FMath::Frac(Tm / 2.6f);
	const FVector2D B0(400.f, 430.f), Pull(-80.f, 60.f);
	float Drag = 0.f, Fly = -1.f, HandA = 1.f, ArrowA = 1.f, BallA = 1.f;
	if (U < 0.16f) { Drag = 0.f; HandA = HoloEase(U / 0.16f); }
	else if (U < 0.52f) Drag = HoloEase((U - 0.16f) / 0.36f);
	else if (U < 0.6f) Drag = 1.f;
	else if (U < 0.9f) { Drag = 1.f; Fly = (U - 0.6f) / 0.3f; HandA = 1.f - HoloEase(Fly * 2.f); ArrowA = 1.f - HoloEase(Fly * 1.6f); }
	else { HandA = 0.f; ArrowA = 0.f; BallA = HoloEase((U - 0.9f) / 0.1f); }
	const FVector2D Dir = (-Pull).GetSafeNormal();
	FVector2D BallP = B0 + Pull * Drag;
	if (Fly >= 0.f) { BallP = B0 + Pull + Dir * (HoloEase(Fly) * 520.f); BallA = 1.f - HoloEase((Fly - 0.6f) / 0.4f); }
	Ring(T, B0 + FVector2D(0.f, 52.f), 70.f, 76.f, Bright(Cyan, 0.7f), 48);
	Ring(T, B0 + FVector2D(0.f, 52.f), 100.f, 104.f, Bright(Cyan, 0.4f), 48);
	if (ArrowA > 0.f && Drag > 0.f)
	{
		const FVector2D A0 = B0 + Pull * Drag + Dir * 52.f;
		const FVector2D A1 = A0 + Dir * (90.f + 280.f * Drag);
		const FLinearColor C0(Orange.R, Orange.G, Orange.B, 0.35f * ArrowA), C1(Gold.R, Gold.G, Gold.B, ArrowA);
		Seg(T, A0, A1, 30.f, C0, &C1);
		const FVector2D Nn(-Dir.Y, Dir.X);
		Poly(T, { A1 + Nn * 40.f, A1 + Dir * 64.f, A1 - Nn * 40.f }, C1);
		Seg(T, A0, A1, 9.f, FLinearColor(1.f, 0.97f, 0.8f, ArrowA));
	}
	if (Fly >= 0.f) for (int32 k = 1; k <= 6; ++k) Disc(T, BallP - Dir * (k * 26.f), 36.f - k * 4.f, FLinearColor(0.3f, 0.7f, 1.f, 0.4f * (1.f - k / 7.f) * BallA), 24);
	if (BallA > 0.f) Disc(T, BallP, 54.f, FLinearColor(0.2f, 0.6f, 1.f, 0.45f * BallA), 32);
	Glow(Cv, T);
	const FLinearColor PanelC(0.015f, 0.05f, 0.13f, 1.f);
	auto Fade = [&](const FLinearColor& C, float A) { FLinearColor R = FMath::Lerp(PanelC, C, FMath::Clamp(A, 0.f, 1.f)); R.A = 1.f; return R; };
	if (BallA > 0.f)
	{
		Disc(T, BallP, 46.f, Fade(FLinearColor(0.6f, 0.95f, 1.f), BallA), 40);
		Disc(T, BallP, 40.f, Fade(FLinearColor(0.12f, 0.4f, 0.95f), BallA), 32);
		Flush(Cv, T);
		Disc(T, BallP + FVector2D(-12.f, -14.f), 16.f, FLinearColor(0.85f, 0.95f, 1.f, 0.85f * BallA), 20);
		Glow(Cv, T);
	}
	if (HandA > 0.f)
	{
		const FVector2D Tip = (U < 0.16f) ? FMath::Lerp(B0 + FVector2D(160.f, 110.f), B0 + FVector2D(18.f, 22.f), HoloEase(U / 0.16f)) : B0 + Pull * Drag + FVector2D(18.f, 22.f) + Dir * (Fly > 0.f ? 60.f * Fly : 0.f);
		const float Ang = -0.62f, Sc = 1.1f;
		auto X = [&](const FVector2D& P) { return Tip + Rot(P * Sc, Ang); };
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const float Gr = Pass == 0 ? 6.f : 0.f;
			const FLinearColor C = Fade(Pass == 0 ? FLinearColor(0.45f, 0.9f, 1.f) : FLinearColor(0.1f, 0.32f, 0.62f), HandA);
			Capsule(T, X(FVector2D(0, 0)), X(FVector2D(0, 70)), 14.f + Gr, C);
			Capsule(T, X(FVector2D(-26, 98)), X(FVector2D(-38, 74)), 12.f + Gr, C);
			TArray<FVector2D> Palm = RRectPts(FVector2D(-20.f - Gr, 66.f - Gr), FVector2D(66.f + 2.f * Gr, 76.f + 2.f * Gr), 20.f + Gr);
			for (FVector2D& P : Palm) P = X(P);
			Poly(T, Palm, C);
			for (int32 k = 0; k < 3; ++k) Disc(T, X(FVector2D(22.f + 15.f * k, 74.f + 4.f * k)), 12.f - k + Gr, C, 16);
			TArray<FVector2D> Cuff = RRectPts(FVector2D(-14.f - Gr, 140.f - Gr), FVector2D(56.f + 2.f * Gr, 30.f + 2.f * Gr), 8.f);
			for (FVector2D& P : Cuff) P = X(P);
			Poly(T, Cuff, Pass == 0 ? C : Fade(FLinearColor(0.12f, 0.55f, 0.85f), HandA));
			Flush(Cv, T); // solid: the fill pass covers the inside of the outline pass
		}
		if (Drag > 0.f && Fly < 0.f)
		{
			const float Fr2 = FMath::Frac(Tm * 2.f);
			Ring(T, X(FVector2D(0, 0)), 18.f + 12.f * Fr2, 22.f + 12.f * Fr2, FLinearColor(1.f, 1.f, 1.f, 0.7f * (1.f - Fr2)), 24);
		}
	}
	Glow(Cv, T);
	Composite(Cv, RT_AimTxt, D.Size);
	// holographic build-in: below the scan line nothing yet (colour and alpha cleared), a hot line at the edge
	if (Reveal < 0.999f)
	{
		const float Y = FMath::Clamp(Reveal, 0.f, 1.f) * 704.f;
		Poly(T, { FVector2D(0, Y), FVector2D(1024, Y), FVector2D(1024, 704), FVector2D(0, 704) }, FLinearColor(0.f, 0.f, 0.f, 0.f));
		Flush(Cv, T, SE_BLEND_Opaque);
		Seg(T, FVector2D(20, Y), FVector2D(1004, Y), 6.f, FLinearColor(0.7f, 1.f, 1.f, 1.f));
		Flush(Cv, T, SE_BLEND_Opaque);
	}
	D.End();
}


// ------------------------------------------------------------------------------------------------ tick

namespace ZoogiCritSpire
{
	bool Enabled() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiCritTop")); return b; }
	static UMediaTexture* Clip(const UWorld* W)
	{
		if (!Enabled() || !W) return nullptr;
		const UZoogiAnimeSubsystem* An = W->GetSubsystem<UZoogiAnimeSubsystem>();
		if (!An || !An->bActive || !An->HasMovie() || An->Current.Event != FName(TEXT("Critical")) || !An->MovieTexture) return nullptr;
		return An->MovieTexture.Get();
	}
	bool On(const UWorld* W)
	{
		if (!Clip(W)) return false;
		const UZoogiHoloSpireSys* Ss = W->GetSubsystem<UZoogiHoloSpireSys>();
		return Ss && Ss->Spire.IsValid();
	}
}

void AZoogiHoloSpire::Tick(float Dt)
{
	using namespace ZoogiHolo;
	Super::Tick(Dt);
	if (!bBuilt) return;
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G) return;
	Time += Dt;
	// face the camera (yaw only, smoothed)
	if (APlayerController* PC = W->GetFirstPlayerController())
		if (PC->PlayerCameraManager)
		{
			const FVector To = PC->PlayerCameraManager->GetCameraLocation() - GetActorLocation();
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
			if (!bFaceInit) { FaceYaw = Yaw; bFaceInit = true; }
			FaceYaw = FMath::FixedTurn(FaceYaw, Yaw, FMath::Max(60.f, FMath::Abs(FMath::FindDeltaAngleDegrees(FaceYaw, Yaw)) * 6.f) * Dt);
			Front->SetRelativeRotation(FRotator(0.f, FaceYaw, 0.f));
		}
	// morph
	const float Target = bWantAim ? 1.f : 0.f;
	Morph = FMath::FInterpConstantTo(Morph, Target, Dt, 1.f / 0.9f);
	const float A = FMath::Clamp(Morph / Split, 0.f, 1.f), B = FMath::Clamp((Morph - Split) / (1.f - Split), 0.f, 1.f);
	const float Surge = FMath::Sin(PI * Morph);
	if (BeamMID) BeamMID->SetScalarParameterValue(TEXT("Strength"), 2.f + 5.f * Surge + 0.4f * FMath::Sin(Time * 3.1f));
	if (SheathMID) SheathMID->SetScalarParameterValue(TEXT("Strength"), 0.12f + 0.4f * Surge + 0.05f * FMath::Sin(Time * 2.f));
	// rings / emitter
	for (int32 k = 0; k < QRings.Num(); ++k)
		if (UProceduralMeshComponent* Q = Quads[QRings[k]])
		{
			Q->AddRelativeRotation(FRotator(0.f, RingSpeed[k] * (1.f + 4.f * Surge) * Dt, 0.f));
			const FVector L = Q->GetRelativeLocation();
			Q->SetRelativeLocation(FVector(L.X, L.Y, L.Z + FMath::Sin(Time * 1.3f + k) * 4.f * Dt));
		}
	if (Quads.IsValidIndex(QBase) && Quads[QBase]) Quads[QBase]->AddRelativeRotation(FRotator(0.f, 8.f * Dt, 0.f));
	if (Quads.IsValidIndex(QBase2) && Quads[QBase2]) Quads[QBase2]->AddRelativeRotation(FRotator(0.f, -14.f * Dt, 0.f));
	// chevrons rising up the beam
	for (int32 k = 0; k < QChev.Num(); ++k)
	{
		const float Ph = FMath::Frac(Time * 0.32f + k / 3.f);
		if (UProceduralMeshComponent* Q = Quads[QChev[k]]) Q->SetRelativeLocation(FVector(30.f, 0.f, FMath::Lerp(130.f, 400.f, Ph)));
		SetGlow(QChev[k], 1.5f * FMath::Sin(PI * Ph));
	}
	// top orb, star, orbit rings (scoreboard only)
	const float K = 1.f - A;
	if (TopOrb) { TopOrb->SetRelativeScale3D(FVector(FMath::Max(0.01f, 0.95f * K))); TopOrb->SetVisibility(K > 0.02f); TopOrb->SetRelativeLocation(FVector(0, 0, 850.f - 120.f * A)); }
	for (int32 k = 0; k < OrbRingPivots.Num(); ++k)
	{
		OrbRingPivots[k]->AddRelativeRotation(FRotator(0.f, (k == 0 ? 70.f : -90.f) * Dt, 0.f));
		OrbRingPivots[k]->SetRelativeScale3D(FVector(FMath::Max(0.01f, K)));
		SetGlow(QOrbRings[k], 1.1f * K);
	}
	if (Quads.IsValidIndex(QStar) && Quads[QStar])
	{
		const float a = Time * 1.6f;
		Quads[QStar]->SetRelativeLocation(FVector(70.f + 30.f * FMath::Sin(a), 95.f * FMath::Cos(a), 875.f + 22.f * FMath::Sin(a * 0.7f)));
		Quads[QStar]->SetRelativeRotation(FRotator(0.f, 0.f, FMath::RadiansToDegrees(a)));
		SetGlow(QStar, 1.6f * K);
	}
	// drum: scoreboard collapses into the beam, re-forms as the portrait band
	const bool bPortraits = Morph >= Split;
	const float DZ = bPortraits ? HoloEase(B) : 1.f - HoloEase(A);
	if (bPortraits != bBandIsPortraits) { SetTex(QDrum, bPortraits ? RT_Band : RT_Score); bBandIsPortraits = bPortraits; }
	if (UMediaTexture* Ct = ZoogiCritSpire::Clip(W))
	{	// b89b: the critical clip on the scoreboard drum
		if (Mids.IsValidIndex(QDrum) && Mids[QDrum]) Mids[QDrum]->SetTextureParameterValue(TEXT("Tex"), Ct);
		if (!bCritTex) UE_LOG(LogZoogi, Log, TEXT("HOLOSPIRE critical clip on the scoreboard"));
		bCritTex = true;
	}
	else if (bCritTex) { SetTex(QDrum, bBandIsPortraits ? RT_Band : RT_Score); bCritTex = false; }
	DrumPivot->SetRelativeScale3D(FVector(FMath::Max(0.05f, 0.4f + 0.6f * DZ), FMath::Max(0.05f, 0.4f + 0.6f * DZ), FMath::Max(0.01f, DZ)));
	DrumPivot->SetRelativeRotation(FRotator(0.f, (bPortraits ? 1.f - B : A) * 150.f, 0.f));
	// Oct 29: keep the AIM & LAUNCH panel inside the view - a close / low aim camera (Frozen Ring) slides it down the beam
	if (B > 0.f)
		if (APlayerController* PCv = W->GetFirstPlayerController())
		{
			int32 VX = 0, VY = 0;
			PCv->GetViewportSize(VX, VY);
			FVector2D Sp;
			const FVector TopW = Front->GetComponentTransform().TransformPosition(FVector(40.f, 0.f, AimZ + 195.f));
			if (VY > 0 && PCv->ProjectWorldLocationToScreen(TopW, Sp))
			{
				if (Sp.Y < VY * 0.1f) AimZ = FMath::Max(420.f, AimZ - 420.f * Dt);
				else if (Sp.Y > VY * 0.2f && AimZ < 700.f) AimZ = FMath::Min(700.f, AimZ + 200.f * Dt);
			}
		}
	const float BandZ = FMath::Min(395.f, AimZ - 185.f - 105.f);
	DrumPivot->SetRelativeLocation(FVector(0.f, 0.f, bPortraits ? BandZ : 505.f));
	SetGlow(QDrum, FMath::Clamp(DZ * 1.5f, 0.f, 1.f));
	// side cards retract into the beam
	const float CK = HoloEase(K);
	CardLPivot->SetRelativeLocation(FVector(70.f, 205.f * CK, 300.f));
	CardRPivot->SetRelativeLocation(FVector(70.f, -205.f * CK, 300.f));
	CardLPivot->SetRelativeRotation(FRotator(0.f, -22.f * CK, 0.f));
	CardRPivot->SetRelativeRotation(FRotator(0.f, 22.f * CK, 0.f));
	CardLPivot->SetRelativeScale3D(FVector(FMath::Max(0.02f, CK)));
	CardRPivot->SetRelativeScale3D(FVector(FMath::Max(0.02f, CK)));
	SetGlow(QCardL, CK); SetGlow(QCardR, CK);
	// aim panel builds out of the beam
	const float AX = FMath::Max(0.02f, HoloEase(B)), AZ = FMath::Max(0.02f, HoloEase(FMath::Clamp(B * 1.6f - 0.25f, 0.f, 1.f)));
	AimPivot->SetRelativeScale3D(FVector(1.f, AX, AZ));
	AimPivot->SetRelativeLocation(FVector(40.f, 0.f, AimZ + 6.f * FMath::Sin(Time * 1.7f)));
	SetGlow(QAim, FMath::Clamp(B * 2.f, 0.f, 1.f));
	// live textures
	if (K > 0.01f || !bPortraits)
	{
		const FString Sig = ScoreSig(G);
		if (Sig != LastSig) { LastSig = Sig; ScoreRedraw = 4; }
		if (ScoreRedraw > 0) { --ScoreRedraw; DrawScore(G); } // a few frames: new glyphs reach the font atlas a frame late
		CardClock += Dt;
		if (CardClock >= 0.25f && K > 0.02f) { CardClock = 0.f; DrawCards(G, Time); }
	}
	if (Morph > 0.f)
	{
		const FString BSig = FString::Printf(TEXT("%d:%d"), G->ActivePlayer, G->Balls.Num());
		if (BSig != LastBandSig) { LastBandSig = BSig; DrawBand(G); }
	}
	if (B > 0.f)
	{
		AimClock += Dt;
		if (AimClock >= 1.f / 30.f) { AimClock = 0.f; DrawAim(Time, FMath::Clamp(B * 1.25f, 0.f, 1.f)); }
	}
}

// ======================================================================================================== subsystem

bool UZoogiHoloSpireSys::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

TStatId UZoogiHoloSpireSys::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiHoloSpireSys, STATGROUP_Tickables);
}

bool UZoogiHoloSpireSys::Enabled()
{
	static int32 Cached = -1;
	if (Cached < 0)
	{
		bool b = true;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.HoloSpire"), TEXT("bSpire"), b, GGameIni);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSpire"))) b = false;
		Cached = b ? 1 : 0;
	}
	return Cached == 1;
}

// b79 (Evan: "any object in the centre around the spire fades away with the scoreboard - the centre should be free for orbs and
// players to blast across"): small solid props standing in the court centre lose their collision and hide with the spire.
// -ZoogiKeepCentre = leave them.
namespace { TArray<TWeakObjectPtr<UPrimitiveComponent>> GCentreParts; }
static void ZoogiGatherCentre(UWorld* W, AZoogiArena* Ar, AActor* Spire)
{
	GCentreParts.Reset();
	static const bool bKeep = FParse::Param(FCommandLine::Get(), TEXT("ZoogiKeepCentre"));
	if (bKeep || !W || !Ar) return;
	const FVector C = Ar->GetActorLocation();
	const float R = 330.f * Ar->GetActorScale3D().X;
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A || A == Spire || A->IsA<AZoogiBall>() || A->IsA<AZoogiOrb>() || A->IsA<APawn>() || A->IsA<AController>()) continue;
		TInlineComponentArray<UPrimitiveComponent*> Pcs(A);
		for (UPrimitiveComponent* Pc : Pcs)
		{
			if (!Pc || !Pc->IsRegistered() || Pc->IsSimulatingPhysics()) continue;
			const FBoxSphereBounds& B = Pc->Bounds;
			if (B.SphereRadius > R * 1.1f || FVector::Dist2D(B.Origin, C) > R) continue;
			if (B.Origin.Z + B.BoxExtent.Z < C.Z + 12.f) continue; // flat floor dressing stays
			if (B.Origin.Z - B.BoxExtent.Z > C.Z + 900.f) continue; // high up (lights, sky bits)
			Pc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GCentreParts.Add(Pc);
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("HOLOSPIRE centre cleared: %d props lose collision and fade with the scoreboard"), GCentreParts.Num());
}

namespace ZoogiSpireOrbs
{
	struct FEm { TWeakObjectPtr<AZoogiOrb> O; FVector To = FVector::ZeroVector; float T = 0.f, Spark = 0.f; TWeakObjectPtr<AActor> Vis; };
	static TArray<FEm> GEms;
	static double GEmitUntil = 0.0;
	void Emit(AZoogiOrb* O, float Delay)
	{
		if (!O) return;
		O->SetActorHiddenInGame(true);
		FEm E; E.O = O; E.To = O->GetActorLocation(); E.T = -Delay;
		GEms.Add(E);
		GEmitUntil = FMath::Max(GEmitUntil, FPlatformTime::Seconds() + Delay + 1.8);
	}
	void Pulse() { GEmitUntil = FMath::Max(GEmitUntil, FPlatformTime::Seconds() + 1.6); }
	static TArray<TWeakObjectPtr<AZoogiOrb>> GSP;
	void MarkSP(AZoogiOrb* O) { if (O) GSP.AddUnique(O); }
	bool IsSP(const AZoogiOrb* O) { for (const TWeakObjectPtr<AZoogiOrb>& W : GSP) if (W.Get() == O) return true; return false; }
	static void TickSP(AZoogiGameMode* G)
	{
		for (int32 i = GSP.Num() - 1; i >= 0; --i)
		{
			AZoogiOrb* O = GSP[i].Get();
			if (!O || O->bOut || !G) { GSP.RemoveAt(i); continue; }
			if (O->IsHidden()) continue; // still flying in from the spire
			const FVector L = O->OrbLocation();
			AZoogiBall* Who = nullptr;
			for (AZoogiBall* B : G->Balls) if (B && !B->bOut && FVector::Dist(B->BallLocation(), L) < ZoogiConst::ZoogiRadius + ZoogiConst::OrbRadius + 14.f) { Who = B; break; }
			if (!Who) continue;
			UWorld* W = G->GetWorld();
			const FLinearColor Pu(0.7f, 0.3f, 1.f);
			if (UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(G)) CS->AddEnergyBonus(Who->PlayerIndex, 2);
			AZoogiFX::Spawn(W, L, Pu, 220.f, 0.5f, false, 12.f);
			AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), L, FRotator::ZeroRotator, 0.8f);
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 1.f, 1.5f);
			if (!Who->bCPU) ZoogiHaptics::Kick(0.5f, W, TEXT("sp_orb"));
			G->PublicPopup(TEXT("+2 SP"), Who->BallLocation() + FVector(0, 0, 170.f), Pu);
			G->PublicFeed(FString::Printf(TEXT("P%d %s ABSORBS A PURPLE ORB  +2 SP"), Who->PlayerIndex + 1, *Who->Def.Name), Pu);
			UE_LOG(LogZoogi, Log, TEXT("SPIRE SP orb absorbed by P%d (+2 SP)"), Who->PlayerIndex + 1);
			GSP.RemoveAt(i);
			G->Orbs.Remove(O);
			O->Destroy();
		}
	}
	bool Emitting() { return FPlatformTime::Seconds() < GEmitUntil || GEms.Num() > 0; }
}

void UZoogiHoloSpireSys::TickSpireOrbs(float Dt)
{
	using namespace ZoogiSpireOrbs;
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (GSP.Num()) TickSP(G);
	if (GEms.Num() == 0) return;
	FVector Src = (G && G->Arena) ? G->Arena->GetActorLocation() + FVector(0.f, 0.f, 800.f) : FVector(0.f, 0.f, 800.f);
	if (AZoogiHoloSpire* Sp = Spire.Get()) Src = Sp->GetActorTransform().TransformPosition(FVector(0.f, 0.f, 850.f));
	const FLinearColor Cy(0.45f, 0.9f, 1.f), Gd(1.f, 0.82f, 0.3f);
	for (int32 i = GEms.Num() - 1; i >= 0; --i)
	{
		FEm& E = GEms[i];
		AZoogiOrb* O = E.O.Get();
		if (!O || !W) { if (AActor* V = E.Vis.Get()) V->Destroy(); GEms.RemoveAt(i); continue; }
		E.T += Dt;
		if (E.T < 0.f) continue;
		const FLinearColor Col = IsSP(O) ? FLinearColor(0.7f, 0.3f, 1.f) : (O->bStar ? Gd : Cy);
		if (!E.Vis.IsValid())
		{
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Src), SP))
			{
				A->SetMobility(EComponentMobility::Movable);
				A->GetStaticMeshComponent()->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
				A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				A->GetStaticMeshComponent()->SetCastShadow(false);
				A->GetStaticMeshComponent()->SetMaterial(0, ZoogiUtil::FxMID(A, Col, 10.f));
				A->SetActorScale3D(FVector(0.45f));
				E.Vis = A;
			}
			AZoogiFX::Spawn(W, Src, Col, 110.f, 0.3f, false, 10.f); // the spire top flashes as it throws one
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.5f, 1.4f);
		}
		const float K = FMath::Clamp(E.T / 0.85f, 0.f, 1.f), Ke = 1.f - FMath::Square(1.f - K);
		const FVector Mid = (Src + E.To) * 0.5f + FVector(0.f, 0.f, 380.f);
		const FVector P = FMath::Lerp(FMath::Lerp(Src, Mid, Ke), FMath::Lerp(Mid, E.To, Ke), Ke);
		if (AActor* V = E.Vis.Get()) V->SetActorLocation(P);
		E.Spark += Dt;
		if (E.Spark > 0.05f) { E.Spark = 0.f; AZoogiFX::Spawn(W, P, Col, 45.f, 0.25f, false, 8.f); }
		if (K >= 1.f)
		{
			if (AActor* V = E.Vis.Get()) V->Destroy();
			O->SetActorHiddenInGame(false);
			AZoogiFX::Spawn(W, E.To, Col, 140.f, 0.4f, true, 8.f);
			AZoogiFX::Niagara(W, O->bStar ? TEXT("NS_Burst_Yellow") : TEXT("NS_Burst_White"), E.To, FRotator::ZeroRotator, 0.5f);
			GEms.RemoveAt(i);
		}
	}
}

void UZoogiHoloSpireSys::Tick(float DeltaTime)
{
	TickSpireOrbs(FMath::Min(DeltaTime, 0.05f));
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	AZoogiArena* Ar = G ? G->Arena.Get() : nullptr;
	const bool bWant = Enabled() && G && Ar && G->Phase != EZoogiPhase::Menu && !G->bRoyaleMode && Ar->GetTheme() != EZoogiArena::Training && Ar->GetTheme() != EZoogiArena::Ancient; // Oct 31: Ancient's centrepiece is the volcano crown orb
	if (!bWant || BuiltFor.Get() != Ar)
	{
		if (AZoogiHoloSpire* Old = Spire.Get()) Old->Destroy();
		Spire = nullptr;
		BuiltFor = nullptr;
		if (!bWant || Ar->IsForming()) return;
		// stand on whatever is at the centre of the court (hub / fountain top), never higher than 2.5 m
		const FVector C = Ar->GetActorLocation();
		FVector Base = C;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiSpire), false);
		for (AZoogiBall* B : G->Balls) if (B) Q.AddIgnoredActor(B);
		for (AZoogiOrb* O : G->Orbs) if (O) Q.AddIgnoredActor(O);
		FHitResult H;
		if (W->LineTraceSingleByChannel(H, C + FVector(0, 0, 3000.f), C - FVector(0, 0, 2000.f), ECC_Visibility, Q)) Base.Z = FMath::Clamp((float)H.ImpactPoint.Z, (float)C.Z - 50.f, (float)C.Z + 250.f);
		float Sc = Ar->GetActorScale3D().X;
		float Cfg = 1.25f; // a touch over the court scale so it reads from the aim camera
		if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.HoloSpire"), TEXT("Scale"), Cfg, GGameIni);
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AZoogiHoloSpire* N = W->SpawnActor<AZoogiHoloSpire>(AZoogiHoloSpire::StaticClass(), FTransform(Base), SP)) { N->Build(Sc * Cfg); Spire = N; BuiltFor = Ar; ZoogiGatherCentre(W, Ar, N); }
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), ShotDir);
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiSpireShots"))) ShotDir.Reset();
		NextShot = FPlatformTime::Seconds() + 4.0;
		return;
	}
	AZoogiHoloSpire* Sp = Spire.Get();
	if (!Sp) { BuiltFor = nullptr; return; }
	// b69 (Evan: "The centre spire and scoreboard fade away after you launch the first time in round one and don't reappear until
	// rounds 3, 5, 8 - fading away each time"): up at the start of rounds 1 / 3 / 5 / 8 until that round's first launch, else shrunk away.
	// -ZoogiSpireAlways = always up.
	{
		static TWeakObjectPtr<AZoogiHoloSpire> ForSpire; static FVector BaseScale = FVector::OneVector; static float FadeK = 1.f; static int32 LastRound = -1; static bool bLaunched = false;
		if (ForSpire.Get() != Sp) { ForSpire = Sp; BaseScale = Sp->GetActorScale3D(); FadeK = 1.f; LastRound = -1; bLaunched = false; }
		static const bool bAlways = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSpireAlways"));
		const int32 R = G->RoundNumber;
		if (R != LastRound) { LastRound = R; bLaunched = false; }
		if (G->Phase == EZoogiPhase::Rolling) bLaunched = true;
		const bool bShowRound = R <= 1 || R == 3 || R == 5 || R == 8;
		const bool bCritUp = ZoogiCritSpire::On(W); // b89b: up for a critical clip on the scoreboard
		bool bUnder = false; // b142 (Evan): the turn's Zoogi under the scoreboard -> the centre pieces get out of the way
		if (const AZoogiBall* Ab = G->GetActiveBall())
		{
			const float Ur = 850.f * FMath::Max(1.f, G->Arena ? G->Arena->GetActorScale3D().X : 1.f);
			bUnder = !Ab->bOut && G->Phase != EZoogiPhase::Menu && FVector::Dist2D(Ab->BallLocation(), Sp->GetActorLocation()) < Ur;
			static bool bWasUnder = false;
			if (bUnder != bWasUnder) { bWasUnder = bUnder; UE_LOG(LogZoogi, Log, TEXT("HOLOSPIRE: P%d %s the scoreboard"), Ab->PlayerIndex + 1, bUnder ? TEXT("is under") : TEXT("left")); }
		}
		const bool bShow = bAlways || (((bShowRound && !bLaunched) || ZoogiSpireOrbs::Emitting() || bCritUp) && !(bUnder && !bCritUp)); // b86: up while it throws the new orbs
		const float Was = FadeK;
		FadeK = FMath::FInterpConstantTo(FadeK, bShow ? 1.f : 0.f, W->GetDeltaSeconds(), bCritUp ? 1.f / 0.2f : 1.f / 0.8f);
		if (Was != FadeK)
		{
			const float E = FadeK * FadeK * (3.f - 2.f * FadeK);
			Sp->SetActorScale3D(BaseScale * FMath::Max(0.01f, E));
			Sp->SetActorHiddenInGame(FadeK < 0.01f);
			// b77 (Evan: "the white clear pole spire in the centre should fade away with the scoreboard too"): every part, beam / sheath included
			TInlineComponentArray<UPrimitiveComponent*> Parts(Sp);
			for (UPrimitiveComponent* Pc : Parts) if (Pc) Pc->SetHiddenInGame(FadeK < 0.99f && E < 0.35f);
			for (const TWeakObjectPtr<UPrimitiveComponent>& Wp : GCentreParts) if (UPrimitiveComponent* Pc = Wp.Get()) Pc->SetHiddenInGame(FadeK < 0.99f && E < 0.35f); // b79: the centre props go with it
			if ((Was >= 0.01f) != (FadeK >= 0.01f) || (Was < 0.99f && FadeK >= 0.99f)) UE_LOG(LogZoogi, Log, TEXT("HOLOSPIRE %s (round %d)"), FadeK >= 0.5f ? TEXT("back") : TEXT("faded away"), R);
		}
	}
	// Image 1 by default / opponent turns; Image 2 only in the local player's own aim step; back as soon as the shot launches
	const bool bAim = G->IsHumanAiming() && G->IsAimStep();
	if (bAim != Sp->bWantAim) UE_LOG(LogZoogi, Log, TEXT("HOLOSPIRE -> %s (P%d, round %d)"), bAim ? TEXT("AIM & LAUNCH") : TEXT("SCOREBOARD"), G->ActivePlayer + 1, G->RoundNumber);
	Sp->bWantAim = bAim;
	// -ZoogiSpireShots: engine-viewport captures of both modes and the morphs
	if (!ShotDir.IsEmpty() && ShotN < 14)
	{
		const double Now = FPlatformTime::Seconds();
		auto Q = [&](const TCHAR* Name, double Dl) { ShotQ.Add(TPair<FString, double>(FPaths::Combine(ShotDir, FString::Printf(TEXT("spire_%02d_%s.png"), ++ShotN, Name)), Now + Dl)); };
		if (NextShot > 0.0 && Now >= NextShot && !bAim) { NextShot = 0.0; Q(TEXT("score"), 0.0); }
		if (bAim && !bWasAim) { Q(TEXT("to_aim_mid"), 0.35); Q(TEXT("aim"), 1.1); if (!bExported) ExportAt = Now + 1.2; }
		if (ExportAt > 0.0 && Now >= ExportAt) { ExportAt = 0.0; bExported = true; Sp->ExportDebug(ShotDir); }
		if (!bAim && bWasAim) { Q(TEXT("to_score_mid"), 0.35); Q(TEXT("score_after"), 1.2); }
		for (int32 i = ShotQ.Num() - 1; i >= 0; --i)
			if (Now >= ShotQ[i].Value) { FScreenshotRequest::RequestScreenshot(ShotQ[i].Key, true, false); ShotQ.RemoveAt(i); break; }
	}
	bWasAim = bAim;
}
