// DarcHeavyObject.cpp
#include "DarcHeavyObject.h"
#include "CarryableItem.h"
#include "DarcPlayerState.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ADarcHeavyObject::ADarcHeavyObject()
{
	bReplicates = true;
	SetReplicateMovement(true);

	// Тик нужен только серверу и только пока объект несут.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetSimulatePhysics(true);
	Body->SetMassOverrideInKg(NAME_None, 150.f, true);
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	RootComponent = Body;
}

void ADarcHeavyObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADarcHeavyObject, Carriers);
	DOREPLIFETIME(ADarcHeavyObject, bCableAttached);
}

int32 ADarcHeavyObject::GetAlivePlayerCount() const
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
	return FMath::Max(1, Alive);
}

int32 ADarcHeavyObject::GetNeededCarriers() const
{
	// Нельзя требовать больше людей, чем есть живых: соло и «остались вдвоём из пяти» всегда проходимы.
	return FMath::Clamp(RequiredCarriers, 1, GetAlivePlayerCount());
}

bool ADarcHeavyObject::CanMoveNow() const
{
	return bCableAttached && Carriers.Num() >= GetNeededCarriers();
}

bool ADarcHeavyObject::CanInteract_Implementation(AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	if (!Pawn)
	{
		return false;
	}
	if (Carriers.Contains(Pawn))
	{
		return true; // отпустить
	}
	if (!bCableAttached)
	{
		return true; // закрепить кабель
	}
	// Взяться — только пустыми руками и если есть место.
	return Carriers.Num() < MaxCarriers && ACarryableItem::FindItemHeldBy(Interactor) == nullptr;
}

void ADarcHeavyObject::OnInteract_Implementation(AActor* Interactor)
{
	// Сервер.
	APawn* Pawn = Cast<APawn>(Interactor);
	if (!Pawn)
	{
		return;
	}

	if (Carriers.Contains(Pawn))
	{
		RemoveCarrier(Pawn);
		return;
	}

	if (!bCableAttached)
	{
		bCableAttached = true;
		OnRep_Cable();
		if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
		{
			Memory->RecordInteraction(GetFName(), TEXT("CableReattached"), Interactor);
		}
		return;
	}

	if (Carriers.Num() < MaxCarriers && !ACarryableItem::FindItemHeldBy(Interactor))
	{
		AddCarrier(Pawn);
	}
}

void ADarcHeavyObject::AddCarrier(APawn* Pawn)
{
	// Запоминаем, где объект относительно игрока, в его «локальных» координатах по yaw,
	// чтобы объект поворачивал вместе с несущими.
	const FRotator Yaw(0.f, Pawn->GetActorRotation().Yaw, 0.f);
	GripOffsets.Add(Pawn, Yaw.UnrotateVector(GetActorLocation() - Pawn->GetActorLocation()));
	Carriers.Add(Pawn);
	Body->IgnoreActorWhenMoving(Pawn, true); // объект не должен «упираться» в того, кто его несёт
	UpdateCarriedState();
}

void ADarcHeavyObject::RemoveCarrier(APawn* Pawn)
{
	Carriers.Remove(Pawn);
	GripOffsets.Remove(Pawn);
	Body->IgnoreActorWhenMoving(Pawn, false);
	UpdateCarriedState();
}

void ADarcHeavyObject::ReleaseAll()
{
	if (!HasAuthority())
	{
		return;
	}
	for (APawn* Pawn : Carriers)
	{
		Body->IgnoreActorWhenMoving(Pawn, false);
	}
	Carriers.Reset();
	GripOffsets.Reset();
	UpdateCarriedState();
}

