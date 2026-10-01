// DarcGameState.cpp
#include "DarcGameState.h"
#include "TaskManagerComponent.h"
#include "RareEventManagerComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/PrimitiveComponent.h"

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

void ADarcGameState::Multicast_PushObject_Implementation(AActor* Target, FVector Impulse)
{
    // Ссылка на нереплицируемый предмет доходит, только если он размещён в уровне
    // (стабильное имя). Заспавненная локально мелочь придёт как null — просто пропускаем.
    if (UPrimitiveComponent* Body = Target ? Cast<UPrimitiveComponent>(Target->GetRootComponent()) : nullptr)
    {
        if (Body->IsSimulatingPhysics())
        {
            Body->AddImpulse(Impulse, NAME_None, true);
        }
    }
}
