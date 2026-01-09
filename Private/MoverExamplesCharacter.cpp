// Copyright Epic Games, Inc. All Rights Reserved.


#include "MoverExamplesCharacter.h"
#include "Components/InputComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/LocalPlayer.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "CharacterVariants/AbilityInputs.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PhysicsVolume.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "DefaultMovementSet/NavMoverComponent.h"

AMoverExamplesCharacter::AMoverExamplesCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NavMoverComponent(nullptr)// 初始化AI导航组件为空
{
	PrimaryActorTick.bCanEverTick = true;

	// 关键：禁用Actor级别的移动复制
	// Mover组件有自己独立的网络预测和复制系统，不需要标准的Actor移动复制
	SetReplicatingMovement(false);	// disable Actor-level movement replication, since our Mover component will handle it

	// Lambda函数：检查函数是否在蓝图中实现
	auto IsImplementedInBlueprint = [](const UFunction* Func) -> bool
	{
		return Func && ensure(Func->GetOuter())
			&& Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
	};

	// 检查蓝图是否实现了输入生产事件
	// 这是一个优化，避免在运行时反复检查
	static FName ProduceInputBPFuncName = FName(TEXT("OnProduceInputInBlueprint"));
	UFunction* ProduceInputFunction = GetClass()->FindFunctionByName(ProduceInputBPFuncName);
	bHasProduceInputinBpFunc = IsImplementedInBlueprint(ProduceInputFunction);
}

/**
 * 组件初始化后调用
 * 1. 查找并缓存CharacterMoverComponent引用
 * 2. 设置更新组件是否影响导航生成
 */
void AMoverExamplesCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 获取CharacterMoverComponent，这是Mover系统的核心
	CharacterMotionComponent = FindComponentByClass<UCharacterMoverComponent>();

	if (CharacterMotionComponent)
	{
		// 如果移动组件有更新的场景组件（通常是 CapsuleComponent）
		// 设置它是否影响导航网格生成
		if (USceneComponent* UpdatedComponent = CharacterMotionComponent->GetUpdatedComponent())
		{
			UpdatedComponent->SetCanEverAffectNavigation(bCanAffectNavigationGeneration);
		}
	}
}

/**
 * 每帧调用（在Mover系统模拟之后）
 * 1. 处理视角旋转（将输入应用到控制器）
 * 2. 清空视角缓存（避免累积）
 * 
 * 注意：角色移动本身由CharacterMoverComponent在模拟中处理，不在这里
 */
// Called every frame
void AMoverExamplesCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Do whatever you want here. By now we have the latest movement state and latest input processed.


	// 根据输入旋转摄像机
	// Spin camera based on input
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// 简单的输入缩放，真实游戏通常会映射到加速度曲线
		// Simple input scaling. A real game will probably map this to an acceleration curve
		static float LookRateYaw = 100.f;	// degs/sec
		static float LookRatePitch = 100.f;	// degs/sec

		// 将缓存的视角输入应用到控制器
		// 注意：Yaw是水平旋转，Pitch是垂直旋转
		PC->AddYawInput(CachedLookInput.Yaw * LookRateYaw * DeltaTime);
		PC->AddPitchInput(-CachedLookInput.Pitch * LookRatePitch * DeltaTime);// 负号因为UE的Pitch输入是反的
	}

	// 清除所有与摄像机相关的缓存输入
	// 重要：视角输入是"消耗性"的，每帧清空避免累积
	// Clear all camera-related cached input
	CachedLookInput = FRotator::ZeroRotator;
}

/**
 * 游戏开始时调用
 * 1. 设置摄像机俯仰角限制（防止过度仰视/俯视）
 * 2. 查找NavMoverComponent（AI导航用）
 */
void AMoverExamplesCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 限制摄像机旋转角度
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->PlayerCameraManager->ViewPitchMax = 89.0f;
		PC->PlayerCameraManager->ViewPitchMin = -89.0f;
	}
	
	NavMoverComponent = FindComponentByClass<UNavMoverComponent>();
}

/**
 * 绑定输入功能到回调函数
 * 使用增强输入系统（Enhanced Input System）
 * 为每个动作绑定触发、完成等事件
 */
// Called to bind functionality to input
void AMoverExamplesCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 设置输入绑定，使用增强输入系统
	// 为简化，使用在编辑器中分配的一些输入动作
	// Setup some bindings - we are currently using Enhanced Input and just using some input actions assigned in editor for simplicity
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{	
		Input->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AMoverExamplesCharacter::OnMoveTriggered);
		Input->BindAction(MoveInputAction, ETriggerEvent::Completed, this, &AMoverExamplesCharacter::OnMoveCompleted);
		Input->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AMoverExamplesCharacter::OnLookTriggered);
		Input->BindAction(LookInputAction, ETriggerEvent::Completed, this, &AMoverExamplesCharacter::OnLookCompleted);
		Input->BindAction(JumpInputAction, ETriggerEvent::Started, this, &AMoverExamplesCharacter::OnJumpStarted);
		Input->BindAction(JumpInputAction, ETriggerEvent::Completed, this, &AMoverExamplesCharacter::OnJumpReleased);
		Input->BindAction(FlyInputAction, ETriggerEvent::Triggered, this, &AMoverExamplesCharacter::OnFlyTriggered);
	}
}

