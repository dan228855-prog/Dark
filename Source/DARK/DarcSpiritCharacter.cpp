// DarcSpiritCharacter.cpp
#include "DarcSpiritCharacter.h"
#include "DarcAssetSettings.h"
#include "DarcFuseBox.h"
#include "DarcGameState.h"
#include "DarcPowerConsumerComponent.h"
#include "DarcWorldMemorySubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"

ADarcSpiritCharacter::ADarcSpiritCharacter()
{
	// Видим и нужен только своему владельцу.
	bOnlyRelevantToOwner = true;

	// Сквозь игроков проходит, сквозь стены — нет (уровень остаётся уровнем).
	GetCapsuleComponent()->InitCapsuleSize(20.f, 20.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);

	// Меша нет — дух невидим.
	GetMesh()->SetVisibility(false);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->DefaultLandMovementMode = MOVE_Flying;
	Move->GravityScale = 0.f;
	Move->MaxFlySpeed = 420.f;
	Move->BrakingDecelerationFlying = 1200.f;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->bUsePawnControlRotation = true;
}

void ADarcSpiritCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxisKey(EKeys::W, this, &ADarcSpiritCharacter::MoveForwardKey);
	PlayerInputComponent->BindAxisKey(EKeys::S, this, &ADarcSpiritCharacter::MoveBackKey);
	PlayerInputComponent->BindAxisKey(EKeys::D, this, &ADarcSpiritCharacter::MoveRightKey);
	PlayerInputComponent->BindAxisKey(EKeys::A, this, &ADarcSpiritCharacter::MoveLeftKey);
	PlayerInputComponent->BindAxisKey(EKeys::SpaceBar, this, &ADarcSpiritCharacter::MoveUpKey);
	PlayerInputComponent->BindAxisKey(EKeys::LeftControl, this, &ADarcSpiritCharacter::MoveDownKey);
	PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &ADarcSpiritCharacter::LookYaw);
	PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &ADarcSpiritCharacter::LookPitch);
}

// Направление — по взгляду: дух летит туда, куда смотрит.
void ADarcSpiritCharacter::MoveForwardKey(float Value) { AddMovementInput(GetControlRotation().Vector(), Value); }
void ADarcSpiritCharacter::MoveBackKey(float Value)    { AddMovementInput(GetControlRotation().Vector(), -Value); }
void ADarcSpiritCharacter::MoveRightKey(float Value)   { AddMovementInput(FRotationMatrix(GetControlRotation()).GetScaledAxis(EAxis::Y), Value); }
void ADarcSpiritCharacter::MoveLeftKey(float Value)    { AddMovementInput(FRotationMatrix(GetControlRotation()).GetScaledAxis(EAxis::Y), -Value); }
void ADarcSpiritCharacter::MoveUpKey(float Value)      { AddMovementInput(FVector::UpVector, Value); }
void ADarcSpiritCharacter::MoveDownKey(float Value)    { AddMovementInput(FVector::UpVector, -Value); }
void ADarcSpiritCharacter::LookYaw(float Value)        { AddControllerYawInput(Value); }
void ADarcSpiritCharacter::LookPitch(float Value)      { AddControllerPitchInput(-Value); }

void ADarcSpiritCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcSpiritCharacter, CooldownEndTime, COND_OwnerOnly);
}

float ADarcSpiritCharacter::GetIntensity() const
{
	const UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this);
	const int32 Deaths = Memory ? Memory->GetCampaignDeaths() : 0;
	const int32 Level = Memory ? Memory->GetCurrentLevelIndex() : 1;

	// Ранние уровни, мало смертей — дух слабый и редкий; к 6–7 уровню и с ростом смертей — настойчивый.
	return FMath::Clamp(0.5f + 0.08f * Deaths + 0.15f * FMath::Max(0, Level - 1), 0.5f, 2.f);
}

AActor* ADarcSpiritCharacter::TraceTarget() const
{
	if (!Camera || !GetWorld())
	{
		return nullptr;
	}

	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * AbilityRange;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpiritTrace), false, this);
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) ? Hit.GetActor() : nullptr;
}

