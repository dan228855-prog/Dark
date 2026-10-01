// DarcRareEventTypes.h
// Данные редких событий (data-driven). Строки таблицы заводятся в редакторе:
// DataTable на структуре FDarcRareEventRow, имя строки = ID события ("FifthPlayer", "RoomRemembers"...).
// Тексты, звуки и визуал события в C++ не задаются — их играет Blueprint якоря (ADarcRareEventAnchor).
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DarcRareEventTypes.generated.h"

class ADarcRareEventAnchor;
class APlayerState;

/** Когда сервер проверяет событие. */
UENUM(BlueprintType)
enum class EDarcRareEventTrigger : uint8
{
	Periodic      UMETA(DisplayName = "Периодически"),
	RoomEntered   UMETA(DisplayName = "Игрок вошёл в комнату"),
	RoomExited    UMETA(DisplayName = "Игрок вышел из комнаты"),
	TaskCompleted UMETA(DisplayName = "Выполнена задача"),
	MissionStart  UMETA(DisplayName = "Начало выезда"),
	MissionEnd    UMETA(DisplayName = "Конец выезда"),
	Manual        UMETA(DisplayName = "Только вручную (Blueprint/сценарий)")
};

/** «Сила страха». Strong — не более одного за уровень, и не на всех уровнях. */
UENUM(BlueprintType)
enum class EDarcFearLevel : uint8
{
	Subtle     UMETA(DisplayName = "Едва заметное"),
	Noticeable UMETA(DisplayName = "Заметное"),
	Strong     UMETA(DisplayName = "Сильное")
};

UENUM(BlueprintType)
enum class EDarcRareEventRepeat : uint8
{
	OncePerCampaign UMETA(DisplayName = "Один раз за кампанию"),
	OncePerMission  UMETA(DisplayName = "Один раз за выезд"),
	Repeatable      UMETA(DisplayName = "Может повторяться (с кулдауном)")
};

/** Кто воспринимает событие. Разная перцепция игроков — по мастер-документу, раздел 20. */
UENUM(BlueprintType)
enum class EDarcRareEventAudience : uint8
{
	AllPlayers    UMETA(DisplayName = "Все игроки"),
	SinglePlayer  UMETA(DisplayName = "Только выбранный игрок"),
	AllExceptOne  UMETA(DisplayName = "Все, кроме выбранного")
};

/** Что событие меняет в мире на сервере (то, что увидят все и что останется). */
UENUM(BlueprintType)
enum class EDarcEchoAction : uint8
{
	None             UMETA(DisplayName = "Ничего (только показ)"),
	CloseDoorLeftOpen UMETA(DisplayName = "Закрыть дверь, оставленную открытой"),
	NudgeMovedItem   UMETA(DisplayName = "Слегка сдвинуть ранее перемещённый предмет")
};

UENUM(BlueprintType)
enum class EDarcRareEventCondition : uint8
{
	MinPlayers       UMETA(DisplayName = "Игроков не меньше Number"),
	MaxPlayers       UMETA(DisplayName = "Игроков не больше Number"),
	PlayersSplit     UMETA(DisplayName = "Команда разделена по комнатам"),
	RoomVisited      UMETA(DisplayName = "Комната Name посещена"),
	RoomNotVisited   UMETA(DisplayName = "Комната Name не посещена"),
	AnyItemMoved     UMETA(DisplayName = "Хоть один предмет перемещён"),
	AnyDoorLeftOpen  UMETA(DisplayName = "Хоть одна дверь оставлена открытой"),
	ObjectFound      UMETA(DisplayName = "Найден объект Name"),
	TaskCompleted    UMETA(DisplayName = "Выполнена задача Name"),
	CampaignFact     UMETA(DisplayName = "Есть факт кампании Name"),
	NoCampaignFact   UMETA(DisplayName = "Нет факта кампании Name"),
	EventFiredBefore UMETA(DisplayName = "Событие Name уже случалось в кампании"),
	MinMissionTime   UMETA(DisplayName = "С начала выезда прошло не меньше Number сек"),
	MinTotalDeaths   UMETA(DisplayName = "Смертей у команды не меньше Number")
};

