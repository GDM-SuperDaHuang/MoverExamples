#pragma once

#include "CoreMinimal.h"
#include "Navigation/NavMovementComponent.h"
#include "MyNavMoverComponent.generated.h"

/**
 * UMyNavMoverComponent
 *
 * 职责：
 * - 接收 AI / PathFollowing 的 RequestDirectMove
 * - 缓存导航产生的移动意图 / 速度
 * - 提供给 Mover 使用（ConsumeNavMovementData）
 *
 * ❗不负责：
 * - 实际位移
 * - 碰撞
 * - 网络同步
 */
UCLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class YOURGAME_API UMyNavMoverComponent : public UNavMovementComponent
{
	GENERATED_BODY()

public:
	UMyNavMoverComponent();

	/** 
	 * PathFollowingComponent 每帧调用
	 * 这是 AI → Movement 的唯一官方入口
	 */
	virtual void RequestDirectMove(
		const FVector& MoveVelocity,
		bool bForceMaxSpeed) override;

	/**
	 * 被 Pawn / Character 在 ProduceInput 阶段调用
	 * 把导航输入“转交”给 Mover
	 *
	 * @return true  本帧有导航输入
	 * @return false 本帧没有
	 */
	bool ConsumeNavMovementData(
		FVector& OutMoveIntent,
		FVector& OutMoveVelocity);

	/** 当前是否存在待消费的导航输入 */
	bool HasNavMovement() const { return bHasNavInput; }

protected:
	/** 导航期望的速度（世界空间） */
	UPROPERTY(Transient)
	FVector CachedNavVelocity = FVector::ZeroVector;

	/** 导航期望的方向（世界空间，单位向量） */
	UPROPERTY(Transient)
	FVector CachedNavIntent = FVector::ZeroVector;

	/** 是否有未消费的导航输入 */
	UPROPERTY(Transient)
	bool bHasNavInput = false;
};
