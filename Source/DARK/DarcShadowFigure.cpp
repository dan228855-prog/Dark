// DarcShadowFigure.cpp
#include "DarcShadowFigure.h"
#include "DarcAssetSettings.h"
#include "Animation/AnimSequence.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ADarcShadowFigure::ADarcShadowFigure()
{
	bReplicates = false; // своя у каждого игрока
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);       // тень не отбрасывает тени
	Body->SetReceivesDecals(false);
	Body->bAffectDistanceFieldLighting = false;
}

void ADarcShadowFigure::BeginPlay()
{
	Super::BeginPlay();
	if (USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")))
	{
		Body->SetSkeletalMeshAsset(Mesh);
		// Сплошная чернота без бликов: силуэт, а не человек.
		UMaterialInterface* Black = UDarcAssetSettings::MakeColorMaterial(this, FLinearColor(0.002f, 0.002f, 0.002f));
		for (int32 i = 0; i < Body->GetNumMaterials(); ++i)
		{
			Body->SetMaterial(i, Black);
		}
	}
	if (UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle")))
	{
		Body->PlayAnimation(Idle, true);
		Body->SetPlayRate(0.6f); // дышит медленнее живого
	}
}

void ADarcShadowFigure::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager || Age > LifeSeconds)
	{
		Destroy();
		return;
	}
	// Посмотрели прямо (и его ничто не загораживает) — исчезает в тот же кадр.
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Chest = GetActorLocation() + FVector(0.f, 0.f, 120.f);
	const FVector ToFigure = (Chest - Eye).GetSafeNormal();
	if (FVector::DotProduct(PC->PlayerCameraManager->GetCameraRotation().Vector(), ToFigure) > FMath::Cos(FMath::DegreesToRadians(LookAngle)))
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcShadow), false, PC->GetPawn());
		Params.AddIgnoredActor(this);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Chest, ECC_Visibility, Params))
		{
			Destroy();
		}
	}
}

ADarcShadowFigure* ADarcShadowFigure::TrySpawnNear(APlayerController* PC)
{
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!World || !PC->PlayerCameraManager)
	{
		return nullptr;
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const float ViewYaw = PC->PlayerCameraManager->GetCameraRotation().Yaw;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcShadowSpawn), false, Pawn);
	for (int32 Try = 0; Try < 24; ++Try)
	{
		// Сбоку или за спиной (90..180° от взгляда), 8–20 м: обернулся — а там кто-то стоит.
		const float Yaw = ViewYaw + FMath::FRandRange(90.f, 180.f) * (FMath::RandBool() ? 1.f : -1.f);
		const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
		const FVector Point = Eye + Dir * FMath::FRandRange(800.f, 2000.f);
		FHitResult Hit;
		// Должно быть видно с места игрока (иначе, обернувшись, его не увидеть).
		if (World->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Params))
		{
			continue;
		}
		// Пол под ним — на уровне пола игрока (не в пропасти, не на шкафу).
		if (!World->LineTraceSingleByChannel(Hit, Point, Point - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params))
		{
			continue;
		}
		const float FeetZ = Pawn->GetActorLocation().Z - Pawn->GetSimpleCollisionHalfHeight();
		if (FMath::Abs(Hit.ImpactPoint.Z - FeetZ) > 60.f)
		{
			continue;
		}
		// Место свободно на рост человека (не в стене/мебели).
		FHitResult Block;
		if (World->SweepSingleByChannel(Block, Hit.ImpactPoint + FVector(0.f, 0.f, 100.f), Hit.ImpactPoint + FVector(0.f, 0.f, 101.f),
			FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(30.f, 85.f), Params))
		{
			continue;
		}
		const FRotator Facing(0.f, (Eye - Hit.ImpactPoint).Rotation().Yaw - 90.f, 0.f); // у манекена «вперёд» — +Y
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ADarcShadowFigure>(ADarcShadowFigure::StaticClass(), Hit.ImpactPoint, Facing, Spawn);
	}
	return nullptr;
}