USTRUCT(BlueprintType)
struct FDarcRareEventConditionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	EDarcRareEventCondition Type = EDarcRareEventCondition::MinPlayers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	float Number = 0.f;
};

/** Одна строка таблицы редких событий. */
USTRUCT(BlueprintType)
struct FDarcRareEventRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Тип события — по нему подбирается якорь в уровне ("Terminal", "Photo", "Speaker", "PhantomDoor"...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	FName EventType;

	/** Диапазон уровней первой арки (1–7), на которых событие допустимо. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent", meta = (ClampMin = "0"))
	int32 MinLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent", meta = (ClampMin = "0"))
	int32 MaxLevel = 7;

	/** Шанс срабатывания при одной проверке, 0..1. Для ультраредких — 0.01 и меньше. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent", meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	EDarcRareEventTrigger Trigger = EDarcRareEventTrigger::Periodic;

	/** Допустимая дистанция от выбранного игрока до якоря события, см. 0 = без ограничения. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent", meta = (ClampMin = "0"))
	float MinDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent", meta = (ClampMin = "0"))
	float MaxDistance = 0.f;

	/** Для триггеров по комнате: якорь должен стоять в той самой комнате («шаги из только что покинутой комнаты»). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	bool bAnchorInTriggerRoom = false;

	/** Нужен ли якорь в уровне. Без якоря событие доставляется только через делегат/UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	bool bRequiresAnchor = true;

	/** Все условия должны выполняться (И). Могут ссылаться на WorldMemory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	TArray<FDarcRareEventConditionData> Conditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	EDarcFearLevel FearLevel = EDarcFearLevel::Subtle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	EDarcRareEventRepeat Repeat = EDarcRareEventRepeat::OncePerCampaign;

	/** Кулдаун для повторяемых событий, сек. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent", meta = (ClampMin = "0"))
	float Cooldown = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	EDarcRareEventAudience Audience = EDarcRareEventAudience::AllPlayers;

	/** Серверное изменение мира — «искажённое эхо» на основе WorldMemory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	EDarcEchoAction EchoAction = EDarcEchoAction::None;

	/** Перенос в кампанию: если задан, после события в постоянное состояние добавляется этот факт. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	FName CampaignFactOnFire;
};

/**
 * То, что сервер отправляет клиентам. Только результат и параметры показа —
 * без условий, шансов и данных памяти, по которым принималось решение.
 */
USTRUCT(BlueprintType)
struct FDarcRareEventPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RareEvent")
	FName EventId;

	UPROPERTY(BlueprintReadOnly, Category = "RareEvent")
	FName EventType;

	UPROPERTY(BlueprintReadOnly, Category = "RareEvent")
	EDarcFearLevel FearLevel = EDarcFearLevel::Subtle;

	/** Где играть событие (может быть null для событий без якоря). */
	UPROPERTY(BlueprintReadOnly, Category = "RareEvent")
	TObjectPtr<ADarcRareEventAnchor> Anchor = nullptr;

	/** Игрок, вокруг которого событие построено. */
	UPROPERTY(BlueprintReadOnly, Category = "RareEvent")
	TObjectPtr<APlayerState> TargetPlayer = nullptr;

	/** Число игроков в сессии на момент события («ФОТОГРАФИЯ КОМАНДЫ», «ПЯТЫЙ ИГРОК»). */
	UPROPERTY(BlueprintReadOnly, Category = "RareEvent")
	int32 PlayerCount = 0;

	/** Комната/объект из памяти, к которым относится событие. */
	UPROPERTY(BlueprintReadOnly, Category = "RareEvent")
	FName ContextId;

	/** Сид, чтобы у всех клиентов случайные детали показа совпали. */
	UPROPERTY(BlueprintReadOnly, Category = "RareEvent")
	int32 Seed = 0;
};
