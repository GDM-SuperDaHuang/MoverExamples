#include "MyNavMoverComponent.h"
#include "GameFramework/Pawn.h"

UMyNavMoverComponent::UMyNavMoverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// NavMovementComponent 的一些基础设置
	bUseAccelerationForPaths = true;
	bUpdateNavAgentWithOwnersCollision = true;
}

void UMyNavMoverComponent::RequestDirectMove(
	const FVector& MoveVelocity,
	bool bForceMaxSpeed)
{
	/**
	 * 🚨 核心原则：
	 * - 只存数据
	 * - 不要移动 Actor
	 * - 不要调用 Mover
	 */

	if (MoveVelocity.IsNearlyZero())
	{
		return;
	}

	CachedNavVelocity = MoveVelocity;
	CachedNavIntent   = MoveVelocity.GetSafeNormal();
	bHasNavInput      = true;

	// ❌ 千万不要在这里移动 Pawn
	// ❌ 不要 AddActorWorldOffset
	// ❌ 不要 SetActorLocation
}

bool UMyNavMoverComponent::ConsumeNavMovementData(
	FVector& OutMoveIntent,
	FVector& OutMoveVelocity)
{
	if (!bHasNavInput)
	{
		return false;
	}

	OutMoveIntent   = CachedNavIntent;
	OutMoveVelocity = CachedNavVelocity;

	// 本帧消费完毕，立刻清空
	CachedNavIntent   = FVector::ZeroVector;
	CachedNavVelocity = FVector::ZeroVector;
	bHasNavInput      = false;

	return true;
}
