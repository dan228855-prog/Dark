// InteractableDoor.cpp
#include "InteractableDoor.h"
#include "DarcWorldMemorySubsystem.h"
#include "Net/UnrealNetwork.h"

AInteractableDoor::AInteractableDoor()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;
}

void AInteractableDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AInteractableDoor, bIsOpen);
    DOREPLIFETIME(AInteractableDoor, bIsLocked);
}

bool AInteractableDoor::CanInteract_Implementation(AActor* Interactor) const
{
    // Взаимодействовать можно и с запертой: игрок видит «Заперто» и может дёрнуть ручку.
    // Открыть её — только альтернативными способами (ключ, код, отключение системы), они меняют bIsLocked.
    return true;
}

void AInteractableDoor::OnInteract_Implementation(AActor* Interactor)
{
    // Этот код выполняется ТОЛЬКО на сервере (вызывается из InteractionComponent::Server_Interact).
    if (bIsLocked)
    {
        Multicast_LockedAttempt(); // дёрнули ручку — звук «заперто» у всех рядом
        if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
        {
            Memory->RecordInteraction(GetMemoryId(), TEXT("LockedAttempt"), Interactor);
        }
        return;
    }

    SetDoorOpen(!bIsOpen, Interactor);
}

void AInteractableDoor::SetDoorOpen(bool bNewIsOpen, AActor* ByActor)
{
    if (!HasAuthority() || bIsOpen == bNewIsOpen)
    {
        return;
    }

    bIsOpen = bNewIsOpen;
    OnRep_IsOpen(); // на сервере OnRep не вызывается автоматически - дергаем сами для консистентности

    if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
    {
        Memory->RecordDoorState(GetMemoryId(), bIsOpen, ByActor);
    }
}

void AInteractableDoor::SetLocked(bool bNewLocked)
{
    if (HasAuthority())
    {
        bIsLocked = bNewLocked;
    }
}

FText AInteractableDoor::GetInteractionPrompt_Implementation() const
{
    return bIsLocked ? PromptLocked : (bIsOpen ? PromptClose : PromptOpen);
}

void AInteractableDoor::OnRep_IsOpen()
{
    // Вызывается на клиентах при получении новой реплики bIsOpen,
    // и вручную на сервере из SetDoorOpen.
    OnDoorStateChanged(bIsOpen);
}

void AInteractableDoor::Multicast_LockedAttempt_Implementation()
{
    OnLockedAttempt();
}
