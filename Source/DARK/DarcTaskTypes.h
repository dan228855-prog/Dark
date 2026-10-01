// DarcTaskTypes.h
// Данные Task / Objective System. Задание выезда — дерево задач:
// - у задачи может быть родитель; родитель выполняется по правилу (все дети / любой ребёнок / вручную);
// - «любой ребёнок» = несколько путей решения (щит ИЛИ генератор, ключ-карта ИЛИ код);
// - предпосылки (Prerequisites) задают порядок цепочки (сначала доступ, потом копирование);
// - во время выезда сервер может добавить «внезапную» задачу (AddEmergentTask) —
//   например, сгоревший предохранитель обесточил коридор.
// Тексты задач — FText из String Table, выбираются в редакторе в ассете выезда.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "DarcTaskTypes.generated.h"

UENUM(BlueprintType)
enum class EDarcTaskStatus : uint8
{
	Locked    UMETA(DisplayName = "Ещё недоступна"),
	Active    UMETA(DisplayName = "Активна"),
	Completed UMETA(DisplayName = "Выполнена"),
	Failed    UMETA(DisplayName = "Провалена"),
	Obsolete  UMETA(DisplayName = "Не понадобилась (выбран другой путь)")
};

UENUM(BlueprintType)
enum class EDarcTaskRule : uint8
{
	Manual      UMETA(DisplayName = "Выполняется вызовом CompleteTask"),
	AllChildren UMETA(DisplayName = "Когда выполнены все обязательные подзадачи"),
	AnyChild    UMETA(DisplayName = "Когда выполнена любая подзадача (несколько путей)")
};

/** Описание задачи. Может жить в ассете выезда или строкой DataTable (импорт из CSV). */
USTRUCT(BlueprintType)
struct FDarcTaskDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FName TaskId;

	/** Текст для игрока — строка из String Table. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FText Title;

	/** Родительская задача (None — задача верхнего уровня). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FName ParentTaskId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	EDarcTaskRule Rule = EDarcTaskRule::Manual;

	/** Задачи, которые должны быть выполнены, прежде чем эта станет активной. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	TArray<FName> Prerequisites;

	/**
	 * Обязательна ли. Для задач верхнего уровня — нужна ли для завершения выезда;
	 * для подзадач AllChildren-родителя — блокирует ли родителя.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bRequired = true;

	/** Не показывать в списке, пока не станет активной (сюжетные/скрытые задачи). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bHiddenUntilActive = false;

	/** Провал этой задачи проваливает весь выезд. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bFailsMission = false;

	/** Факт кампании при выполнении (например "Slice.ExtraFileTaken") — последствия на будущие выезды. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FName CampaignFactOnComplete;
};

/** Состояние задачи в рантайме — реплицируется всем клиентам. */
USTRUCT(BlueprintType)
struct FDarcTaskState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Task")
	FDarcTaskDefinition Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Task")
	EDarcTaskStatus Status = EDarcTaskStatus::Locked;

	/** Появилась во время выезда, а не была в исходном задании. */
	UPROPERTY(BlueprintReadOnly, Category = "Task")
	bool bEmergent = false;

	FName GetId() const { return Definition.TaskId; }
	bool IsClosed() const { return Status == EDarcTaskStatus::Completed || Status == EDarcTaskStatus::Failed || Status == EDarcTaskStatus::Obsolete; }
};

/** Ассет одного выезда: создаётся в редакторе (Data Asset → DarcMissionDefinition). */
UCLASS(BlueprintType)
class DARK_API UDarcMissionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Название задания для игрока — из String Table. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FText Title;

	/** Номер уровня первой арки (1–7) — для редких событий и эскалации. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = "0"))
	int32 LevelIndex = 1;

	/** Разрешено ли на этом выезде сильное редкое событие (некоторые уровни — без них). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bAllowStrongRareEvents = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FDarcTaskDefinition> Tasks;

	/**
	 * Задачи из таблицы (DataTable со строками DarcTaskDefinition, можно импортировать из CSV).
	 * Добавляются к списку Tasks. Если TaskId в строке пуст — берётся имя строки.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TObjectPtr<UDataTable> TaskTable;

	/** Факты кампании при успешном выезде (например "Base.ArchiveTerminal" — открыть улучшение базы). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FName> CampaignFactsOnSuccess;

	/** Все задачи выезда: список + таблица. */
	TArray<FDarcTaskDefinition> GatherTasks() const
	{
		TArray<FDarcTaskDefinition> Result = Tasks;
		if (TaskTable)
		{
			TaskTable->ForeachRow<FDarcTaskDefinition>(TEXT("DarcMissionDefinition"),
				[&Result](const FName& Key, const FDarcTaskDefinition& Row)
				{
					FDarcTaskDefinition& Added = Result.Add_GetRef(Row);
					if (Added.TaskId.IsNone())
					{
						Added.TaskId = Key;
					}
				});
		}
		return Result;
	}
};
