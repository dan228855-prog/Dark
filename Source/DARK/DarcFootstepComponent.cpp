// DarcFootstepComponent.cpp
#include "DarcFootstepComponent.h"
#include "DarcAssetSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"

UDarcFootstepComponent::UDarcFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true); // сам по себе ничего не шлёт — нужен, чтобы появиться у клиентов
}

FName UDarcFootstepComponent::DetectSurface() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !GetWorld())
	{
		return TEXT("Concrete");
	}
	const float HalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Start = Character->GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, HalfHeight + 40.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcFootstep), false, Character);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) || !Hit.GetComponent())
	{
		return TEXT("Concrete");
	}

	// Имена материала и модели пола — по ключевым словам (импортные ассеты так и называются:
	// M_metal_plate, M_floor_tiles_02, M_asphalt_02 ...).
	FString Name;
	if (const UMaterialInterface* Material = Hit.GetComponent()->GetMaterial(0))
	{
		Name += Material->GetName();
	}
	Name += TEXT(" ") + Hit.GetComponent()->GetName();
	Name.ToLowerInline();

	struct FSurfaceKeywords { const TCHAR* Surface; TArray<const TCHAR*> Words; };
	static const FSurfaceKeywords Table[] = {
		{ TEXT("Metal"),  { TEXT("metal"), TEXT("steel"), TEXT("iron"), TEXT("grate"), TEXT("grating"), TEXT("corrugated") } },
		{ TEXT("Tile"),   { TEXT("tile"), TEXT("ceramic"), TEXT("marble") } },
		{ TEXT("Wood"),   { TEXT("wood"), TEXT("plank"), TEXT("parquet"), TEXT("strand_board"), TEXT("plywood") } },
		{ TEXT("Carpet"), { TEXT("carpet"), TEXT("rug"), TEXT("fabric") } },
		{ TEXT("Grass"),  { TEXT("grass"), TEXT("leaves"), TEXT("moss") } },
		{ TEXT("Dirt"),   { TEXT("dirt"), TEXT("soil"), TEXT("gravel"), TEXT("mud"), TEXT("sand") } },
	};
	for (const FSurfaceKeywords& Entry : Table)
	{
		for (const TCHAR* Word : Entry.Words)
		{
			if (Name.Contains(Word))
			{
				return Entry.Surface;
			}
		}
	}
	return TEXT("Concrete"); // бетон, асфальт, штукатурка и всё неизвестное
}

void UDarcFootstepComponent::PlayBodySound(FName Slot, float Volume) const
{
	if (const AActor* Owner = GetOwner())
	{
		const float HalfHeight = Cast<ACharacter>(Owner) && Cast<ACharacter>(Owner)->GetCapsuleComponent()
			? Cast<ACharacter>(Owner)->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
		UDarcAssetSettings::PlaySound(Owner, Slot, Owner->GetActorLocation() - FVector(0.f, 0.f, HalfHeight), Volume);
	}
}

void UDarcFootstepComponent::PlayStep(float Volume) const
{
	// Footstep_<Поверхность>, если такой слот заполнен, иначе общий Footstep.
	const FName SurfaceSlot(*(FString(TEXT("Footstep_")) + DetectSurface().ToString()));
	PlayBodySound(UDarcAssetSettings::HasSound(SurfaceSlot) ? SurfaceSlot : FName(TEXT("Footstep")), Volume);
}

void UDarcFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Звук нужен только там, где есть слушатель (не на выделенном сервере).
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	const FVector Velocity = Character->GetVelocity();
	const bool bOnGround = Movement->IsMovingOnGround();

	if (bOnGround && !bWasOnGround)
	{
		// Приземление: громкость — от скорости падения.
		const float FallSpeed = -LastVerticalSpeed;
		if (FallSpeed > LandMinFallSpeed)
		{
			const float Volume = FMath::GetMappedRangeValueClamped(FVector2D(LandMinFallSpeed, 1200.f), FVector2D(0.5f, 1.3f), FallSpeed);
			PlayBodySound(UDarcAssetSettings::HasSound(TEXT("Land")) ? FName(TEXT("Land")) : FName(TEXT("Footstep")), Volume);
		}
		DistanceSinceStep = 0.f;
	}
	else if (!bOnGround && bWasOnGround && Movement->IsFalling() && Velocity.Z > 150.f)
	{
		// Оторвался от земли вверх — прыжок (просто сошёл с края — без звука).
		PlayBodySound(TEXT("Jump"), 0.8f);
	}

	if (bOnGround)
	{
		const float Speed = Velocity.Size2D();
		if (Speed > MinSpeed)
		{
			const bool bCrouched = Movement->IsCrouching();
			const float Stride = FMath::GetMappedRangeValueClamped(FVector2D(200.f, 600.f), FVector2D(WalkStride, RunStride), Speed);
			DistanceSinceStep += Speed * DeltaTime;
			if (DistanceSinceStep >= Stride)
			{
				DistanceSinceStep = 0.f;
				const float Volume = FMath::GetMappedRangeValueClamped(FVector2D(100.f, 600.f), FVector2D(0.35f, 1.f), Speed);
				PlayStep(bCrouched ? Volume * 0.4f : Volume);
			}
		}
		else
		{
			// Остановился — следующий шаг сразу, как тронется (половина шага).
			DistanceSinceStep = WalkStride * 0.5f;
		}
	}

	bWasOnGround = bOnGround;
	LastVerticalSpeed = Velocity.Z;
}
