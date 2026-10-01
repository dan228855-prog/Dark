// DarcGameState.h
// Общее для всех игроков состояние текущего выезда.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DarcGameState.generated.h"

UENUM(BlueprintType)
enum class EMissionPhase : uint8
{
    Preparation UMETA(DisplayName = "Подготовка"),
    InProgress  UMETA(DisplayName = "Выполняется"),
    Completed   UMETA(DisplayName = "Завершено"),
    Failed      UMETA(DisplayName = "Провалено")
};

class UTaskManagerComponent;
class URareEventManagerComponent;

UCLASS()
class DARK_API ADarcGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ADarcGameState();

    // Задачи выезда — общие для команды, реплицируются всем.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    TObjectPtr<UTaskManagerComponent> TaskManager;

    // Редкие события. Таблицу событий (EventTable) назначить в Blueprint-наследнике GameState.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    TObjectPtr<URareEventManagerComponent> RareEventManager;

    // Текущая фаза выезда - реплицируется всем, используется и UI, и логикой задач.
    UPROPERTY(ReplicatedUsing = OnRep_MissionPhase, BlueprintReadOnly, Category = "Mission")
    EMissionPhase MissionPhase = EMissionPhase::Preparation;

    // Меняет только сервер. Обычно вызывается из TaskManager, а не напрямую.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mission")
    void SetMissionPhase(EMissionPhase NewPhase);

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_MissionPhase();

    UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
    void OnMissionPhaseChanged(EMissionPhase NewPhase);
};