/**
 * 实现导航代理接口
 * 返回导航位置，优先使用NavMoverComponent的脚部位置
 * 回退到CharacterMoverComponent的底部位置
 */
FVector AMoverExamplesCharacter::GetNavAgentLocation() const
{
	FVector AgentLocation = FNavigationSystem::InvalidLocation;
	const USceneComponent* UpdatedComponent = CharacterMotionComponent ? CharacterMotionComponent->GetUpdatedComponent() : nullptr;
	// 优先使用NavMoverComponent提供的脚部位置（更精确）
	if (NavMoverComponent)
	{
		AgentLocation = NavMoverComponent->GetFeetLocation();
	}
	// 如果无效，回退到场景组件底部
	if (FNavigationSystem::IsValidLocation(AgentLocation) == false && UpdatedComponent != nullptr)
	{
		AgentLocation = UpdatedComponent->GetComponentLocation() - FVector(0,0,UpdatedComponent->Bounds.BoxExtent.Z);
	}

	return AgentLocation;
}


/**
 * 更新导航相关性
 * 设置移动组件是否影响导航网格生成
 */
void AMoverExamplesCharacter::UpdateNavigationRelevance()
{
	if (CharacterMotionComponent)
	{
		if (USceneComponent* UpdatedComponent = CharacterMotionComponent->GetUpdatedComponent())
		{
			UpdatedComponent->SetCanEverAffectNavigation(bCanAffectNavigationGeneration);
		}
	}
}


/**
 * IMoverInputProducerInterface的核心实现
 * Mover系统每模拟帧调用一次（不一定与渲染帧同步）
 * 1. 调用C++的OnProduceInput
 * 2. 如果蓝图实现了事件，调用蓝图版本
 * 
 * 这是输入生产的主入口，将各种输入源聚合为统一的FCharacterDefaultInputs
 */
void AMoverExamplesCharacter::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	OnProduceInput((float)SimTimeMs, InputCmdResult);

	if (bHasProduceInputinBpFunc)
	{
		InputCmdResult = OnProduceInputInBlueprint((float)SimTimeMs, InputCmdResult);
	}
}


/**
 * 生产输入的核心函数
 * 这是理解Mover输入系统的关键！
 * 
 * 职责：
 * 1. 获取或创建FCharacterDefaultInputs数据结构
 * 2. 处理控制器旋转
 * 3. 消费AI导航输入（如果存在）
 * 4. 转换和优先级处理移动输入（意图 vs 速度）
 * 5. 计算朝向意图
 * 6. 处理特殊输入（跳跃、飞行切换）
 * 7. 转换为基于移动平台的相对输入（如果需要）
 * 8. 清除一次性输入（bIsJumpJustPressed等）
 */
