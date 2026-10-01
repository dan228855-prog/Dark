// DarcWorldMemorySubsystem.h
// WorldMemory — серверная «память мира»: что игроки делали в текущем выезде
// (двери, перемещённые предметы, посещённые комнаты, разделение команды,
// найденные объекты, взаимодействия, порядок задач, редкие события).
// Сама генерация «искажённого эха» — в URareEventManagerComponent, здесь только данные.
//
// ПОЧЕМУ UGameInstanceSubsystem, А НЕ GameState:
// - GameState уничтожается при каждой смене карты (база → выезд → база). А эхо и
//   последствия должны проявляться в том числе после возвращения на базу
//   («после возвращения на базе изменился предмет»), то есть память обязана
//   пережить переход между картами. GameInstance живёт всю сессию запуска игры.
// - На Listen Server у каждой машины свой GameInstance. Заполняется и читается
//   только серверный экземпляр (хост). На клиентах подсистема существует, но
//   пустая — клиенты получают только результаты событий, а не саму память.
//   Все методы записи молча игнорируют вызовы не на сервере.
// - «В рамках текущей сессии» = в рамках выезда. Поэтому память разделена на два слоя:
//   * MissionMemory — подробности текущего выезда. Очищается в BeginMission().
//   * Campaign-слой — то немногое, что переносится дальше: факты кампании
//     (AddCampaignFact), какие редкие события уже случались, краткая сводка
//     последнего выезда. Его позже будет сохранять Save/Progression System (P1).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DarcWorldMemorySubsystem.generated.h"

/** Что произошло в памяти — для подписчиков (RareEventManager). */
UENUM(BlueprintType)
enum class EDarcMemoryEvent : uint8
{
	DoorChanged,
	ItemMoved,
	RoomEntered,
	RoomExited,
	ObjectFound,
	Interaction,
	TaskCompleted,
	RareEventFired
};

USTRUCT(BlueprintType)
struct FDarcDoorMemory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	FName DoorId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	bool bIsOpen = false;

	/** PlayerId того, кто последним менял состояние (INDEX_NONE — не игрок, например событие). */
	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	int32 LastPlayerId = INDEX_NONE;

	/** Время мира (сек с начала выезда) последнего изменения. */
	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	float LastChangeTime = 0.f;
};

USTRUCT(BlueprintType)
struct FDarcItemMemory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	FName ItemId;

	/** Где предмет был до первого перемещения в этом выезде. */
	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	FTransform OriginalTransform;

	/** Где его оставили последний раз. */
	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	FTransform LastTransform;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	int32 LastPlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	int32 MoveCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	float LastMoveTime = 0.f;
};

USTRUCT(BlueprintType)
struct FDarcRoomMemory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	FName RoomId;

	/** Кто из игроков хоть раз здесь был. */
	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	TArray<int32> VisitedBy;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	int32 VisitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	float FirstVisitTime = -1.f;

	/** Когда отсюда последний раз кто-то вышел (-1 — ещё не выходили). Нужно для «ОН УЖЕ ВЫШЕЛ». */
	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	float LastExitTime = -1.f;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	int32 LastExitPlayerId = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FDarcInteractionRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	FName TargetId;

	/** Произвольная метка действия: "Open", "PickUp", "EnterCode"... */
	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	FName Tag;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	float Time = 0.f;
};

/** Краткая сводка выезда — переживает выезд и доступна на базе. */
USTRUCT(BlueprintType)
struct FDarcMissionSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	int32 LevelIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	float Duration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	TArray<FName> CompletedTasks;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	TArray<FName> FoundObjects;

	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	TArray<FName> RareEvents;

	/** Расходилась ли команда по разным комнатам хоть раз. */
	UPROPERTY(BlueprintReadOnly, Category = "WorldMemory")
	bool bTeamWasSplit = false;
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnDarcMemoryEventNative, EDarcMemoryEvent /*Event*/, FName /*Id*/, int32 /*PlayerId*/);

