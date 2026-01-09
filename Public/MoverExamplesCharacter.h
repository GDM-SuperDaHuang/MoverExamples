// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoverSimulationTypes.h"
#include "GameFramework/Pawn.h"
#include "MoverExamplesCharacter.generated.h"

class UNavMoverComponent;// 处理AI导航移动的组件
class UInputAction;// 增强输入系统动作
class UCharacterMoverComponent;// Mover系统的核心角色移动组件
struct FInputActionValue; // 输入动作的值结构体

/** 
 * MoverExamplesCharacter: the base pawn class used by the MoverExamples plugin. Handles coalescing of input events.
 * Cannot be instantiated on its own.
 */ 

 
/**
 * MoverExamplesCharacter: MoverExamples插件的基础Pawn类，负责聚合输入事件。
 * 这是一个抽象类，不能直接实例化，需要被具体角色类继承。
 * 
 * 核心职责：
 * 1. 收集玩家输入（移动、视角、跳跃等）
 * 2. 实现IMoverInputProducerInterface接口，为Mover系统生产输入命令
 * 3. 协调CharacterMoverComponent和NavMoverComponent的工作
 * 4. 处理基于移动平台的相对移动转换
 */
UCLASS(Abstract)
class MOVEREXAMPLES_API AMoverExamplesCharacter : public APawn, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AMoverExamplesCharacter(const FObjectInitializer& ObjectInitializer);


public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void BeginPlay() override;

	// 组件初始化后调用，用于获取Mover组件引用
	virtual void PostInitializeComponents() override;

	// 绑定输入功能，使用增强输入系统
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// 【蓝图可用】获取角色移动组件
	UFUNCTION(BlueprintPure, Category = Mover)
	UCharacterMoverComponent* GetMoverComponent() const { return CharacterMotionComponent; }

	// 【蓝图调用】通过方向意图请求移动，长度为1表示最大加速度
	// 用于AI或脚本控制，不是直接玩家输入
	UFUNCTION(BlueprintCallable, Category=MoverExamples)
	virtual void RequestMoveByIntent(const FVector& DesiredIntent) { CachedMoveInputIntent = DesiredIntent; }

	// 【蓝图调用】通过目标速度请求移动，会覆盖其他输入
	// 用于需要精确速度控制的场景（如AI路径跟随）
	UFUNCTION(BlueprintCallable, Category=MoverExamples)
	virtual void RequestMoveByVelocity(const FVector& DesiredVelocity) { CachedMoveInputVelocity=DesiredVelocity; }

	//~ Begin INavAgentInterface Interface
	// 实现导航代理接口，返回导航位置（通常基于CharacterMoverComponent）
	//~ Begin INavAgentInterface Interface
	virtual FVector GetNavAgentLocation() const override;
	//~ End INavAgentInterface Interface
	
	// 更新导航相关性，确保移动组件影响导航生成
	virtual void UpdateNavigationRelevance() override;
	
