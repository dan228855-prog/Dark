// DarcVitalsComponent.cpp
#include "DarcVitalsComponent.h"
#include "DarcGameMode.h"
#include "DarcGrabComponent.h"
#include "DarcGameState.h"
#include "DarcAssetSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

UDarcVitalsComponent::UDarcVitalsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UDarcVitalsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDarcVitalsComponent, Health);
	DOREPLIFETIME_CONDITION(UDarcVitalsComponent, Stamina, COND_OwnerOnly);
	DOREPLIFETIME(UDarcVitalsComponent, bSprintPressed);
	DOREPLIFETIME(UDarcVitalsComponent, bExhausted);
	DOREPLIFETIME_CONDITION(UDarcVitalsComponent, LastDamageTime, COND_OwnerOnly);
	DOREPLIFETIME(UDarcVitalsComponent, Statuses);
}

void UDarcVitalsComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	Stamina = MaxStamina;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		BaseWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
		if (GetOwner()->HasAuthority())
		{
			GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UDarcVitalsComponent::HandleAnyDamage);
			Character->LandedDelegate.AddDynamic(this, &UDarcVitalsComponent::HandleLanded);
		}
	}
}

float UDarcVitalsComponent::GetSecondsSinceDamage() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() - LastDamageTime : 100.f;
}

void UDarcVitalsComponent::SetStatus(FName Status, bool bActive)
{
	if (bActive)
	{
		Statuses.AddUnique(Status);
	}
	else
	{
		Statuses.Remove(Status);
	}
}

void UDarcVitalsComponent::SetSprintPressed(bool bPressed)
{
	bSprintPressed = bPressed; // сразу у себя — скорость меняется без задержки сети
	Server_SetSprint(bPressed);
}

void UDarcVitalsComponent::Server_SetSprint_Implementation(bool bPressed)
{
	bSprintPressed = bPressed;
}

bool UDarcVitalsComponent::IsSprinting() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !bSprintPressed || bExhausted || bDead)
	{
		return false;
	}
	// Бежим только по земле и вперёд-в сторону — стоя на месте выносливость не тратится.
	return Character->GetCharacterMovement()->IsMovingOnGround() && Character->GetVelocity().Size2D() > BaseWalkSpeed * 0.5f;
}

void UDarcVitalsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || BaseWalkSpeed <= 0.f)
	{
		return;
	}
	const bool bSprinting = IsSprinting();
	if (Character->IsLocallyControlled())
	{
		TickFear(DeltaTime);
	}

	// Скорость — у сервера и у владельца одинаково (иначе движение дёргается). Пока тащим
	// тяжёлое, скоростью управляет захват — не вмешиваемся.
	const UDarcGrabComponent* Grab = Character->FindComponentByClass<UDarcGrabComponent>();
	const bool bHolding = Grab && Grab->IsHolding();
	if (!bHolding && (Character->HasAuthority() || Character->IsLocallyControlled()))
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * ((bSprinting && Stamina > 0.f) ? SprintSpeedFactor : 1.f);
	}

	if (!Character->HasAuthority())
	{
		return;
	}
	Stamina = FMath::Clamp(Stamina + (bSprinting ? -StaminaDrain : StaminaRegen) * DeltaTime, 0.f, MaxStamina);
	if (Stamina <= 0.f)
	{
		bExhausted = true;
	}
	else if (bExhausted && Stamina >= SprintResumeStamina)
	{
		bExhausted = false;
	}
	if (!bDead && Health < MaxHealth && GetSecondsSinceDamage() > RegenDelay)
	{
		Health = FMath::Min(Health + HealthRegen * DeltaTime, MaxHealth);
	}
}

void UDarcVitalsComponent::HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	ApplyHealthDamage(Damage);
}

void UDarcVitalsComponent::HandleLanded(const FHitResult& Hit)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const float Speed = Character ? -Character->GetCharacterMovement()->Velocity.Z : 0.f;
	if (Speed > FallDamageSpeed)
	{
		ApplyHealthDamage((Speed - FallDamageSpeed) / 100.f * FallDamagePer100);
	}
}

void UDarcVitalsComponent::ApplyHealthDamage(float Amount)
{
	if (bDead || bGodMode || Amount <= 0.f)
	{
		return;
	}
	Health = FMath::Max(Health - Amount, 0.f);
	if (const AGameStateBase* GS = GetWorld()->GetGameState())
	{
		LastDamageTime = GS->GetServerWorldTimeSeconds();
	}
	if (Health <= 0.f)
	{
		bDead = true;
		APawn* Pawn = Cast<APawn>(GetOwner());
		if (ADarcGameMode* GM = GetWorld()->GetAuthGameMode<ADarcGameMode>())
		{
			GM->KillPlayer(Pawn ? Pawn->GetController() : nullptr, nullptr);
		}
	}
}

float UDarcVitalsComponent::GetBaseFear() const
{
	return FMath::Clamp(0.05f + 0.07f * (ADarcGameState::GetMissionLevel(this) - 1), 0.f, 0.45f);
}

void UDarcVitalsComponent::AddFear(float Amount)
{
	Fear = FMath::Clamp(FMath::Max(Fear, GetBaseFear()) + Amount, 0.f, 1.f);
}

void UDarcVitalsComponent::TickFear(float DeltaTime)
{
	const float Base = GetBaseFear();
	const float Before = FearLastTick; // испуг (AddFear) мог случиться между кадрами
	// Отпускает медленно: от сильного испуга до спокойствия — около минуты.
	Fear = Fear < Base ? Base : FMath::FInterpConstantTo(Fear, Base, DeltaTime, 0.012f);
	// Перешёл черту — тяжёлое дыхание (у себя, «в голове»), не чаще раза в 20 с.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Before < 0.6f && Fear >= 0.6f && Now >= NextBreathTime)
	{
		NextBreathTime = Now + 20.f;
		if (USoundBase* Breath = UDarcAssetSettings::FindSoundVariant(TEXT("FearBreath")))
		{
			UGameplayStatics::PlaySound2D(this, Breath, 0.55f);
		}
	}
	FearLastTick = Fear;
}
