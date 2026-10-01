// CarryableItem.cpp
#include "CarryableItem.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

ACarryableItem::ACarryableItem()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;
}

void ACarryableItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACarryableItem, CurrentHolder);
}

bool ACarryableItem::CanInteract_Implementation(AActor* Interactor) const
{
    // Поднять может либо никем не занятый предмет, либо свой собственный (чтобы положить обратно).
    return CurrentHolder == nullptr || CurrentHolder == Interactor;
}

void ACarryableItem::OnInteract_Implementation(AActor* Interactor)
{
    // Выполняется только на сервере.
    if (CurrentHolder == Interactor)
    {
        DetachFromHolder();
        CurrentHolder = nullptr;
    }
    else if (CurrentHolder == nullptr)
    {
        CurrentHolder = Interactor;
        AttachToHolder(Interactor);
    }

    OnRep_Holder(); // на сервере OnRep не срабатывает сам - дергаем вручную для консистентности
}

FText ACarryableItem::GetInteractionPrompt_Implementation() const
{
    return CurrentHolder ? NSLOCTEXT("Carry", "Drop", "Положить") : NSLOCTEXT("Carry", "PickUp", "Поднять");
}

void ACarryableItem::ForceDrop()
{
    // Публичный метод для сценариев вроде "игрок потерял сознание - предмет падает".
    if (CurrentHolder)
    {
        DetachFromHolder();
        CurrentHolder = nullptr;
        OnRep_Holder();
    }
}

void ACarryableItem::AttachToHolder(AActor* Holder)
{
    if (ACharacter* Character = Cast<ACharacter>(Holder))
    {
        AttachToComponent(
            Character->GetMesh(),
            FAttachmentTransformRules::SnapToTargetIncludingScale,
            CarrySocketName
        );
    }
}

void ACarryableItem::DetachFromHolder()
{
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}

void ACarryableItem::OnRep_Holder()
{
    // Вызывается на клиентах при получении новой реплики CurrentHolder,
    // и вручную на сервере из OnInteract_Implementation.
    if (CurrentHolder)
    {
        AttachToHolder(CurrentHolder);
    }
    else
    {
        DetachFromHolder();
    }

    OnHolderChanged(CurrentHolder);
}
