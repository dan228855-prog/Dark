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

/** Субтитр: кто говорит и что. Тексты — из String Table. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDarcSubtitle, const FText&, Speaker, const FText&, Line, float, Duration);

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

    /** У каждого клиента, который слышит реплику, — HUD подписывается и показывает субтитр. */
    UPROPERTY(BlueprintAssignable, Category = "Subtitles")
    FOnDarcSubtitle OnSubtitle;

    /**
     * Сервер: сказать реплику. Слышат игроки (и «духи») в радиусе Radius от Location;
     * Radius <= 0 — слышат все (громкая связь, рация, куратор на базе).
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Subtitles")
    void Say(const FText& Speaker, const FText& Line, float Duration, FVector Location, float Radius = 0.f);

    // Толчок локального физического предмета (класс C) у всех клиентов — полтергейст духа.
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PushObject(AActor* Target, FVector Impulse);

protected:
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_Subtitle(const FText& Speaker, const FText& Line, float Duration, FVector_NetQuantize Location, float Radius);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_MissionPhase();

    UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
    void OnMissionPhaseChanged(EMissionPhase NewPhase);
};
