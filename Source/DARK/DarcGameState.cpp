// DarcGameState.cpp
#include "DarcGameState.h"
#include "TaskManagerComponent.h"
#include "RareEventManagerComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

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

void ADarcGameState::Say(const FText& Speaker, const FText& Line, float Duration, FVector Location, float Radius)
{
    if (HasAuthority() && !Line.IsEmpty())
    {
        Multicast_Subtitle(Speaker, Line, Duration, Location, Radius);
    }
}

void ADarcGameState::Multicast_Subtitle_Implementation(const FText& Speaker, const FText& Line, float Duration, FVector_NetQuantize Location, float Radius)
{
    // Фильтр по дистанции — на клиенте: реплика не секретная, а так не нужен список адресатов.
    if (Radius > 0.f)
    {
        const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
        const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        if (!Pawn || FVector::Dist(Pawn->GetActorLocation(), Location) > Radius)
        {
            return;
        }
    }
    OnSubtitle.Broadcast(Speaker, Line, Duration);
}
