// DarcGameMode.cpp
#include "DarcGameMode.h"
#include "DarcGameState.h"
#include "DarcPlayerState.h"
#include "DarcSpiritCharacter.h"
#include "DarcWorldMemorySubsystem.h"
#include "CarryableItem.h"
#include "TaskManagerComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

ADarcGameMode::ADarcGameMode()
{
    GameStateClass = ADarcGameState::StaticClass();
    PlayerStateClass = ADarcPlayerState::StaticClass();
    SpiritClass = ADarcSpiritCharacter::StaticClass();
}

void ADarcGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    // Подключившийся посреди выезда получает задачи, двери и предметы обычной репликацией
    // GameState и акторов — отдельная синхронизация не нужна.
}

void ADarcGameMode::KillPlayer(AController* Victim, AActor* Cause)
{
    ADarcPlayerState* PS = Victim ? Victim->GetPlayerState<ADarcPlayerState>() : nullptr;
    APawn* Body = Victim ? Victim->GetPawn() : nullptr;
    if (!PS || !PS->bIsAlive || !Body)
    {
        return;
    }

    // 1. Предмет из рук падает — его может поднять другой (носитель с архивом не теряется).
    if (ACarryableItem* Held = ACarryableItem::FindItemHeldBy(Body))
    {
        Held->ForceDrop();
    }

    // 2. Состояние и память (до смены пешки — чтобы память знала, кто это был).
    PS->SetAlive(false);
    if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
    {
        Memory->RecordPlayerDeath(Body);
    }

    // 3. Тело остаётся в мире, игрок переходит в «духа» на том же месте.
    const FVector SpiritLocation = Body->GetActorLocation() + FVector(0.f, 0.f, 60.f);
    const FRotator SpiritRotation = Victim->GetControlRotation();
    Victim->UnPossess();
    OnPlayerDied(Body, Victim, Cause);

    if (SpiritClass)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        Params.Owner = Victim;
        if (ADarcSpiritCharacter* Spirit = GetWorld()->SpawnActor<ADarcSpiritCharacter>(SpiritClass, SpiritLocation, SpiritRotation, Params))
        {
            Victim->Possess(Spirit);
        }
    }

    // 4. Погибли все — выезд провален (тяжёлые объекты и терминалы отпускают мёртвых сами).
    if (!AnyPlayerAlive())
    {
        if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
        {
            Tasks->FailMission();
        }
    }
}

bool ADarcGameMode::AnyPlayerAlive() const
{
    if (const AGameStateBase* GS = GameState)
    {
        for (const APlayerState* PS : GS->PlayerArray)
        {
            const ADarcPlayerState* DarcPS = Cast<ADarcPlayerState>(PS);
            if (DarcPS && DarcPS->bIsAlive)
            {
                return true;
            }
        }
    }
    return false;
}
