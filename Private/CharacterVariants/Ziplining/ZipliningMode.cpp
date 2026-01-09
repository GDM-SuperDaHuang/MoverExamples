// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterVariants/Ziplining/ZipliningMode.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"
#include "CharacterVariants/Ziplining/ZiplineInterface.h"
#include "CharacterVariants/Ziplining/ZipliningTransitions.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoverLog.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(ZipliningMode)





// FZipliningState //////////////////////////////

FMoverDataStructBase* FZipliningState::Clone() const
{
	FZipliningState* CopyPtr = new FZipliningState(*this);
	return CopyPtr;
}

/**
 * Actor 指针 → PackageMap
 * bool → bit 压缩
 */
bool FZipliningState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << ZiplineActor;
	Ar.SerializeBits(&bIsMovingAtoB,1);

	bOutSuccess = true;
	return true;
}

void FZipliningState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("ZiplineActor: %s\n", *GetNameSafe(ZiplineActor));
	Out.Appendf("IsMovingAtoB: %d\n", bIsMovingAtoB);
}
/**
 * 如果 ZiplineActor 不同
 * → 客户端必须 整体回滚 + 重演
 */
bool FZipliningState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FZipliningState* AuthorityZiplineState = static_cast<const FZipliningState*>(&AuthorityState);

	return (ZiplineActor != AuthorityZiplineState->ZiplineActor) ||
		   (bIsMovingAtoB != AuthorityZiplineState->bIsMovingAtoB);
}

void FZipliningState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FZipliningState* FromState = static_cast<const FZipliningState*>(&From);
	const FZipliningState* ToState = static_cast<const FZipliningState*>(&To);

	ZiplineActor = ToState->ZiplineActor;
	bIsMovingAtoB = ToState->bIsMovingAtoB;
}



/**
 * 评估 UZiplineEndTransition，如何从 Ziplining 离开
 * 
 * 一、为什么 UZipliningMode 里只有 UZiplineEndTransition？
 * 关键结论先给出
 * 进入 Ziplining 的 Transition（StartTransition）
 * 并不属于 UZipliningMode，
 * 而是属于「前一个 Mode」
 * 
 * 那 UZiplineStartTransition 在哪里？
 * 它通常在 其它 Mode 里，例如：
 * Falling
 * Walking
 * Flying
 */
UZipliningMode::UZipliningMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Transitions.Add(CreateDefaultSubobject<UZiplineEndTransition>(TEXT("ZiplineEndTransition")));
}

/**
 * 这是一个刻意的空实现
 * 为什么？

Zipline 的移动是：

不依赖输入，后续不会有输入影响这个行为

沿一条确定路径

不需要预测分段

👉 所以 完全跳过 GenerateMove
 */
void UZipliningMode::GenerateMove_Implementation(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	UMoverComponent* MoverComp = GetMoverComponent();

	// Ziplining is just following a path from A to B, so all movement is handled in OnSimulationTick
	OutProposedMove = FProposedMove();

}


/**
 * 1个Simulation Frame 内会执行多次，一个 Simulation Frame 会有多个 SimulationTick
 */
void UZipliningMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	// Are we continuing a move or starting fresh?
	/**
	 * ████████ 阶段一：状态判断 - 新开始还是继续滑动？ ████████
	 * 是否存在 FZipliningState 是第一次进入 Zipline
	 * 1 找不到 → 第一次
	 * 2 找到 → 继续滑
	 * 此时：Params.StartState 为上一个 OutputState
	 */
	const FZipliningState* StartingZipState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FZipliningState>();

	/**
	 * 准备输出状态引用
	 * 获取默认同步状态（位置、旋转、速度等基础信息）
	 */
	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	/**
	 * 获取或创建滑索专用状态（滑索Actor、移动方向等），每次SimulationTick都创建空的OutZipState
	 */
	FZipliningState& OutZipState            = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FZipliningState>();

	// 被移动的组件（通常是角色的碰撞胶囊体）
	USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	
	// Mover组件
	UMoverComponent* MoverComp = Params.MovingComps.MoverComponent.Get();
	// 移动的Actor（角色）
	AActor* MoverActor = MoverComp->GetOwner();

	// 滑索相关变量
	USceneComponent* StartPoint = nullptr;// 滑索起点组件
	USceneComponent* EndPoint = nullptr;// 滑索终点组件
	FVector ZipDirection;// 滑索方向向量（从起点指向终点）
	FVector FlatFacingDir;// 角色面向方向（投影到水平面）

	// 时间转换：毫秒 → 秒, 本帧要模拟的时间长度
	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
   
	// ████████ 计算角色边界偏移 ████████
    // 获取角色的边界框，用于计算角色中心到滑索的垂直偏移
    // 这样角色会悬挂在滑索下方，而不是身体卡在滑索里
	FVector ActorOrigin;
	FVector BoxExtent;
	MoverActor->GetActorBounds(true, OUT ActorOrigin, OUT BoxExtent);
	const FVector ActorToZiplineOffset = MoverComp->GetUpDirection() * BoxExtent.Z;// 向上方向 * 半身高度

	 // ████████ 阶段二：初始化逻辑 - 第一次进入滑索 ████████
	if (!StartingZipState) // 2. 如果没有起始滑索状态，说明是第一次接触滑索，需要初始化
	{
		// There is no existing zipline state... so let's find the target
		//    A) teleport to the closest starting point, set the zip direction
		//    B) choose the appropriate facing direction
		//    C) choose the appropriate initial velocity
		/**
		 * 没有现有的滑索状态…
		 * A)传送到最近的起始点，设置压缩方向
		 * B)选择合适的面向方向
		 * C)选择合适的初始速度 tarray OverlappingActors；
		 */
		TArray<AActor*> OverlappingActors;
		// 获取所有与角色重叠的Actor，从中寻找滑索
		MoverComp->GetOwner()->GetOverlappingActors(OUT OverlappingActors);

		for (AActor* CandidateActor : OverlappingActors)
		{
			// 检查Actor是否实现了滑索接口
			bool bIsZipline = UKismetSystemLibrary::DoesImplementInterface(CandidateActor, UZipline::StaticClass());

			if (bIsZipline)
			{
				// 获取角色当前位置
				const FVector MoverLoc = UpdatedComponent->GetComponentLocation();
				// 获取滑索的两个端点
				USceneComponent* ZipPointA = IZipline::Execute_GetStartComponent(CandidateActor);
				USceneComponent* ZipPointB = IZipline::Execute_GetEndComponent(CandidateActor);

				// 计算角色到两个端点的距离，选择更近的作为起点
                // 这样无论从哪个方向接近滑索，角色都会从最近点开始滑行
				if (FVector::DistSquared(ZipPointA->GetComponentLocation(), MoverLoc) < FVector::DistSquared(ZipPointB->GetComponentLocation(), MoverLoc))
				{
					OutZipState.bIsMovingAtoB = true;// 标记为从A到B移动
					StartPoint = ZipPointA;// 设置起点
					EndPoint = ZipPointB;// 设置终点
				}
				else
				{
					OutZipState.bIsMovingAtoB = false;// 标记为从B到A移动
					StartPoint = ZipPointB;// 设置起点（B点）
					EndPoint = ZipPointA;// 设置终点（A点）
				}

				// 计算滑索方向：从起点指向终点的单位向量
				ZipDirection = (EndPoint->GetComponentLocation() - StartPoint->GetComponentLocation()).GetSafeNormal();

				// ████████ 角色位置校准 ████████
                // 计算传送位置：起点位置 - 角色高度偏移
                // 这样角色会悬挂在滑索的正下方，而不是身体卡在滑索里
				const FVector WarpLocation = StartPoint->GetComponentLocation() - ActorToZiplineOffset;

				// 计算角色面向方向：将滑索方向投影到角色所在的平面（通常是水平面）
                // 这样角色会面朝移动方向
				FlatFacingDir = FVector::VectorPlaneProject(ZipDirection, MoverComp->GetUpDirection()).GetSafeNormal();

				// 保存滑索Actor引用到状态，供后续帧使用
				OutZipState.ZiplineActor = CandidateActor;

				//将角色传送到计算好的起点位置，并设置面向方向 ,传送 到起点
				UpdatedComponent->GetOwner()->TeleportTo(WarpLocation, FlatFacingDir.ToOrientationRotator());

				break;
			}
		}

		// If we were unable to find a valid target zipline, refund all the time and let the actor fall
		 // ████████ 错误处理 ████████
        // 如果没有找到有效的滑索（起点或终点为空），说明初始化失败
        // 这种情况下，将角色切换到默认的空中模式（自由落体），并退还本帧剩余时间
		if (!StartPoint || !EndPoint)
		{
			// 获取默认的空中模式名称（通常是"Falling"）
			FName DefaultAirMode = DefaultModeNames::Falling;
			if (UCommonLegacyMovementSettings* LegacySettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>())
			{
				// 使用配置中的空中模式
				DefaultAirMode = LegacySettings->AirMovementModeName;
			}

			/***
			 * 通常一个Simulation Frame 内只执行一个MovementMode,
			 * 如果这个MovementMode提前结束，
			 * 则，OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;可以使得剩余时间执行下一个MovementMode
			 * 这是「同一个 Simulation Frame 内，多个 MovementMode 串行执行」的唯一合法通道
			 * 在剩余时间内会执行下一个对应的SimulationTick_Implementation，但是不会执行GenerateMove_Implementation(这个需要下一个Simulation Frame)
			 */
			// 设置下一帧的运动模式为空中模式
			OutputState.MovementEndState.NextModeName = DefaultAirMode;
			// 退还本帧剩余时间，让新的运动模式处理完整的时间步长
			OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
			return;
		}

	}
	else// ████████ 阶段三：继续滑动逻辑 - 非首次进入 ████████
	{
		check(StartingZipState->ZiplineActor); // 断言：滑索Actor必须存在
		// 复制之前的滑索状态到输出状态
		OutZipState = *StartingZipState;

		// 获取滑索的两个端点
		USceneComponent* ZipPointA = IZipline::Execute_GetStartComponent(StartingZipState->ZiplineActor);
		USceneComponent* ZipPointB = IZipline::Execute_GetEndComponent(StartingZipState->ZiplineActor);

		 // 根据之前记录的移动方向，确定当前起点和终点
		if (StartingZipState->bIsMovingAtoB)
		{
			StartPoint = ZipPointA;
			EndPoint = ZipPointB;
		}
		else
		{
			StartPoint = ZipPointB;
			EndPoint = ZipPointA;
		}

		// 重新计算滑索方向（考虑滑索可能在运动）
		ZipDirection = (EndPoint->GetComponentLocation() - StartPoint->GetComponentLocation()).GetSafeNormal();
		FlatFacingDir = FVector::VectorPlaneProject(ZipDirection, MoverComp->GetUpDirection()).GetSafeNormal();
	}


	// 沿 Zipline 移动, 计算本帧的移动, 当前在滑索上的起点位置（角色位置 + 偏移量，得到滑索上的实际悬挂点）
	// Now let's slide along the zipline
	const FVector StepStartPos = UpdatedComponent->GetComponentLocation() + ActorToZiplineOffset;


	// ████████ 运动学计算 ████████
    // 计算期望的终点位置：起点 + 方向 * 速度 * 时间
    // 使用MaxSpeed作为恒定速度，TODO：未来可以做成动态的（如加速/减速）
    // 这种计算方法保证完全确定性，不会超出线段范围
	// DesiredEndPos = Start + Direction * Speed * Δt;
	// ✔ 保证不会超出线段
	// ✔ 完全 deterministic
	const FVector DesiredEndPos = StepStartPos + (ZipDirection * MaxSpeed * DeltaSeconds);	// TODO: Make speed more dynamic，


	// ████████ 边界约束 ████████
    // 将期望终点限制在滑索线段上，确保不会滑出滑索范围
    // 如果超过终点，会返回线段上最近的点（即终点）
	FVector ActualEndPos = FMath::ClosestPointOnSegment(DesiredEndPos,
		StartPoint->GetComponentLocation(),
		EndPoint->GetComponentLocation());

	// 判断是否即将到达终点：如果实际终点和终点的距离几乎为零
	bool bWillReachEndPosition = (ActualEndPos - EndPoint->GetComponentLocation()).IsNearlyZero();

	// 准备移动记录，用于记录本次移动的详细信息
	FVector MoveDelta = ActualEndPos - StepStartPos;



	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);


	// ████████ 物理移动 ████████
    // 如果有实际移动，执行物理检测和移动
	if (!MoveDelta.IsNearlyZero())
	{
		
		FHitResult Hit(1.f); // 初始化碰撞结果
		// 第一步：执行物理计算，获取结果
        // 执行安全的组件移动：
        // - 使用胶囊体扫描检测碰撞
        // - 记录移动后的速度
        // - 支持碰撞阻挡和滑动
        // - 不会触发传送
		UMovementUtils::TrySafeMoveUpdatedComponent(Params.MovingComps, MoveDelta, FlatFacingDir.ToOrientationQuat(), true, Hit, ETeleportType::None, MoveRecord);
	}


	// ████████ 阶段五：状态同步 ████████
    // 获取移动后的最终位置和速度
	const FVector FinalLocation = UpdatedComponent->GetComponentLocation();
	const FVector FinalVelocity = MoveRecord.GetRelevantVelocity();

	// 第二步：显式应用结果到状态
	// ⚠️ 极关键：写入同步状态
    // 这是Mover插件中唯一允许"修改位置"的地方
    // 将最终状态写入输出，供网络同步和下一帧使用
	OutputSyncState.SetTransforms_WorldSpace(FinalLocation,
		UpdatedComponent->GetComponentRotation(),
		FinalVelocity,
		FVector::ZeroVector,
		nullptr); // no movement base

	// 同时更新组件的速度（用于物理模拟等）
	UpdatedComponent->ComponentVelocity = FinalVelocity;


	// ████████ 阶段六：终点处理 ████████
    // 如果到达终点，切换到默认的空中模式
	if (bWillReachEndPosition)
	{
		// 获取默认空中模式名称
		FName DefaultAirMode = DefaultModeNames::Falling;
		if (UCommonLegacyMovementSettings* LegacySettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>())
		{
			DefaultAirMode = LegacySettings->AirMovementModeName;
		}

 
		/*** 设置下一帧的运动模式
		 * 这里没有使用OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
		 * 则需要下一个 Simulation Frame才会执行一个MovementMode
		 */
		OutputState.MovementEndState.NextModeName = DefaultAirMode;
		// TODO: If we reach the end position early, we should refund the remaining time

	}
}