UCLASS()
class DARK_API UDarcWorldMemorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Удобный доступ из C++ и Blueprint. Может вернуть nullptr. */
	UFUNCTION(BlueprintPure, Category = "WorldMemory", meta = (WorldContext = "WorldContextObject"))
	static UDarcWorldMemorySubsystem* GetWorldMemory(const UObject* WorldContextObject);

	/** Нативная подписка (RareEventManager). Вызывается только на сервере. */
	FOnDarcMemoryEventNative OnMemoryEvent;

	// ---------- Жизненный цикл выезда ----------

	/** Начало выезда: очищает память выезда. Campaign-слой не трогает. */
	UFUNCTION(BlueprintCallable, Category = "WorldMemory")
	void BeginMission(int32 LevelIndex, int32 Seed);

	/** Конец выезда: собирает сводку в Campaign-слой. Подробности выезда остаются до следующего BeginMission. */
	UFUNCTION(BlueprintCallable, Category = "WorldMemory")
	void EndMission(bool bSucceeded);

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	int32 GetCurrentLevelIndex() const { return CurrentLevelIndex; }

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	int32 GetMissionSeed() const { return MissionSeed; }

	/** Секунды с начала выезда. */
	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	float GetMissionTime() const;

	// ---------- Запись (только сервер) ----------

	UFUNCTION(BlueprintCallable, Category = "WorldMemory")
	void RecordDoorState(FName DoorId, bool bIsOpen, AActor* ByActor);

	/** Предмет перенесли из From в To. Первое перемещение запоминает исходную позицию. */
	UFUNCTION(BlueprintCallable, Category = "WorldMemory")
	void RecordItemMoved(FName ItemId, const FTransform& From, const FTransform& To, AActor* ByActor);

	UFUNCTION(BlueprintCallable, Category = "WorldMemory")
	void RecordPlayerEnteredRoom(FName RoomId, AActor* PlayerActor);

	UFUNCTION(BlueprintCallable, Category = "WorldMemory")
	void RecordPlayerExitedRoom(FName RoomId, AActor* PlayerActor);

	UFUNCTION(BlueprintCallable, Category = "WorldMemory")
	void RecordObjectFound(FName ObjectId, AActor* ByActor);

	UFUNCTION(BlueprintCallable, Category = "WorldMemory")
	void RecordInteraction(FName TargetId, FName Tag, AActor* ByActor);

	UFUNCTION(BlueprintCallable, Category = "WorldMemory")
	void RecordTaskCompleted(FName TaskId);

	UFUNCTION(BlueprintCallable, Category = "WorldMemory")
	void RecordRareEvent(FName EventId);

	/** Смерть игрока. Счётчик за кампанию живёт здесь, а не в PlayerState, — тот сбрасывается при смене карты. */
	UFUNCTION(BlueprintCallable, Category = "WorldMemory")
	void RecordPlayerDeath(AActor* PlayerActor);

	// ---------- Чтение ----------

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	bool GetDoorMemory(FName DoorId, FDarcDoorMemory& OutMemory) const;

	/** Двери, которые игроки открыли и так и оставили открытыми. */
	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	TArray<FName> GetDoorsLeftOpen() const;

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	bool GetItemMemory(FName ItemId, FDarcItemMemory& OutMemory) const;

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	TArray<FName> GetMovedItems() const;

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	bool WasRoomVisited(FName RoomId) const;

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	bool GetRoomMemory(FName RoomId, FDarcRoomMemory& OutMemory) const;

	/** Комнаты, из которых кто-то вышел не позднее WithinSeconds назад. */
	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	TArray<FName> GetRecentlyExitedRooms(float WithinSeconds) const;

	/** Комната, в которой сейчас игрок (None — вне отмеченных комнат). */
	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	FName GetPlayerRoom(int32 PlayerId) const;

	/** Находятся ли игроки сейчас в разных комнатах. */
	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	bool ArePlayersSplit() const;

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	bool WasObjectFound(FName ObjectId) const;

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	int32 GetInteractionCount(FName TargetId) const;

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	TArray<FName> GetCompletedTasks() const { return CompletedTasks; }

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	bool WasTaskCompleted(FName TaskId) const { return CompletedTasks.Contains(TaskId); }

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	bool HasRareEventFiredThisMission(FName EventId) const { return MissionRareEvents.Contains(EventId); }

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	bool HasRareEventEverFired(FName EventId) const { return CampaignRareEvents.Contains(EventId); }

	// ---------- Campaign-слой ----------

	/** Факт, который переживает выезд (например "Mission1.ExtraFileTaken"). */
	UFUNCTION(BlueprintCallable, Category = "WorldMemory|Campaign")
	void AddCampaignFact(FName Fact);

	UFUNCTION(BlueprintPure, Category = "WorldMemory|Campaign")
	bool HasCampaignFact(FName Fact) const { return CampaignFacts.Contains(Fact); }

	/** Смертей команды за всю кампанию — на них завязана эскалация (логи, поведение духа). */
	UFUNCTION(BlueprintPure, Category = "WorldMemory|Campaign")
	int32 GetCampaignDeaths() const { return CampaignDeaths; }

	UFUNCTION(BlueprintPure, Category = "WorldMemory")
	int32 GetMissionDeaths() const { return MissionDeaths; }

	UFUNCTION(BlueprintPure, Category = "WorldMemory|Campaign")
	FDarcMissionSummary GetLastMissionSummary() const { return LastMissionSummary; }

	/** Полный сброс кампании (новая игра). */
	UFUNCTION(BlueprintCallable, Category = "WorldMemory|Campaign")
	void ResetCampaign();

	/** PlayerId по пешке/контроллеру/PlayerState. INDEX_NONE, если это не игрок. */
	static int32 GetPlayerIdFromActor(const AActor* Actor);

protected:
	/** Пишем только на сервере (хосте). Клиентские экземпляры подсистемы остаются пустыми. */
	bool IsServer() const;
	float Now() const;
	void Broadcast(EDarcMemoryEvent Event, FName Id, int32 PlayerId);

	// --- Слой выезда ---
	UPROPERTY() int32 CurrentLevelIndex = 0;
	UPROPERTY() int32 MissionSeed = 0;
	UPROPERTY() float MissionStartTime = 0.f;
	UPROPERTY() TMap<FName, FDarcDoorMemory> Doors;
	UPROPERTY() TMap<FName, FDarcItemMemory> Items;
	UPROPERTY() TMap<FName, FDarcRoomMemory> Rooms;
	UPROPERTY() TMap<int32, FName> PlayerRooms;
	UPROPERTY() TArray<FName> FoundObjects;
	UPROPERTY() TArray<FDarcInteractionRecord> Interactions;
	UPROPERTY() TArray<FName> CompletedTasks;
	UPROPERTY() TArray<FName> MissionRareEvents;
	UPROPERTY() bool bTeamWasSplit = false;
	UPROPERTY() int32 MissionDeaths = 0;

	// --- Campaign-слой ---
	UPROPERTY() TSet<FName> CampaignFacts;
	UPROPERTY() TSet<FName> CampaignRareEvents;
	UPROPERTY() FDarcMissionSummary LastMissionSummary;
	UPROPERTY() int32 CampaignDeaths = 0;

	/** Ограничение журнала взаимодействий, чтобы память не росла бесконечно. */
	static constexpr int32 MaxInteractionRecords = 512;
};