protected:
	/**
	 * 输入生产入口点，不要重写！
	 * 这是IMoverInputProducerInterface的核心实现，Mover系统每模拟帧调用一次
	 * 要扩展功能，请重写OnProduceInput（C++）或实现"Produce Input"蓝图事件
	 */
	// Entry point for input production. Do not override. To extend in derived character types, override OnProduceInput for native types or implement "Produce Input" blueprint event
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

	/**
	 * 蓝图实现事件，为下一模拟帧生成输入
	 * 在蓝图中实现"Produce Input"事件来扩展输入逻辑
	 * @param DeltaMs 距离上次模拟的毫秒数
	 * @param InputCmd 输入命令，可以修改后传出
	 */
	// Override this function in native class to author input for the next simulation frame. Consider also calling Super method.
	virtual void OnProduceInput(float DeltaMs, FMoverInputCmdContext& InputCmdResult);

	// Implement this event in Blueprints to author input for the next simulation frame. Consider also calling Parent event.
	UFUNCTION(BlueprintImplementableEvent, DisplayName="On Produce Input", meta = (ScriptName = "OnProduceInput"))
	FMoverInputCmdContext OnProduceInputInBlueprint(float DeltaMs, FMoverInputCmdContext InputCmd);

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	TObjectPtr<UInputAction> MoveInputAction;
   
	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	TObjectPtr<UInputAction> LookInputAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	TObjectPtr<UInputAction> JumpInputAction;

	/** Fly Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	TObjectPtr<UInputAction> FlyInputAction;

public:
	/**
	 * 是否相对于站立的移动平台来编写移动输入（true）还是保留在世界空间（false）
	 * 站在移动平台（如电梯、移动平台）上时，输入方向是否随平台旋转
	 * 例如：站在旋转平台上时，按"前进"是否应该相对于平台方向
	 */
	// Whether or not we author our movement inputs relative to whatever base we're standing on, or leave them in world space. Only applies if standing on a base of some sort.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MoverExamples)
	bool bUseBaseRelativeMovement = true;
	
	/**
	 * If true, rotate the Character toward the direction the actor is moving
	 * 
	 * 是否使角色朝向移动方向旋转（true）
	 * false时角色朝向固定方向（如摄像机方向）
	 *
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MoverExamples)
	bool bOrientRotationToMovement = true;
	
	/**
	 * If true, the actor will continue orienting towards the last intended orientation (from input) even after movement intent input has ceased.
	 * This makes the character finish orienting after a quick stick flick from the player.  If false, character will not turn without input.
	 * 是否在停止移动输入后继续朝向最后输入的方向
	 * 允许玩家快速轻推摇杆后角色完成转向，false时无输入就不转向
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MoverExamples)
	bool bMaintainLastInputOrientation = false;

protected:
	/** 角色移动组件（Mover系统的核心） */
	UPROPERTY(Category = Movement, VisibleAnywhere, BlueprintReadOnly, Transient, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterMoverComponent> CharacterMotionComponent;

	/**
	 * 它不移动 Actor，只“生产输入”
	 * AIController / PathFollowingComponent 的导航请求
	 * Directional Intent或 Velocity
	 * 
	 * 写入：CachedMoveInputIntent
	 * CachedMoveInputVelocity
	 * 
	 * 	 * AI导航移动组件
	 * 与CharacterMoverComponent不同，它不直接移动Actor，而是"生产输入"
	 * 这样AIController/PathFollowingComponent可以通过导航请求生成Directional Intent或Velocity输入
	 * 从而实现统一的输入处理，无论是玩家还是AI都走相同的输入生产路径
	 * 
	 * 工作方式：
	 * 1. AI系统调用 RequestMoveByIntent / RequestMoveByVelocity
	 * 2. NavMoverComponent 记录这些请求
	 * 3. ProduceInput时调用 ConsumeNavMovementData 获取AI输入
	 * 4. 写入：CachedMoveInputIntent 或 CachedMoveInputVelocity
	 * 
	 * 
	 * 如：自动导航A到B
	 * AMyAIController (发起导航)
	 *     ↓ (调用 MoveToLocation)，执行MoveTo时候，PathFollowing 有效时，会自动检查这个pawn是否有UNavMoverComponent组件，然后每帧调用 UCharacterMoverComponent::RequestDirectMove
	 * UNavigationSystem (计算路径)
	 *     ↓ (生成路径点)
	 * UPathFollowingComponent (跟随路径)
	 *     ↓ (每帧调用)
	 * UNavMoverComponent (存储导航请求)
	 *     ↓ (ProduceInput 时)
	 * AMoverExamplesCharacter::OnProduceInput() (消费导航数据)
	 *     ↓ (转换为 FCharacterDefaultInputs)
	 * UCharacterMoverComponent (执行物理模拟)
	 *     ↓ (更新 Actor 位置)
	 * AMyMoverCharacter (最终移动)
	 */
	/** Holds functionality for nav movement data and functions */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Nav Movement")
	TObjectPtr<UNavMoverComponent> NavMoverComponent;
	
private:
	/** 最后一次非零移动输入（用于维持朝向） */
	FVector LastAffirmativeMoveInput = FVector::ZeroVector;	// Movement input (intent or velocity) the last time we had one that wasn't zero

	/** 缓存的方向意图输入（来自玩家或AI） */
	FVector CachedMoveInputIntent = FVector::ZeroVector;
	/** 缓存的速度输入（来自玩家或AI，优先级高于意图） */
	FVector CachedMoveInputVelocity = FVector::ZeroVector;

	/** 缓存的视角输入（俯仰/偏航） */
	FRotator CachedTurnInput = FRotator::ZeroRotator;
	/** 缓存的转向输入（可能用于其他系统） */
	FRotator CachedLookInput = FRotator::ZeroRotator;

	/** 跳跃是否刚刚按下（用于触发一次性跳跃） */
	bool bIsJumpJustPressed = false;
	/** 跳跃是否持续按住 */
	bool bIsJumpPressed = false;
	/** 飞行模式是否激活 */
	bool bIsFlyingActive = false;
	/** 是否应该切换飞行模式（在下一帧处理） */
	bool bShouldToggleFlying = false;

	void OnMoveTriggered(const FInputActionValue& Value);
	void OnMoveCompleted(const FInputActionValue& Value);
	void OnLookTriggered(const FInputActionValue& Value);
	void OnLookCompleted(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpReleased(const FInputActionValue& Value);
	void OnFlyTriggered(const FInputActionValue& Value);

	/** 标记蓝图中是否实现了OnProduceInputInBlueprint事件 */
	uint8 bHasProduceInputinBpFunc : 1;
};
