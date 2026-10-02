// DarcGrabComponent.cpp
#include "DarcGrabComponent.h"
#include "DarcGameplayLibrary.h"
#include "CarryableItem.h"
#include "DarcPlayerState.h"
#include "DarcWorldMemorySubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

UDarcGrabComponent::UDarcGrabComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UDarcGrabComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDarcGrabComponent, GrabbedActor);
}

void UDarcGrabComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseInternal(); // игрок погиб/вышел — предмет отпускается
	Super::EndPlay(EndPlayReason);
}

bool UDarcGrabComponent::IsGrabbable(const UPrimitiveComponent* Component)
{
	const AActor* Owner = Component ? Component->GetOwner() : nullptr;
	if (!Owner || !Component->IsSimulatingPhysics())
	{
		return false;
	}
	// Только сетевые предметы: иначе у других игроков он остался бы на месте.
	if (!Owner->GetIsReplicated() || !Owner->IsReplicatingMovement())
	{
		return false;
	}
	// Предмет у кого-то в руках (E) — не отнимаем.
	const ACarryableItem* Item = Cast<ACarryableItem>(Owner);
	return !Item || !Item->CurrentHolder;
}

bool UDarcGrabComponent::GetEyes(FVector& OutLocation, FVector& OutForward) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return false;
	}
	FRotator Rotation;
	UDarcGameplayLibrary::GetAimViewPoint(Pawn, OutLocation, Rotation); // у себя — камера, на сервере — реплицированный взгляд
	OutForward = Rotation.Vector();
	return true;
}

UPrimitiveComponent* UDarcGrabComponent::TraceForGrabbable(FVector* OutHit) const
{
	FHitResult Hit;
	if (UDarcGameplayLibrary::TraceAim(Cast<APawn>(GetOwner()), GrabRange, Hit) && IsGrabbable(Hit.GetComponent()))
	{
		if (OutHit)
		{
			*OutHit = Hit.ImpactPoint;
		}
		return Hit.GetComponent();
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Клиент
// ---------------------------------------------------------------------------

void UDarcGrabComponent::StartGrab()
{
	FVector Hit;
	if (UPrimitiveComponent* Target = TraceForGrabbable(&Hit))
	{
		Server_Grab(Target->GetOwner(), Hit);
	}
}

void UDarcGrabComponent::StopGrab()
{
	if (GrabbedActor)
	{
		Server_Release();
	}
}

void UDarcGrabComponent::Throw()
{
	if (GrabbedActor)
	{
		Server_Throw();
	}
}

void UDarcGrabComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		// Подсказка в HUD: что можно схватить.
		const UPrimitiveComponent* Target = GrabbedActor ? nullptr : TraceForGrabbable();
		FocusedGrabbable = Target ? Target->GetOwner() : nullptr;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ServerTickHold(DeltaTime);
	}

	// Привязь считают и клиент (он сам предсказывает своё движение), и сервер — иначе сервер
	// поправлял бы позицию клиента рывками.
	if (GrabbedActor && Pawn && (Pawn->IsLocallyControlled() || Pawn->HasAuthority()))
	{
		ApplyTether(DeltaTime);
	}
	else if (SavedMaxWalkSpeed >= 0.f)
	{
		RestoreWalkSpeed(); // отпустили — обычная скорость
	}
}

void UDarcGrabComponent::ApplyTether(float DeltaTime)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const UPrimitiveComponent* Body = GrabbedActor ? Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent()) : nullptr;
	if (!Movement || !Body)
	{
		return;
	}

	// Тяжесть: чем массивнее предмет, тем медленнее идёт тот, кто его держит.
	if (SavedMaxWalkSpeed < 0.f)
	{
		SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
	}
	const float Heaviness = FMath::Clamp(Body->IsSimulatingPhysics() ? Body->GetMass() / HeavyMassKg : 0.f, 0.f, 1.f);
	Movement->MaxWalkSpeed = SavedMaxWalkSpeed * FMath::Lerp(1.f, HeavyWalkSpeedFactor, Heaviness);

	// Ближайшая к игроку точка предмета (по габаритам) — по горизонтали.
	const FVector PawnLocation = Character->GetActorLocation();
	const FVector Closest = Body->Bounds.GetBox().GetClosestPointTo(PawnLocation);
	FVector Away = PawnLocation - Closest;
	Away.Z = 0.f;
	const float Distance = Away.Size();
	const float SoftStart = TetherLength * FMath::Clamp(TetherSoftStart, 0.f, 0.95f);
	if (Distance <= SoftStart || Distance < KINDA_SMALL_NUMBER)
	{
		return;
	}
	Away /= Distance;

	// Сопротивление нарастает от SoftStart до полной длины: движение «от предмета» гасится
	// всё сильнее (плавно, как натягивающийся трос), к предмету и вбок — свободно.
	const float Tension = FMath::SmoothStep(0.f, 1.f, (Distance - SoftStart) / FMath::Max(TetherLength - SoftStart, 1.f));
	const float Outward = FVector::DotProduct(Movement->Velocity, Away);
	if (Outward > 0.f)
	{
		Movement->Velocity -= Away * Outward * Tension;
	}
	// Дальше полной длины (кадр, лаг, схватили издалека) — мягко подтягивает обратно.
	if (Distance > TetherLength)
	{
		Character->AddActorWorldOffset(-Away * FMath::Min(Distance - TetherLength, 200.f * DeltaTime), true);
	}
}