void AMoverExamplesCharacter::OnProduceInput(float DeltaMs, FMoverInputCmdContext& OutInputCmd)
{
	/**
	 * 关键步骤1：获取输入数据结构
	 * FCharacterDefaultInputs是派生自FMoverDataStructBase的结构
	 * 使用FindOrAddMutableDataByType获取或创建，存储在InputCollection中
	 * 生命周期：仅属于这一帧的InputCmd，模拟帧结束后丢弃
	 */
	FCharacterDefaultInputs& CharacterInputs = OutInputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	// 如果没有控制器且是服务器上的模拟代理，提供默认空输入
	// 这是为网络同步考虑：未控制的Pawn不应接收输入
	if (GetController() == nullptr)
	{
		if (GetLocalRole() == ENetRole::ROLE_Authority && GetRemoteRole() == ENetRole::ROLE_SimulatedProxy)
		{
			static const FCharacterDefaultInputs DoNothingInput;
			// If we get here, that means this pawn is not currently possessed and we're choosing to provide default do-nothing input
			// 如果我们到这里，意味着这个pawn当前未被控制，我们提供默认的"什么都不做"输入
			CharacterInputs = DoNothingInput;
		}

		// We don't have a local controller so we can't run the code below. This is ok. Simulated proxies will just use previous input when extrapolating
		return;
	}


	// 查找弹簧臂组件并启用Pawn控制旋转
	// 注意：这里每帧查找组件不是最佳实践，实际项目应缓存引用
	if (USpringArmComponent* SpringComp = FindComponentByClass<USpringArmComponent>())
	{
		// This is not best practice: do not search for component every frame
		SpringComp->bUsePawnControlRotation = true;
	}

	// 初始化控制旋转为零
	CharacterInputs.ControlRotation = FRotator::ZeroRotator;

	// 获取玩家控制器旋转（摄像机方向）
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		CharacterInputs.ControlRotation = PC->GetControlRotation();
	}

	/**
	 * 关键步骤2：如果本帧是 AI / MoveTo 在控制 ，覆盖玩家输入，用完即清，不影响下一帧玩家输入
	 * 如果存在NavMoverComponent，尝试从中获取AI导航请求
	 * ConsumeNavMovementData会返回是否有AI移动请求，并填充缓存
	 */
	bool bRequestedNavMovement = false;
	if (NavMoverComponent)
	{
		// 拷贝到 CachedXXX 里，这样会覆盖玩家数据 → 并立刻清空 Nav 侧缓存
		bRequestedNavMovement = NavMoverComponent->ConsumeNavMovementData(CachedMoveInputIntent, CachedMoveInputVelocity);
		// 可以在别的地方使用UNavMoverComponent::RequestDirectMove(...)写入数据
	}
	
	// 优先级：速度输入 > 方向意图
	// Favor velocity input 
	bool bUsingInputIntentForMove = CachedMoveInputVelocity.IsZero();

	/**
	 * 关键步骤3：设置移动输入
	 * 两种模式：
	 * 1. DirectionalIntent：方向意图，Mover系统会根据加速度模型计算最终速度
	 * 2. Velocity：目标速度，Mover系统会尝试精确达到这个速度
	 */
	if (bUsingInputIntentForMove)
	{
		// 使用控制旋转将输入从局部空间转换到世界空间
		// 例如：玩家按"前进"是基于摄像机方向的"前进"，不是世界坐标Z轴
		const FVector FinalDirectionalIntent = CharacterInputs.ControlRotation.RotateVector(CachedMoveInputIntent);
		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FinalDirectionalIntent);
	}
	else
	{
		CharacterInputs.SetMoveInput(EMoveInputType::Velocity, CachedMoveInputVelocity);
	}

	// 通常缓存输入由OnMoveCompleted输入事件清除
	// 但如果移动来自AI导航，不会触发该事件，所以这里手动清除
	// 防止AI输入持续影响下一帧
	// Normally cached input is cleared by OnMoveCompleted input event but that won't be called if movement came from nav movement
	if (bRequestedNavMovement)
	{
		CachedMoveInputIntent = FVector::ZeroVector;
		CachedMoveInputVelocity = FVector::ZeroVector;
	}
	
	static float RotationMagMin(1e-3);// 最小旋转幅度阈值
	// 检查是否有有效的移动输入（长度大于阈值）
	const bool bHasAffirmativeMoveInput = (CharacterInputs.GetMoveInput().Size() >= RotationMagMin);
	
	/**
	 * 关键步骤4：计算朝向意图
	 * 决定角色应该面向哪里
	 */
	// Figure out intended orientation
	CharacterInputs.OrientationIntent = FVector::ZeroVector;


	if (bHasAffirmativeMoveInput)
	{
		if (bOrientRotationToMovement)
		{
			// 将意图设置为角色移动方向（移动方向决定朝向）
			// set the intent to the actors movement direction
			CharacterInputs.OrientationIntent = CharacterInputs.GetMoveInput().GetSafeNormal();
		}
		else
		{
			// 将意图设置为控制旋转（通常是玩家摄像机方向）
			// set intent to the the control rotation - often a player's camera rotation
			CharacterInputs.OrientationIntent = CharacterInputs.ControlRotation.Vector().GetSafeNormal();
		}

		// 记录最后一次有效移动输入，用于bMaintainLastInputOrientation
		LastAffirmativeMoveInput = CharacterInputs.GetMoveInput();
	}
	else if (bMaintainLastInputOrientation)
	{
		// There is no movement intent, so use the last-known affirmative move input
		CharacterInputs.OrientationIntent = LastAffirmativeMoveInput;
	}
	
	// 设置跳跃状态
	CharacterInputs.bIsJumpPressed = bIsJumpPressed;
	CharacterInputs.bIsJumpJustPressed = bIsJumpJustPressed;// 一次性标记，见下方清除逻辑

	/**
	 * 关键步骤5：处理飞行模式切换
	 * 飞行是互斥状态：按一次开启，再按一次关闭
	 */
	if (bShouldToggleFlying)
	{
		if (!bIsFlyingActive)
		{
			CharacterInputs.SuggestedMovementMode = DefaultModeNames::Flying; // 建议切换到飞行模式
		}
		else
		{
			CharacterInputs.SuggestedMovementMode = DefaultModeNames::Falling;// 建议切换到下落模式
		}

		bIsFlyingActive = !bIsFlyingActive;// 切换状态
	}
	else
	{
		CharacterInputs.SuggestedMovementMode = NAME_None;// 无模式建议
	}

	/**
	 * 关键步骤6：基于移动平台的输入转换
	 * 如果站在移动平台（如电梯）上，将输入从世界空间转换到平台局部空间
	 * 这样"前进"就是相对于平台的前进，不是世界坐标
	 */
	// Convert inputs to be relative to the current movement base (depending on options and state)
	CharacterInputs.bUsingMovementBase = false;

	if (bUseBaseRelativeMovement)
	{
		if (const UCharacterMoverComponent* MoverComp = GetComponentByClass<UCharacterMoverComponent>())
		{
			// 获取当前移动平台
			if (UPrimitiveComponent* MovementBase = MoverComp->GetMovementBase())
			{
				FName MovementBaseBoneName = MoverComp->GetMovementBaseBoneName();

				FVector RelativeMoveInput, RelativeOrientDir;

				// 使用BasedMovementUtils将世界方向转换到基于平台的方向
				UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterInputs.GetMoveInput(), RelativeMoveInput);
				UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterInputs.OrientationIntent, RelativeOrientDir);

				// 更新输入为相对方向
				CharacterInputs.SetMoveInput(CharacterInputs.GetMoveInputType(), RelativeMoveInput);
				CharacterInputs.OrientationIntent = RelativeOrientDir;

				// 标记使用了移动平台
				CharacterInputs.bUsingMovementBase = true;
				CharacterInputs.MovementBase = MovementBase;
				CharacterInputs.MovementBaseBoneName = MovementBaseBoneName;
			}
		}
	}

	/**
	 * 关键步骤7：清除一次性输入
	 * 某些输入只应在触发它们的模拟帧中有效（避免重复触发）
	 * 但不清除持续性输入（如移动意图），因为模拟频率可能低于游戏帧率
	 */
	// Clear/consume temporal movement inputs. We are not consuming others in the event that the game world is ticking at a lower rate than the Mover simulation. 
	// In that case, we want most input to carry over between simulation frames.
	{

		bIsJumpJustPressed = false;
		bShouldToggleFlying = false;
	}
}

