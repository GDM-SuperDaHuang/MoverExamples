#include "MyMoverPawn.h"

#include "Components/CapsuleComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MyNavMoverComponent.h"
#include "AIController.h"

AMyMoverPawn::AMyMoverPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// =========================
	// Root / Collision
	// =========================
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	RootComponent = CapsuleComponent;

	CapsuleComponent->InitCapsuleSize(34.f, 88.f);
	CapsuleComponent->SetCollisionProfileName(TEXT("Pawn"));
	CapsuleComponent->SetCanEverAffectNavigation(true);

	// =========================
	// Mover
	// =========================
	MoverComponent = CreateDefaultSubobject<UCharacterMoverComponent>(TEXT("MoverComponent"));
	MoverComponent->SetUpdatedComponent(CapsuleComponent);

	// =========================
	// Nav → Mover 桥接
	// =========================
	NavMoverComponent = CreateDefaultSubobject<UMyNavMoverComponent>(TEXT("NavMoverComponent"));

	// NavMovementComponent 必须知道 UpdatedComponent
	NavMoverComponent->UpdatedComponent = CapsuleComponent;

	// =========================
	// AI 设置
	// =========================
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	bUseControllerRotationYaw = false;
}

void AMyMoverPawn::BeginPlay()
{
	Super::BeginPlay();

	// 确保 NavAgent 数据来自 Capsule
	if (NavMoverComponent)
	{
		NavMoverComponent->UpdateNavAgent(*CapsuleComponent);
	}
}
