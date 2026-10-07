// DarcRain.cpp
#include "DarcRain.h"
#include "DarcAssetSettings.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/AmbientSound.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "EngineUtils.h"
#include "DarcIntroDrive.h"
#include "GameFramework/Pawn.h"

ADarcRain::ADarcRain()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // после камеры — струйки не отстают
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADarcRain::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}
	Random.Initialize(1234);

	Drops = NewObject<UInstancedStaticMeshComponent>(this, TEXT("RainDrops"));
	Drops->SetupAttachment(RootComponent);
	Drops->SetUsingAbsoluteLocation(true);
	Drops->SetUsingAbsoluteRotation(true);
	Drops->SetWorldLocation(FVector::ZeroVector); // инстансы — в мировых координатах
	Drops->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Drops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Drops->SetCastShadow(false);
	Drops->SetReceivesDecals(false);
	Drops->bAffectDistanceFieldLighting = false;
	Drops->SetVisibleInRayTracing(false);
	Drops->SetMobility(EComponentMobility::Movable);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DARC/Materials/M_DarcRain.M_DarcRain")))
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
		Material->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		Drops->SetMaterial(0, Material);
	}
	Drops->RegisterComponent();

	// Начальная раскладка вокруг стартовой точки — дальше переносится за камерой.
	Positions.SetNum(DropCount);
	Transforms.SetNum(DropCount);
	for (int32 i = 0; i < DropCount; ++i)
	{
		Positions[i] = GetActorLocation() + FVector(Random.FRandRange(-Radius, Radius), Random.FRandRange(-Radius, Radius), Random.FRandRange(0.f, Height));
		Transforms[i] = FTransform::Identity;
	}
	Drops->AddInstances(Transforms, false, true);

	for (TActorIterator<AAmbientSound> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Outdoor")) && It->GetAudioComponent())
		{
			OutdoorSounds.Add(It->GetAudioComponent());
			OutdoorBaseVolumes.Add(It->GetAudioComponent()->VolumeMultiplier);
		}
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Indoor")))
		{
			IndoorBoxes.Add(It->GetComponentsBoundingBox(true));
		}
	}
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		Fog = It->GetComponent();
		if (Fog)
		{
			FogBaseDensity = Fog->FogDensity;
			bFogVolumetric = Fog->bEnableVolumetricFog;
		}
		break;
	}

	if (USoundBase* Loop = UDarcAssetSettings::FindSound(SoundSlot))
	{
		Sound = UGameplayStatics::SpawnSound2D(this, Loop, Volume, 1.f, 0.f, nullptr, false, false);
	}
}

void ADarcRain::ApplyStreetVolume()
{
	auto Apply = [this](UAudioComponent* Audio, float Base)
	{
		if (!Audio)
		{
			return;
		}
		if (StreetAlpha <= 0.01f)
		{
			if (Audio->IsPlaying())
			{
				Audio->Stop();
			}
			return;
		}
		Audio->SetVolumeMultiplier(Base * StreetAlpha);
		if (!Audio->IsPlaying())
		{
			Audio->Play();
		}
	};
	Apply(Sound, Volume);
	for (int32 i = 0; i < OutdoorSounds.Num(); ++i)
	{
		Apply(OutdoorSounds[i], OutdoorBaseVolumes.IsValidIndex(i) ? OutdoorBaseVolumes[i] : 1.f);
	}
}

