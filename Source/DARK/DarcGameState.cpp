// DarcGameState.cpp
#include "DarcGameState.h"
#include "TaskManagerComponent.h"
#include "RareEventManagerComponent.h"
#include "Net/UnrealNetwork.h"

ADarcGameState::ADarcGameState()
{
    TaskManager = CreateDefaultSubobject<UTaskManagerComponent>(TEXT("TaskManager"));
    RareEventManager = CreateDefaultSubobject<URareEventManagerComponent>(TEXT("RareEventManager"));
}

void ADarcGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADarcGameState, MissionPhase);
}

void ADarcGameState::SetMissionPhase(EMissionPhase NewPhase)
{
    if (!HasAuthority() || MissionPhase == NewPhase)
    {
        return;
    }
    MissionPhase = NewPhase;
    OnRep_MissionPhase();
}

void ADarcGameState::OnRep_MissionPhase()
{
    OnMissionPhaseChanged(MissionPhase);
}
