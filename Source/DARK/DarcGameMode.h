// DarcGameMode.h
// Game Mode выезда (существует только на сервере). Отвечает за правила сессии:
// подключение игроков, смерть и превращение в «духа», провал при гибели всех.
// Старт задания — UTaskManagerComponent::StartMission (вызывается отсюда из Blueprint,
// когда команда готова).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DarcGameMode.generated.h"

class ADarcSpiritCharacter;

UCLASS(Config = Game)
class DARK_API ADarcGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ADarcGameMode();

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void StartPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

    /** Кем становится погибший. Можно заменить Blueprint-наследником (звук, пост-эффект для духа). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death")
    TSubclassOf<ADarcSpiritCharacter> SpiritClass;

    /**
     * Гибель игрока: опасности (ток, обвал, ловушка, аномалия) вызывают это на сервере.
     * Тело остаётся в мире, предмет из рук падает, игрок становится «духом».
     * Если живых не осталось — выезд проваливается.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Death")
    void KillPlayer(AController* Victim, AActor* Cause);

protected:
    /** Тело погибшего: рэгдолл, звук, свет — Blueprint. Cause — что убило (может быть null). */
    UFUNCTION(BlueprintImplementableEvent, Category = "Death")
    void OnPlayerDied(APawn* Body, AController* Victim, AActor* Cause);

    bool AnyPlayerAlive() const;

    /** Срез собран кодом (пустая карта) — игроки появляются у КПП. */
    bool bSliceBuilt = false;
    int32 SpawnedPlayers = 0;

    void AutoStartMission();
    void WatchMissionEnd();
    void ReturnToBase();

    /** Карта базы: после выезда команда возвращается сюда (пусто — остаёмся на месте, как в срезе). */
    UPROPERTY(Config, EditAnywhere, Category = "Base")
    FString BaseMap;

    /** Секунд от конца выезда до затемнения и переезда на базу. */
    UPROPERTY(Config, EditAnywhere, Category = "Base")
    float ReturnToBaseDelay = 8.f;

    FTimerHandle ReturnTimer;
    FTimerHandle StartTimer;
    FTimerHandle EndWatchTimer;
    bool bEndLinesSaid = false;
};
