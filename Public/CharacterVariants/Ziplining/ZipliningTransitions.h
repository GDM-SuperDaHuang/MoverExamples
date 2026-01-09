// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovementModeTransition.h"
#include "CharacterVariants/Ziplining/ZipliningMode.h"
#include "ZipliningTransitions.generated.h"


/**
 * 跨 Mode 的全局决策
 * 要不要打断当前 Mode？
 * 
 * 和下面的不同，下面的是mode自身的判断模式切换，
 * “ OutputState.MovementEndState.NextModeName = DefaultAirMode; OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;”
 * 虽然可以使用上面的进行模式切换，但是会导致强耦合，如Walking需要在SimulationTick_Implementation判断所以的模式的切换，
 * 新增 Falling / Flying 都要写一份判断
 * 
 * Transition that handles starting ziplining based on input. Character must be airborne to catch the
 * zipline, regardless of input.
 * 根据输入处理启动滑索的转换。角色必须在空中才能抓住滑索，不管输入是什么
 */
UCLASS(Blueprintable, BlueprintType)
class UZiplineStartTransition : public UBaseMovementModeTransition
{
	GENERATED_UCLASS_BODY()

public:
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;

	UPROPERTY(EditAnywhere, Category = "Ziplining")
	FName ZipliningModeName = ExtendedModeNames::Ziplining;
};

/**
 * Transition that handles exiting ziplining based on input
 * 根据输入处理退出滑索的转换
 */
UCLASS(Blueprintable, BlueprintType)
class UZiplineEndTransition : public UBaseMovementModeTransition
{
	GENERATED_UCLASS_BODY()

public:
	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;

	// Mode to enter when exiting the zipline
	UPROPERTY(EditAnywhere, Category = "Ziplining")
	FName AutoExitToMode = DefaultModeNames::Falling;
};
