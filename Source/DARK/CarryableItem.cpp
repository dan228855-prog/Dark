// CarryableItem.cpp
#include "CarryableItem.h"
#include "DarcWorldMemorySubsystem.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"

ACarryableItem::ACarryableItem()
{
    bReplicates = true;
    // Позиция и привязка реплицируются с сервера. Без этого после «положить»
    // каждый клиент оставлял бы предмет там, где посчитал сам, и позиции расходились бы.
    SetReplicateMovement(true);
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
        ServerRelease(Interactor);
        return;
    }

    if (CurrentHolder == nullptr)
    {
        PickUpTransform = GetActorTransform();
        CurrentHolder = Interactor;
        AttachToHolder(Interactor);
        OnRep_Holder(); // на сервере OnRep не срабатывает сам - дергаем вручную для консистентности
    }
}

void ACarryableItem::ServerRelease(AActor* ByActor)
{
    if (!HasAuthority() || !CurrentHolder)
    {
        return;
    }

    DetachFromHolder();
    CurrentHolder = nullptr;
    OnRep_Holder();

    if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
    {
        Memory->RecordItemMoved(GetMemoryId(), PickUpTransform, GetActorTransform(), ByActor);
    }
}

FText ACarryableItem::GetInteractionPrompt_Implementation() const
{
    return CurrentHolder ? PromptDrop : PromptPickUp;
}

void ACarryableItem::ForceDrop()
{
    // Публичный метод для сценариев вроде "игрок потерял сознание - предмет падает".
    // Только сервер: владение предметом — серверное состояние.
    ServerRelease(CurrentHolder);
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

ACarryableItem* ACarryableItem::FindItemHeldBy(const AActor* Holder)
{
    if (!Holder || !Holder->GetWorld())
    {
        return nullptr;
    }

    for (TActorIterator<ACarryableItem> It(Holder->GetWorld()); It; ++It)
    {
        if (It->CurrentHolder == Holder)
        {
            return *It;
        }
    }
    return nullptr;
}
