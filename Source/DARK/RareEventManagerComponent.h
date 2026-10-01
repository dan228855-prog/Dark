// RareEventManagerComponent.h
// RareEventManager — сервер решает, случится ли редкое событие, где и с кем.
// Живёт компонентом на ADarcGameState: у GameState уже есть репликация на всех клиентов,
// поэтому multicast-доставка «бесплатная», а для событий «видит только один игрок»
// используется Client RPC на его ADarcPlayerState.
//
// Правила из документа «Редкие события» v1.1, которые здесь соблюдаются кодом:
// - максимум одно сильное событие на уровень (MaxStrongEventsPerMission);
// - на некоторых уровнях сильных нет вовсе (bAllowStrongEvents из описания выезда);
// - события не блокируют прохождение: менеджер ничего не запирает и не отнимает,
//   а «эхо» трогает только двери/предметы, уже затронутые игроками;
// - редкость: глобальная пауза между событиями (MinSecondsBetweenEvents).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarcRareEventTypes.h"
#include "DarcWorldMemorySubsystem.h"
#include "RareEventManagerComponent.generated.h"

class UDataTable;
class ADarcRareEventAnchor;
class ADarcPlayerState;

/** Контекст проверки — что именно произошло (только сервер, не UHT-структура). */
struct FDarcRareEventEvalContext
{
	EDarcRareEventTrigger Trigger = EDarcRareEventTrigger::Periodic;
	FName ContextId;               // комната / задача
	int32 PlayerId = INDEX_NONE;   // кто вызвал триггер
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDarcRareEventReceived, const FDarcRareEventPayload&, Payload);

UCLASS(ClassGroup = (DARC), meta = (BlueprintSpawnableComponent))
class DARK_API URareEventManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URareEventManagerComponent();

	UFUNCTION(BlueprintPure, Category = "RareEvent", meta = (WorldContext = "WorldContextObject"))
	static URareEventManagerComponent* GetRareEventManager(const UObject* WorldContextObject);

	/** Таблица событий (строки FDarcRareEventRow). Назначается в Blueprint-наследнике GameState. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	TObjectPtr<UDataTable> EventTable;

	/** Как часто проверять периодические события, сек. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent", meta = (ClampMin = "1"))
	float EvaluationInterval = 15.f;

	/** Минимальная пауза между любыми двумя событиями, сек — чтобы события оставались редкими. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent", meta = (ClampMin = "0"))
	float MinSecondsBetweenEvents = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent", meta = (ClampMin = "0"))
	int32 MaxStrongEventsPerMission = 1;

	/** Общий множитель шансов (для отладки/настройки баланса). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RareEvent", meta = (ClampMin = "0"))
	float ChanceMultiplier = 1.f;

	/** Вызывается на каждом клиенте, которому событие доставлено (для UI/звука без якоря). */
	UPROPERTY(BlueprintAssignable, Category = "RareEvent")
	FOnDarcRareEventReceived OnRareEventReceived;

	/** Сервер: начало выезда. Вызывает TaskManager при старте задания. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RareEvent")
	void BeginMission(int32 LevelIndex, bool bAllowStrongEvents, int32 Seed);

	/** Сервер: конец выезда — даём шанс событиям с триггером MissionEnd и останавливаем проверки. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RareEvent")
	void EndMission();

	/**
	 * Сервер: принудительно попытаться запустить событие из сценария/Blueprint.
	 * bIgnoreChance — пропустить бросок шанса (условия, лимиты и якоря всё равно проверяются).
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RareEvent")
	bool TryTriggerEvent(FName EventId, bool bIgnoreChance = true);

	/** Клиент: доставка события (вызывается из RPC). Не вызывать вручную. */
	void DeliverLocally(const FDarcRareEventPayload& Payload);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_RareEvent(const FDarcRareEventPayload& Payload);

	void HandleMemoryEvent(EDarcMemoryEvent Event, FName Id, int32 PlayerId);
	void EvaluatePeriodic();
	void Evaluate(const FDarcRareEventEvalContext& Context);

	bool CanFire(FName EventId, const FDarcRareEventRow& Row, const FDarcRareEventEvalContext& Context) const;
	bool CheckCondition(const FDarcRareEventConditionData& Condition) const;
	bool Fire(FName EventId, const FDarcRareEventRow& Row, const FDarcRareEventEvalContext& Context);

	ADarcPlayerState* PickTargetPlayer(const FDarcRareEventEvalContext& Context);
	ADarcRareEventAnchor* PickAnchor(const FDarcRareEventRow& Row, const FDarcRareEventEvalContext& Context, const ADarcPlayerState* Target);
	FName ApplyEcho(EDarcEchoAction Action);

	TArray<ADarcPlayerState*> GetPlayers(bool bAliveOnly) const;
	UDarcWorldMemorySubsystem* GetMemory() const;
	float Now() const;

	// --- Серверное состояние текущего выезда (клиентам не реплицируется) ---
	int32 CurrentLevel = 0;
	bool bStrongAllowed = true;
	bool bMissionActive = false;
	int32 StrongFiredThisMission = 0;
	float LastEventTime = -1.f;
	TMap<FName, float> LastFireTimeById;
	FRandomStream Random;

	FTimerHandle EvaluationTimer;
	FDelegateHandle MemoryHandle;
};
