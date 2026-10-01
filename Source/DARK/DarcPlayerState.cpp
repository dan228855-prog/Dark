// DarcPlayerState.cpp
#include "DarcPlayerState.h"
#include "Net/UnrealNetwork.h"

void ADarcPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADarcPlayerState, bIsAlive);
    DOREPLIFETIME(ADarcPlayerState, DeathCount);
}

void ADarcPlayerState::SetAlive(bool bNewIsAlive)
{
    // Вызывать только на сервере.
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