// ---------------------------------------------------------------------------
// Клиент: выбрать цель и попросить сервер
// ---------------------------------------------------------------------------

void ADarcSpiritCharacter::TryFlicker()
{
	if (AActor* Target = TraceTarget())
	{
		Server_Flicker(Target);
	}
}

void ADarcSpiritCharacter::TryToggleBreaker()
{
	if (ADarcFuseBox* FuseBox = Cast<ADarcFuseBox>(TraceTarget()))
	{
		Server_ToggleBreaker(FuseBox);
	}
}

void ADarcSpiritCharacter::TryPush()
{
	if (AActor* Target = TraceTarget())
	{
		Server_Push(Target, Camera->GetForwardVector());
	}
}

// ---------------------------------------------------------------------------
// Сервер: проверка и действие
// ---------------------------------------------------------------------------

bool ADarcSpiritCharacter::ValidateAction(const AActor* Target) const
{
	return Target
		&& GetWorld()->GetTimeSeconds() >= CooldownEndTime
		&& FVector::Dist(Target->GetActorLocation(), GetActorLocation()) <= AbilityRange * 1.2f; // запас на задержку сети
}

void ADarcSpiritCharacter::StartCooldown()
{
	CooldownEndTime = GetWorld()->GetTimeSeconds() + BaseCooldown / GetIntensity();
}

void ADarcSpiritCharacter::Server_Flicker_Implementation(AActor* Target)
{
	UDarcPowerConsumerComponent* Consumer = Target ? Target->FindComponentByClass<UDarcPowerConsumerComponent>() : nullptr;
	const bool bOk = ValidateAction(Target) && Consumer && Consumer->IsPowered();
	if (bOk)
	{
		Consumer->Multicast_Flicker(0.6f + 0.6f * GetIntensity());
		StartCooldown();
		if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
		{
			Memory->RecordInteraction(Target->GetFName(), TEXT("SpiritFlicker"), this);
		}
	}
	Client_ActionResult(bOk);
}

void ADarcSpiritCharacter::Server_ToggleBreaker_Implementation(ADarcFuseBox* FuseBox)
{
	const bool bOk = ValidateAction(FuseBox);
	if (bOk)
	{
		FuseBox->SpiritToggleBreaker(this);
		StartCooldown();
	}
	Client_ActionResult(bOk);
}

void ADarcSpiritCharacter::Server_Push_Implementation(AActor* Target, FVector_NetQuantizeNormal Direction)
{
	UPrimitiveComponent* Body = Target ? Cast<UPrimitiveComponent>(Target->GetRootComponent()) : nullptr;
	const bool bOk = ValidateAction(Target) && Body && Body->IsSimulatingPhysics() && Body->GetMass() <= MaxPushMass;
	if (bOk)
	{
		const FVector Impulse = FVector(Direction).GetSafeNormal() * BasePushImpulse * GetIntensity();

		if (Target->GetIsReplicated() && Target->IsReplicatingMovement())
		{
			// Сетевой физический предмет — толкаем на сервере, позиция уйдёт всем.
			Body->AddImpulse(Impulse, NAME_None, true);
		}
		else if (ADarcGameState* GS = GetWorld()->GetGameState<ADarcGameState>())
		{
			// Мелочь класса C (локальная физика) — толчок у каждого клиента. Конечное положение
			// может немного отличаться у разных игроков: для декоративной мелочи это допустимо.
			GS->Multicast_PushObject(Target, Impulse);
		}

		StartCooldown();
		if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
		{
			Memory->RecordInteraction(Target->GetFName(), TEXT("SpiritPush"), this);
		}
	}
	Client_ActionResult(bOk);
}

void ADarcSpiritCharacter::Client_ActionResult_Implementation(bool bSuccess)
{
	if (bSuccess)
	{
		UDarcAssetSettings::PlaySound(this, TEXT("SpiritAction"), GetActorLocation());
	}
	OnActionResult(bSuccess);
}