void ADarcHeavyObject::UpdateCarriedState()
{
	const bool bCarried = Carriers.Num() > 0;

	// Пока несут — двигаем сами (кинематически), отпустили — снова физика.
	if (bCarried && Body->IsSimulatingPhysics())
	{
		bPhysicsWasOn = true;
		Body->SetSimulatePhysics(false);
	}
	else if (!bCarried && bPhysicsWasOn)
	{
		bPhysicsWasOn = false;
		Body->SetSimulatePhysics(true);
	}

	SetActorTickEnabled(bCarried && HasAuthority());
	OnRep_Carriers();
}

void ADarcHeavyObject::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	// Убираем держателей, которые погибли, пропали или ушли слишком далеко.
	for (int32 i = Carriers.Num() - 1; i >= 0; --i)
	{
		APawn* Pawn = Carriers[i];
		const ADarcPlayerState* PS = Pawn ? Pawn->GetPlayerState<ADarcPlayerState>() : nullptr;
		// Без контроллера — это тело погибшего (игрок уже стал «духом»).
		const bool bGone = !IsValid(Pawn) || !Pawn->GetController() || (PS && !PS->bIsAlive)
			|| FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) > BreakDistance * 3.f;
		if (bGone)
		{
			GripOffsets.Remove(Pawn);
			if (Pawn)
			{
				Body->IgnoreActorWhenMoving(Pawn, false);
			}
			Carriers.RemoveAt(i);
			UpdateCarriedState();
		}
	}

	if (!CanMoveNow())
	{
		return; // одному в коопе не сдвинуть — ждём помощь
	}

	// Цель — средняя точка «рук» держателей. Высоту не меняем: тяжёлое тащат по полу.
	FVector Target = FVector::ZeroVector;
	for (APawn* Pawn : Carriers)
	{
		const FRotator Yaw(0.f, Pawn->GetActorRotation().Yaw, 0.f);
		Target += Pawn->GetActorLocation() + Yaw.RotateVector(GripOffsets.FindRef(Pawn));
	}
	Target /= Carriers.Num();
	Target.Z = GetActorLocation().Z;

	const FVector ToTarget = Target - GetActorLocation();

	// Рывок или тянут в разные стороны — кабель отсоединяется, все отпускают.
	if (bHasFragileCable && ToTarget.Size() > BreakDistance)
	{
		bCableAttached = false;
		OnRep_Cable();
		if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
		{
			Memory->RecordInteraction(GetFName(), TEXT("CableDetached"), nullptr);
		}
		ReleaseAll();
		return;
	}

	const bool bSolo = GetAlivePlayerCount() == 1;
	const float Speed = bSolo ? SoloSpeed : CarrySpeed;
	const FVector Step = ToTarget.GetClampedToMaxSize(Speed * DeltaSeconds);

	// Со sweep: объект упирается в стены, а не проходит сквозь них.
	SetActorLocation(GetActorLocation() + Step, true);

	CheckDelivery();
}

void ADarcHeavyObject::CheckDelivery()
{
	if (bDelivered || !DeliveryTarget || TaskIdOnDelivered.IsNone())
	{
		return;
	}
	if (FVector::Dist2D(GetActorLocation(), DeliveryTarget->GetActorLocation()) <= DeliveryRadius)
	{
		bDelivered = true;
		if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
		{
			Tasks->CompleteTask(TaskIdOnDelivered, Carriers.Num() > 0 ? Carriers[0].Get() : nullptr);
		}
	}
}

void ADarcHeavyObject::OnRep_Carriers()
{
	OnCarriersChanged();
}

void ADarcHeavyObject::OnRep_Cable()
{
	OnCableStateChanged(bCableAttached);
}

FText ADarcHeavyObject::GetInteractionPrompt_Implementation() const
{
	if (!bCableAttached)
	{
		return PromptReattachCable;
	}
	// Подсказка считается на клиенте, а держатели реплицируются — можно показать «Отпустить».
	for (const APawn* Pawn : Carriers)
	{
		if (Pawn && Pawn->IsLocallyControlled())
		{
			return PromptRelease;
		}
	}
	return PromptGrab;
}
