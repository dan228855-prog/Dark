// DarcGameState.cpp
#include "DarcGameState.h"
#include "Net/UnrealNetwork.h"

void ADarcGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADarcGameState, MissionPhase);
}

void ADarcGameState::SetMissionPhase(EMissionPhase NewPhase)
{
    // Вызывать только на сервере.
    MissionPhase = NewPhase;
    OnRep_MissionPhase();
}

void ADarcGameState::OnRep_MissionPhase()
{
    OnMissionPhaseChanged(MissionPhase);
}