void ADarcRain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!Drops || !PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);

	// Под крышей? Луч вверх только по статике (машина подвижная — в ней дождь «за стеклом»).
	IndoorCheckTimer -= Dt;
	if (IndoorCheckTimer <= 0.f)
	{
		IndoorCheckTimer = 0.25f;
		FHitResult Hit;
		const FCollisionObjectQueryParams Static(ECC_WorldStatic);
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcRain), false);
		bool bRoof = false;
		if (IndoorBoxes.Num() > 0)
		{
			// Границы зданий заданы на карте — надёжнее лучей (козырёк, навес, окна их не обманут).
			for (const FBox& Box : IndoorBoxes)
			{
				bRoof |= Box.IsInsideOrOn(Camera);
			}
		}
		else
		{
		bRoof = GetWorld()->LineTraceSingleByObjectType(Hit, Camera, Camera + FVector(0.f, 0.f, 3000.f), Static, Params);
		// Одной крыши мало: под козырьком или навесом крыша есть, а вокруг улица. В помещении стены
		// почти со всех сторон — из 8 направлений хотя бы 6 упираются во что-то в пределах 20 м
		// (открытая дверь или окно наружу дают 1–2 «просвета»).
		if (bRoof)
		{
			int32 Walls = 0;
			for (int32 i = 0; i < 8; ++i)
			{
				const FVector Dir = FRotator(0.f, i * 45.f, 0.f).Vector();
				// Два луча: на уровне глаз и колен (ниже подоконников — окна не считаются «просветом»).
				const FVector Knee = Camera - FVector(0.f, 0.f, 120.f);
				Walls += (GetWorld()->LineTraceSingleByObjectType(Hit, Camera, Camera + Dir * 2000.f, Static, Params)
					|| GetWorld()->LineTraceSingleByObjectType(Hit, Knee, Knee + Dir * 2000.f, Static, Params)) ? 1 : 0;
			}
			bRoof = Walls >= 6;
		}
		}
		if (bRoof != bIndoors)
		{
			bIndoors = bRoof;
			Drops->SetVisibility(!bIndoors);
		}
		// Дождь и уличный фон не слышны ни в здании, ни в машине (сидим — пешка прицеплена к машине;
		// струйки за стеклом при этом видны).
		const APawn* Pawn = PC->GetPawn();
		const bool bInCar = Pawn && Cast<ADarcIntroDrive>(Pawn->GetAttachParentActor()) != nullptr;
		bMuted = bIndoors || bInCar;
	}

	// Плавно к цели. Было AdjustVolume(…, 0): на нулевой громкости движок может остановить звук
	// насовсем — и на улице он не возвращался. Теперь в тишине останавливаем сами, снаружи — запускаем заново.
	// Автор: затухает и возвращается за 5 секунд (скорость 0.2 в секунду).
	const float NewAlpha = FMath::FInterpConstantTo(StreetAlpha, bMuted ? 0.f : 1.f, Dt, 0.2f);
	if (NewAlpha != StreetAlpha)
	{
		StreetAlpha = NewAlpha;
		ApplyStreetVolume();
	}
	// Тумана в здании нет: плотность плавно в ноль, объёмный туман выключается (только вид, у каждого свой).
	const float NewFog = FMath::FInterpConstantTo(FogAlpha, bIndoors ? 0.f : 1.f, Dt, 0.2f);
	if (Fog && NewFog != FogAlpha)
	{
		FogAlpha = NewFog;
		Fog->SetFogDensity(FogBaseDensity * FogAlpha);
		Fog->SetVolumetricFog(bFogVolumetric && FogAlpha > 0.5f);
	}
	if (bIndoors)
	{
		return;
	}

	const FVector Fall(Wind.X * Dt, Wind.Y * Dt, -FallSpeed * Dt);
	const FQuat Tilt = FRotationMatrix::MakeFromZ(FVector(Wind.X, Wind.Y, FallSpeed).GetSafeNormal()).ToQuat();
	const FVector Scale(StreakWidth / 100.f, StreakWidth / 100.f, StreakLength / 100.f);
	const float Bottom = Camera.Z - Height * 0.4f;
	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		FVector& P = Positions[i];
		P += Fall;
		// Держим струйки в кубе вокруг камеры: вышла за край — переносим на другую сторону.
		if (P.X < Camera.X - Radius) P.X += 2.f * Radius; else if (P.X > Camera.X + Radius) P.X -= 2.f * Radius;
		if (P.Y < Camera.Y - Radius) P.Y += 2.f * Radius; else if (P.Y > Camera.Y + Radius) P.Y -= 2.f * Radius;
		if (P.Z < Bottom)
		{
			P.Z += Height;
		}
		else if (P.Z > Bottom + Height)
		{
			P.Z -= Height;
		}
		const bool bTooClose = FVector2D::DistSquared(FVector2D(P), FVector2D(Camera)) < InnerRadius * InnerRadius;
		Transforms[i] = FTransform(Tilt, P, bTooClose ? FVector::ZeroVector : Scale);
	}
	Drops->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
}
