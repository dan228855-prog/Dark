// DarcPlayerState.cpp
#include "DarcPlayerState.h"
#include "RareEventManagerComponent.h"
#include "Net/UnrealNetwork.h"

void ADarcPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADarcPlayerState, bIsAlive);
    DOREPLIFETIME(ADarcPlayerState, DeathCount);
}

void ADarcPlayerState::SetAlive(bool bNewIsAlive)
{
    // Вызывать только на сервере. Повторный вызов с тем же значением не должен
    // второй раз увеличивать счётчик смертей.
    if (!HasAuthority() || bIsAlive == bNewIsAlive)
    {
        return;
    }

    bIsAlive = bNewIsAlive;
    if (!bIsAlive)
    {
        DeathCount++;
    }
    OnRep_IsAlive();
}

void ADarcPlayerState::OnRep_IsAlive()
{
    OnAliveStateChanged(bIsAlive);
}

void ADarcPlayerState::Client_ReceiveRareEvent_Implementation(const FDarcRareEventPayload& Payload)
{
    if (URareEventManagerComponent* Manager = URareEventManagerComponent::GetRareEventManager(this))
    {
        Manager->DeliverLocally(Payload);
    }
}