void UDarcGrabComponent::RestoreWalkSpeed()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (Movement && SavedMaxWalkSpeed >= 0.f)
	{
		Movement->MaxWalkSpeed = SavedMaxWalkSpeed;
	}
	SavedMaxWalkSpeed = -1.f;
}

// ---------------------------------------------------------------------------
// Сервер
// ---------------------------------------------------------------------------

void UDarcGrabComponent::Server_Grab_Implementation(AActor* Target, FVector_NetQuantize HitLocation)
{
	if (!Target || GrabbedActor)
	{
		return;
	}
	// Мёртвые («дух») не хватают.
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (const ADarcPlayerState* PS = Pawn->GetPlayerState<ADarcPlayerState>(); PS && !PS->bIsAlive)
		{
			return;
		}
	}

	UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Target->GetRootComponent());
	FVector Eyes, Forward;
	if (!IsGrabbable(Body) || !GetEyes(Eyes, Forward))
	{
		return;
	}
	// Клиенту не верим: точка захвата должна быть рядом с глазами и на самом предмете.
	const FVector Hit = HitLocation;
	if (FVector::Dist(Eyes, Hit) > GrabRange * 1.3f || FVector::Dist(Hit, Body->Bounds.Origin) > Body->Bounds.SphereRadius * 1.2f)
	{
		return;
	}

	HeldComponent = Body;
	LocalGrabPoint = Body->GetComponentTransform().InverseTransformPosition(Hit);
	HoldDistance = FMath::Clamp(FVector::Dist(Eyes, Hit), 80.f, FMath::Min(GrabRange, TetherLength));
	SavedAngularDamping = Body->GetAngularDamping();
	Body->SetAngularDamping(FMath::Max(SavedAngularDamping, 4.f)); // чтобы не крутился волчком на «верёвке»
	Body->WakeAllRigidBodies();
	GrabbedActor = Target;

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(Target->GetFName(), TEXT("Grab"), GetOwner());
	}
}

void UDarcGrabComponent::Server_Release_Implementation()
{
	ReleaseInternal();
}

void UDarcGrabComponent::Server_Throw_Implementation()
{
	UPrimitiveComponent* Body = HeldComponent.Get();
	FVector Eyes, Forward;
	if (Body && GetEyes(Eyes, Forward))
	{
		// Импульс ограничен силой игрока: лёгкое летит, тяжёлое только толкается.
		const float Mass = FMath::Max(Body->GetMass(), 0.1f);
		const float MaxDeltaV = GetEffectiveMaxForce() * 0.08f / Mass; // «рывок» ~0.08 с
		Body->AddImpulse(Forward * FMath::Min(ThrowSpeed, MaxDeltaV), NAME_None, true);
	}
	ReleaseInternal();
}

void UDarcGrabComponent::ReleaseInternal()
{
	if (UPrimitiveComponent* Body = HeldComponent.Get())
	{
		Body->SetAngularDamping(SavedAngularDamping);
	}
	HeldComponent.Reset();
	GrabbedActor = nullptr;
}

float UDarcGrabComponent::GetEffectiveMaxForce() const
{
	int32 Alive = 0;
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			const ADarcPlayerState* DarcPS = Cast<ADarcPlayerState>(PS);
			Alive += (!DarcPS || DarcPS->bIsAlive) ? 1 : 0;
		}
	}
	return MaxForce * (Alive <= 1 ? SoloForceMultiplier : 1.f);
}

void UDarcGrabComponent::ServerTickHold(float DeltaTime)
{
	UPrimitiveComponent* Body = HeldComponent.Get();
	if (!Body)
	{
		if (GrabbedActor)
		{
			ReleaseInternal(); // предмет исчез
		}
		return;
	}
	FVector Eyes, Forward;
	if (!Body->IsSimulatingPhysics() || !GetEyes(Eyes, Forward))
	{
		ReleaseInternal(); // предмет взяли в руки (E) или вставили в слот
		return;
	}

	const FVector Target = Eyes + Forward * HoldDistance;
	const FVector GrabPoint = Body->GetComponentTransform().TransformPosition(LocalGrabPoint);
	const FVector Error = Target - GrabPoint;

	if (Error.Size() > BreakDistance)
	{
		ReleaseInternal(); // упёрлись или отстали — «сорвалось»
		return;
	}

	// Пружина с демпфированием, масштабированная массой (лёгкое и тяжёлое ведут себя одинаково,
	// пока хватает силы), плюс компенсация веса — затем ограничение силой игрока.
	const float Mass = Body->GetMass();
	const FVector Velocity = Body->GetPhysicsLinearVelocityAtPoint(GrabPoint);
	const float Gravity = -GetWorld()->GetGravityZ();
	const FVector Desired = Mass * (Error * Stiffness - Velocity * Damping + FVector(0.f, 0.f, Gravity));

	// Горизонталь и вертикаль ограничиваем раздельно: иначе у тяжёлого вся сила ушла бы на
	// попытку поднять, и его нельзя было бы даже волочь. Частичный подъём уменьшает трение —
	// поэтому вдвоём тяжёлое идёт заметно легче.
	const float MaxF = GetEffectiveMaxForce();
	const FVector Horizontal = FVector(Desired.X, Desired.Y, 0.f).GetClampedToMaxSize(MaxF);
	const float Vertical = FMath::Clamp(Desired.Z, -MaxF, MaxF);
	const FVector Force(Horizontal.X, Horizontal.Y, Vertical);

	Body->AddForceAtLocation(Force, GrabPoint);
}
