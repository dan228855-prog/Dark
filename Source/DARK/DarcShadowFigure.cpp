// DarcShadowFigure.cpp
#include "DarcShadowFigure.h"
#include "DarcAssetSettings.h"
#include "DarcFlashlightComponent.h"
#include "DarcVitalsComponent.h"
#include "Kismet/GameplayStatics.h"
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
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !Pawn || !PC->PlayerCameraManager || Age > LifeSeconds)
	{
		Destroy();
		return;
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector ViewDir = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector Chest = GetActorLocation() + FVector(0.f, 0.f, 120.f);

	// Подошла вплотную — шёпот у самого уха.
	if (FVector::Dist2D(Eye, GetActorLocation()) < TouchDistance)
	{
		Vanish(true);
		return;
	}
	// Посмотрели прямо (и её ничто не загораживает) — исчезает в тот же кадр.
	if (FVector::DotProduct(ViewDir, (Chest - Eye).GetSafeNormal()) > FMath::Cos(FMath::DegreesToRadians(LookAngle)))
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcShadow), false, Pawn);
		Params.AddIgnoredActor(this);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Chest, ECC_Visibility, Params))
		{
			Vanish(false);
			return;
		}
	}
	if (Age >= NextStep)
	{
		NextStep = Age + StepInterval;
		StepCloser(Eye, ViewDir, Pawn);
	}
}

void ADarcShadowFigure::StepCloser(const FVector& Eye, const FVector& ViewDir, const APawn* Pawn)
{
	const FVector ToPlayer = (Eye - GetActorLocation()).GetSafeNormal2D();
	const float Remaining = FVector::Dist2D(Eye, GetActorLocation());
	for (int32 Try = 0; Try < 6; ++Try)
	{
		// Прямо к игроку, а если там стена или туда смотрят — чуть в сторону.
		const float Side = Try == 0 ? 0.f : FMath::FRandRange(-45.f, 45.f);
		const FVector Dir = ToPlayer.RotateAngleAxis(Side, FVector::UpVector);
		const FVector Point = GetActorLocation() + Dir * FMath::Min(StepDistance, Remaining - TouchDistance * 0.5f) + FVector(0.f, 0.f, 60.f);
		// Шагает, только пока на это место не смотрят (вне 60° от взгляда).
		if (FVector::DotProduct(ViewDir, (Point - Eye).GetSafeNormal()) > FMath::Cos(FMath::DegreesToRadians(60.f)))
		{
			continue;
		}
		FVector Floor;
		if (IsStandable(GetWorld(), Point, Eye, Pawn, Floor))
		{
			SetActorLocationAndRotation(Floor, FRotator(0.f, (Eye - Floor).Rotation().Yaw - 90.f, 0.f));
			return;
		}
	}
}

void ADarcShadowFigure::Vanish(bool bTouched)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UDarcVitalsComponent* Vitals = Pawn ? Pawn->FindComponentByClass<UDarcVitalsComponent>() : nullptr;
	if (bTouched)
	{
		// Шёпот — с её места (рядом, за плечом), фонарь мигает.
		UDarcAssetSettings::PlaySound(this, TEXT("ShadowWhisper"), GetActorLocation() + FVector(0.f, 0.f, 160.f), 0.9f);
		if (UDarcFlashlightComponent* Flashlight = Pawn ? Pawn->FindComponentByClass<UDarcFlashlightComponent>() : nullptr)
		{
			Flashlight->FlickerFor(1.6f);
		}
		if (Vitals)
		{
			Vitals->AddFear(0.35f);
		}
	}
	else
	{
		// Увидел — иногда тихий удар музыки (не каждый раз: к нему не привыкнуть).
		if (FMath::FRand() < 0.4f)
		{
			if (USoundBase* Sting = UDarcAssetSettings::FindSoundVariant(TEXT("ShadowSting")))
			{
				UGameplayStatics::PlaySound2D(this, Sting, 0.45f);
			}
		}
		if (Vitals)
		{
			Vitals->AddFear(0.15f);
		}
	}
	Destroy();
}

bool ADarcShadowFigure::IsStandable(UWorld* World, const FVector& Point, const FVector& Eye, const APawn* Pawn, FVector& OutFloor)
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcShadowSpawn), false, Pawn);
	FHitResult Hit;
	// Должно быть видно с места игрока (иначе, обернувшись, её не увидеть).
	if (World->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Params))
	{
		return false;
	}
	// Пол под ней — на уровне пола игрока (не в пропасти, не на шкафу).
	if (!World->LineTraceSingleByChannel(Hit, Point, Point - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params))
	{
		return false;
	}
	const float FeetZ = Pawn->GetActorLocation().Z - Pawn->GetSimpleCollisionHalfHeight();
	if (FMath::Abs(Hit.ImpactPoint.Z - FeetZ) > 60.f)
	{
		return false;
	}
	// Место свободно на рост человека (не в стене/мебели).
	FHitResult Block;
	if (World->SweepSingleByChannel(Block, Hit.ImpactPoint + FVector(0.f, 0.f, 100.f), Hit.ImpactPoint + FVector(0.f, 0.f, 101.f),
		FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(30.f, 85.f), Params))
	{
		return false;
	}
	OutFloor = Hit.ImpactPoint;
	return true;
}

ADarcShadowFigure* ADarcShadowFigure::TrySpawnNear(APlayerController* PC, const FVector* NoiseLocation)
{
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!World || !PC->PlayerCameraManager)
	{
		return nullptr;
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector ViewDir = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const float ViewYaw = PC->PlayerCameraManager->GetCameraRotation().Yaw;
	for (int32 Try = 0; Try < 36; ++Try)
	{
		FVector Point;
		if (NoiseLocation && Try < 12)
		{
			// Сначала — рядом с тем, что шумело: пришла на звук.
			Point = *NoiseLocation + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(150.f, 600.f) + FVector(0.f, 0.f, 60.f);
			if (FVector::Dist(Point, Eye) < 700.f || FVector::DotProduct(ViewDir, (Point - Eye).GetSafeNormal()) > FMath::Cos(FMath::DegreesToRadians(70.f)))
			{
				continue;
			}
		}
		else
		{
			// Сбоку или за спиной (80..180° от взгляда): обернулся — а там кто-то стоит. Сначала ищем
			// подальше (8–20 м), в тесных коридорах и комнатах — ближе (от 4 м).
			const float Yaw = ViewYaw + FMath::FRandRange(80.f, 180.f) * (FMath::RandBool() ? 1.f : -1.f);
			Point = Eye + FRotator(0.f, Yaw, 0.f).Vector() * FMath::FRandRange(Try < 24 ? 800.f : 400.f, 2000.f);
		}
		FVector Floor;
		if (!IsStandable(World, Point, Eye, Pawn, Floor))
		{
			continue;
		}
		const FRotator Facing(0.f, (Eye - Floor).Rotation().Yaw - 90.f, 0.f); // у манекена «вперёд» — +Y
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADarcShadowFigure* Figure = World->SpawnActor<ADarcShadowFigure>(ADarcShadowFigure::StaticClass(), Floor, Facing, Spawn);
		if (Figure)
		{
			Figure->NextStep = Figure->StepInterval;
		}
		return Figure;
	}
	return nullptr;
}
