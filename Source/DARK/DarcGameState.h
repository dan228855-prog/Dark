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

UCLASS()
class DARK_API ADarcGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    // Текущая фаза выезда - реплицируется всем, используется и UI, и логикой задач.
    UPROPERTY(ReplicatedUsing = OnRep_MissionPhase, BlueprintReadOnly, Category = "Mission")
    EMissionPhase MissionPhase = EMissionPhase::Preparation;

    UFUNCTION(BlueprintCallable, Category = "Mission")
    void SetMissionPhase(EMissionPhase NewPhase);

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_MissionPhase();

    UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
    void OnMissionPhaseChanged(EMissionPhase NewPhase);
};
