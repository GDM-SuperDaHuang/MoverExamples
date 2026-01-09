// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterVariants/Ziplining/ZipliningTransitions.h"
#include "CharacterVariants/AbilityInputs.h"
#include "CharacterVariants/Ziplining/ZiplineInterface.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetSystemLibrary.h"


// UZiplineStartTransition //////////////////////////////

UZiplineStartTransition::UZiplineStartTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/**
 * UZiplineStartTransition 在 1 个 Simulation Frame 内可能会被 多次 Evaluate，但它最多只会成功触发一次 Mode 切换
 * 
 * Transition 只判断，不做移动
 * ████████ 开始滑索的过渡条件评估 ████████
 * 核心原则：Transition类只负责"判断是否应该切换模式"，绝不执行任何移动逻辑
 * 这样可以保持职责单一，避免状态机混乱
 * 
 * Params
 * Params.StartState   // 当前 Simulation Frame 的起始状态
 * Params.TimeStep     // 总 StepMs
 * Params.MovingComps
 * 
 * SimulationTick执行OutputState.MovementEndState.NextModeName时，会立刻进入Evaluate_Implementation
 * EvalResult.NextMode = ZipliningModeName;执行时，重新进入 Evaluate 阶段（新 Mode），触发Trigger_Implementation，此时还没 GenerateMove！
 * 
 */
FTransitionEvalResult UZiplineStartTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	// 默认结果为"不切换"（FTransitionEvalResult::NoTransition是静态常量，表示维持当前模式）
	FTransitionEvalResult EvalResult = FTransitionEvalResult::NoTransition;

	// 获取角色移动组件（带类型安全检查）
	UCharacterMoverComponent* MoverComp = Cast<UCharacterMoverComponent>(Params.MovingComps.MoverComponent.Get());

	// 获取上一帧的同步状态（只读）
	// Params.StartState = 上一个 子步 Tick SimulationTick 的 OutputState
	const FMoverSyncState& SyncState = Params.StartState.SyncState;


	// 开始滑索的三大必要条件
	// 1. MoverComp有效
	// 2. IsAirborne() == true（角色必须在空中，不能在地面上）
	// 3. MovementMode != ZipliningModeName（当前不能在滑索状态中）
	if (MoverComp && MoverComp->IsAirborne() && SyncState.MovementMode != ZipliningModeName)
	{
		// 从输入命令中查找能力输入（是否按下滑索键）
		// 使用FindDataByType是因为输入是变体类型集合，支持扩展
		if (const FMoverExampleAbilityInputs* AbilityInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FMoverExampleAbilityInputs>())
		{
			// 检查玩家是否按下了"开始滑索"的输入键
			if (AbilityInputs->bWantsToStartZiplining)
			{
				// 获取所有与角色重叠的Actor
				// 这是检测角色是否接触到滑索的关键
				TArray<AActor*> OverlappingActors;
				MoverComp->GetOwner()->GetOverlappingActors(OUT OverlappingActors);

				// 如果找到滑索，立即设置切换到滑索模式
				for (AActor* CandidateActor : OverlappingActors)
				{
					bool bIsZipline = UKismetSystemLibrary::DoesImplementInterface(CandidateActor, UZipline::StaticClass());

					if (bIsZipline)
					{
						EvalResult.NextMode = ZipliningModeName;
						break;
					}
				}
			}
		}
	}

	return EvalResult;
}



// UZiplineEndTransition //////////////////////////////

UZiplineEndTransition::UZiplineEndTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/**
 *  End Transition（跳跃）
 *  ████████ 结束滑索的过渡条件评估 - 跳跃退出 ████████
 */
FTransitionEvalResult UZiplineEndTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	// 默认不切换
	FTransitionEvalResult EvalResult = FTransitionEvalResult::NoTransition;

	// ████████ 只有跳跃键能触发退出滑索 ████████
	// 从输入命令中查找默认角色输入（跳跃、移动等）
	if (const FCharacterDefaultInputs* DefaultInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>())
	{
		// 检查跳跃键是否在本帧被按下（bIsJumpJustPressed 是边缘触发，不是持续按住）
		if (DefaultInputs->bIsJumpJustPressed)
		{
			// （跳跃）
			EvalResult.NextMode = AutoExitToMode;
		}
	}

	return EvalResult;
}

// ████████ 过渡触发时的回调 - 执行一次性逻辑 ████████
// 与Evaluate的区别：Evaluate每帧都执行检查，Trigger只在实际切换时执行一次
void UZiplineEndTransition::Trigger_Implementation(const FSimulationTickParams& Params)
{
	//TODO: create a small jump, using current directionality
	// TODO: 创建一个小的跳跃效果，使用当前的方向性
	// 注释说明未来计划：当玩家从滑索跳下时，应该给他们一个小的向上速度
	// 这样游戏体验更自然，符合物理直觉
	
	// 实现方案可能包括：
	// 1. 设置一个初始垂直速度（如JumpVelocity）
	// 2. 保留水平方向的速度（惯性）
	// 3. 可能添加一个小的向前推力，让玩家"飞出去"
}
