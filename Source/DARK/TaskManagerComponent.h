// TaskManagerComponent.h
// Task / Objective System. Компонент на ADarcGameState: состояние задач общее для
// всей команды и реплицируется всем (в т.ч. игроку, подключившемуся посреди выезда).
// Менять задачи может только сервер. Игровые объекты (терминал, щит, генератор)
// на сервере вызывают CompleteTask / FailTask / AddEmergentTask.
//
// Главный принцип: мир важнее списка. Если игроки физически сделали что-то раньше,
// чем задача стала активной (например, подняли питание до того, как нашли сервер),
// CompleteTask всё равно засчитает это — не заставляем повторять.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarcTaskTypes.h"
#include "TaskManagerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDarcTasksChanged);

UCLASS(ClassGroup = (DARC), meta = (BlueprintSpawnableComponent))
class DARK_API UTaskManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTaskManagerComponent();

	UFUNCTION(BlueprintPure, Category = "Tasks", meta = (WorldContext = "WorldContextObject"))
	static UTaskManagerComponent* GetTaskManager(const UObject* WorldContextObject);

	/** Срабатывает на сервере и на каждом клиенте при любом изменении задач — для HUD. */
	UPROPERTY(BlueprintAssignable, Category = "Tasks")
	FOnDarcTasksChanged OnTasksChanged;

	// ---------- Сервер ----------

	/**
	 * Начать выезд: загрузить задачи, перевести фазу в InProgress, запустить
	 * память мира и редкие события. Seed = 0 — выбрать случайный.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Tasks")
	void StartMission(UDarcMissionDefinition* Mission, int32 Seed = 0);

	/** Отметить задачу выполненной. ByActor — кто сделал (для памяти мира), может быть null. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Tasks")
	bool CompleteTask(FName TaskId, AActor* ByActor = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Tasks")
	bool FailTask(FName TaskId);

	/**
	 * Добавить задачу, возникшую по ходу выезда (ошибка → новая проблема).
	 * BlocksTaskId — если задана, эта задача становится предпосылкой для неё
	 * (пример: «Восстановить питание» блокирует «Скопировать архив», пока питание не вернут).
	 * Повторное добавление задачи с тем же ID, пока она не закрыта, игнорируется;
	 * закрытую — открывает заново (проблема повторилась).
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Tasks")
	bool AddEmergentTask(const FDarcTaskDefinition& Task, FName BlocksTaskId);

	/** Провал выезда целиком (например, все погибли). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Tasks")
	void FailMission();

	// ---------- Чтение (везде) ----------

	UFUNCTION(BlueprintPure, Category = "Tasks")
	TArray<FDarcTaskState> GetTasks() const { return Tasks; }

	/** Задачи для показа в HUD: без скрытых, пока они не стали активными. */
	UFUNCTION(BlueprintPure, Category = "Tasks")
	TArray<FDarcTaskState> GetVisibleTasks() const;

	UFUNCTION(BlueprintPure, Category = "Tasks")
	EDarcTaskStatus GetTaskStatus(FName TaskId) const;

	UFUNCTION(BlueprintPure, Category = "Tasks")
	bool IsTaskActive(FName TaskId) const { return GetTaskStatus(TaskId) == EDarcTaskStatus::Active; }

	UFUNCTION(BlueprintPure, Category = "Tasks")
	UDarcMissionDefinition* GetCurrentMission() const { return CurrentMission; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Tasks();

	FDarcTaskState* FindTask(FName TaskId);
	const FDarcTaskState* FindTask(FName TaskId) const;

	/** Пересчитать статусы до устойчивого состояния (разблокировки, родители, пути, завершение выезда). */
	void Refresh();
	void MarkSubtreeObsolete(FName ParentId);
	void SetCompleted(FDarcTaskState& Task, AActor* ByActor);
	void FinishMission(bool bSucceeded);
	void FlushMemoryRecords();
	void NotifyChanged();

	UPROPERTY(ReplicatedUsing = OnRep_Tasks)
	TArray<FDarcTaskState> Tasks;

	UPROPERTY(Replicated)
	TObjectPtr<UDarcMissionDefinition> CurrentMission;

	bool bMissionRunning = false;

	/** Выполненные задачи, ещё не записанные в WorldMemory (см. SetCompleted). */
	TArray<TPair<FName, TWeakObjectPtr<AActor>>> PendingMemoryRecords;
};