/**
 * 移动输入触发回调（增强输入系统）
 * 将输入值规范化后存入CachedMoveInputIntent
 */
void AMoverExamplesCharacter::OnMoveTriggered(const FInputActionValue& Value)
{
	const FVector MovementVector = Value.Get<FVector>();
	CachedMoveInputIntent.X = FMath::Clamp(MovementVector.X, -1.0f, 1.0f);
	CachedMoveInputIntent.Y = FMath::Clamp(MovementVector.Y, -1.0f, 1.0f);
	CachedMoveInputIntent.Z = FMath::Clamp(MovementVector.Z, -1.0f, 1.0f);
}

/**
 * 移动输入完成回调（玩家释放按键）
 * 清除移动输入缓存
 * 
 * 注意：AI导航输入不会触发此回调，所以 ProduceInput 中手动清除
 */
void AMoverExamplesCharacter::OnMoveCompleted(const FInputActionValue& Value)
{
	CachedMoveInputIntent = FVector::ZeroVector;
}

/**
 * 视角输入触发回调
 * 获取2D视角变化值并缓存
 */
void AMoverExamplesCharacter::OnLookTriggered(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
	CachedLookInput.Yaw = CachedTurnInput.Yaw = FMath::Clamp(LookVector.X, -1.0f, 1.0f);
	CachedLookInput.Pitch = CachedTurnInput.Pitch = FMath::Clamp(LookVector.Y, -1.0f, 1.0f);
}

/**
 * 视角输入完成回调
 * 清除视角缓存
 */
void AMoverExamplesCharacter::OnLookCompleted(const FInputActionValue& Value)
{
	CachedLookInput = FRotator::ZeroRotator;
	CachedTurnInput = FRotator::ZeroRotator;
}

/**
 * 跳跃开始回调
 * 设置跳跃状态标志
 * bIsJumpJustPressed用于触发一次性跳跃（防止按住连跳）
 */
void AMoverExamplesCharacter::OnJumpStarted(const FInputActionValue& Value)
{
	bIsJumpJustPressed = !bIsJumpPressed;
	bIsJumpPressed = true;
}

/**
 * 跳跃释放回调
 * 清除跳跃状态
 */
void AMoverExamplesCharacter::OnJumpReleased(const FInputActionValue& Value)
{
	bIsJumpPressed = false;
	bIsJumpJustPressed = false;
}

/**
 * 飞行切换回调
 * 设置切换标志（在ProduceInput中处理实际切换逻辑）
 * 使用标志模式确保在正确的模拟帧处理
 */
void AMoverExamplesCharacter::OnFlyTriggered(const FInputActionValue& Value)
{
	bShouldToggleFlying = true;
}
