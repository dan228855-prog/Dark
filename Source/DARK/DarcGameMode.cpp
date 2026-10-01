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
#include "GameFramework/PlayerStart.h"
#include "DarcAssetSettings.h"
#include "DarcGameplayLibrary.h"
#include "DarcHUD.h"
#include "DarcPlayerController.h"
#include "DarcSliceBuilder.h"
#include "RareEventManagerComponent.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ADarcGameMode::ADarcGameMode()
{
    GameStateClass = ADarcGameState::StaticClass();
    PlayerStateClass = ADarcPlayerState::StaticClass();
    PlayerControllerClass = ADarcPlayerController::StaticClass();
    HUDClass = ADarcHUD::StaticClass();
    SpiritClass = ADarcSpiritCharacter::StaticClass();

    // Персонаж из шаблона First Person (руки, камера, ходьба уже настроены в его Blueprint).
    static ConstructorHelpers::FClassFinder<APawn> TemplatePawn(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
    if (TemplatePawn.Succeeded())
    {
        DefaultPawnClass = TemplatePawn.Class;
    }
}

void ADarcGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    // Пустая карта (нет ни точек появления, ни комнат) — строим серый срез кодом.
    if (!UDarcAssetSettings::Get()->bAutoBuildSlice)
    {
        return;
    }
    if (TActorIterator<APlayerStart>(GetWorld()))
    {
        return; // своя карта со своими точками появления — ничего не строим
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (GetWorld()->SpawnActor<ADarcSliceBuilder>(ADarcSliceBuilder::StaticClass(), FTransform::Identity, Params))
    {
        bSliceBuilt = true;
    }
}

void ADarcGameMode::StartPlay()
{
    Super::StartPlay();

    // Таблица редких событий — из настроек (её создаёт Tools/darc_setup.py), если не задана в Blueprint.
    if (ADarcGameState* GS = GetGameState<ADarcGameState>())
    {
        if (GS->RareEventManager && !GS->RareEventManager->EventTable)
        {
            GS->RareEventManager->EventTable = UDarcAssetSettings::Get()->RareEventTable.LoadSynchronous();
        }
    }

    if (UDarcAssetSettings::Get()->bAutoStartMission)
    {
        GetWorldTimerManager().SetTimer(StartTimer, this, &ADarcGameMode::AutoStartMission, 3.f, false);
    }
}

AActor* ADarcGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    if (!bSliceBuilt)
    {
        return Super::ChoosePlayerStart_Implementation(Player);
    }
    // Точка появления у КПП, своя для каждого игрока.
    const FTransform Spawn = ADarcSliceBuilder::GetPlayerSpawn(SpawnedPlayers++);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    return GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Spawn, Params);
}

void ADarcGameMode::AutoStartMission()
{
    UDarcMissionDefinition* Mission = UDarcAssetSettings::Get()->SliceMission.LoadSynchronous();
    UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this);
    ADarcGameState* GS = GetGameState<ADarcGameState>();
    if (!Mission || !Tasks || !GS)
    {
        UE_LOG(LogTemp, Warning, TEXT("DARC: slice mission asset not found - run Tools/darc_setup.py in the editor"));
        return;
    }

    Tasks->StartMission(Mission, 0);

    // Брифинг D.A.R.C. — сухо, по рации (слышат все).
    GS->Say(UDarcGameplayLibrary::UIText(TEXT("Speaker_DARC")), UDarcGameplayLibrary::UIText(TEXT("Brief_NoQuestions")), 4.f, FVector::ZeroVector, 0.f);
    FTimerHandle Brief2;
    GetWorldTimerManager().SetTimer(Brief2, [this]()
    {
        if (ADarcGameState* State = GetGameState<ADarcGameState>())
        {
            State->Say(UDarcGameplayLibrary::UIText(TEXT("Speaker_DARC")), UDarcGameplayLibrary::UIText(TEXT("Brief_Slice")), 5.f, FVector::ZeroVector, 0.f);
        }
    }, 4.5f, false);

    GetWorldTimerManager().SetTimer(EndWatchTimer, this, &ADarcGameMode::WatchMissionEnd, 1.f, true);
}

void ADarcGameMode::WatchMissionEnd()
{
    ADarcGameState* GS = GetGameState<ADarcGameState>();
    if (!GS || GS->MissionPhase != EMissionPhase::Completed || bEndLinesSaid)
    {
        return;
    }
    bEndLinesSaid = true;
    GetWorldTimerManager().ClearTimer(EndWatchTimer);

    // Возвращение на базу в срезе — разговором с куратором по рации (сама база — следующий этап).
    TArray<FName> Lines = { TEXT("Curator_GotIt"), TEXT("Curator_Good"), TEXT("Curator_NextLater") };
    if (const UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
    {
        if (Memory->HasCampaignFact(TEXT("Slice.ExtraFileTaken")))
        {
            Lines.Add(TEXT("Curator_ExtraFile"));
        }
        if (Memory->HasCampaignFact(TEXT("Slice.BlackoutHappened")))
        {
            Lines.Add(TEXT("Curator_Maintenance"));
        }
    }

    float Delay = 5.f;
    for (const FName& Key : Lines)
    {
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, [this, Key]()
        {
            if (ADarcGameState* State = GetGameState<ADarcGameState>())
            {
                State->Say(UDarcGameplayLibrary::UIText(TEXT("Speaker_Curator")), UDarcGameplayLibrary::UIText(Key), 3.5f, FVector::ZeroVector, 0.f);
            }
        }, Delay, false);
        Delay += 4.f;
    }
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
