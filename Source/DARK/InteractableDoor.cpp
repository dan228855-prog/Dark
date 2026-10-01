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
}

bool AInteractableDoor::CanInteract_Implementation(AActor* Interactor) const
{
    // Запертую дверь так просто не открыть - альтернативные способы
    // (ключ, код, отключение системы) реализуются отдельно и меняют bIsLocked.
    return !bIsLocked;
}

void AInteractableDoor::OnInteract_Implementation(AActor* Interactor)
{
    // Этот код выполняется ТОЛЬКО на сервере (вызывается из InteractionComponent::Server_Interact).
    if (bIsLocked)
    {
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
