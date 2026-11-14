// Battle Episode Engine (T2): Awakening + reaction entitlement, per Zoogi. Awakening comes ONLY from the activation
// rule ([ZoogiRoll.Awakening] ActivationRule, default InfuseOrGuard: the Zoogi used Infuse or Guard in its Awaken
// stage), never from Episode Energy. While Awakened it may perform ReactionsPerAwakening reactions (Air Reaction).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZoogiBattleTypes.h"
#include "ZoogiReactionComponent.generated.h"

UCLASS(ClassGroup = (Zoogi))
class ZOOGIROLL_API UZoogiReactionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UZoogiReactionComponent();
	UPROPERTY(VisibleAnywhere) int32 Player = -1;
	UPROPERTY(VisibleAnywhere) bool bAwakened = false;
	UPROPERTY(VisibleAnywhere) int32 ReactionsLeft = 0;
	UPROPERTY(VisibleAnywhere) int32 OwnTurnsAtActivation = 0;
	UPROPERTY(VisibleAnywhere) int32 TurnsToLast = 1;
	UPROPERTY(VisibleAnywhere) int32 Activations = 0;
	UPROPERTY(VisibleAnywhere) int32 ReactionsUsed = 0;
	FString Source;

	void BeginAwakening(const FString& Why, int32 OwnTurns, int32 Turns, int32 Reactions);
	void EndAwakening(const FString& Why);
	/** Own turn started: expires once the Awakening has lasted its turns. */
	void OnOwnTurn(int32 OwnTurns);
	bool CanReact(EZoogiReactionKind Kind) const { return bAwakened && ReactionsLeft > 0; }
	void Consume(EZoogiReactionKind Kind);
};
