#pragma once

#include "CoreMinimal.h"
#include "MoverExamplesCharacter.h"
#include "MyMoverPawn.generated.h"

class UCapsuleComponent;
class UCharacterMoverComponent;
class UMyNavMoverComponent;

/**
 * AMyMoverPawn
 *
 * - 无 SkeletalMesh
 * - 无 CharacterMovement
 * - 使用 Mover + NavMover
 * - 可被 AI MoveTo
 */
UCLASS()
class YOURGAME_API AMyMoverPawn : public AMoverExamplesCharacter
{
	GENERATED_BODY()

public:
	AMyMoverPawn(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

protected:
	/** 碰撞体（NavAgent & Mover 都依赖它） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	/** Mover 核心组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UCharacterMoverComponent> MoverComponent;

	/** AI / Nav → Mover 桥接组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UMyNavMoverComponent> NavMoverComponent;
};
