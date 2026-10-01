// TaskManagerComponent.cpp
#include "TaskManagerComponent.h"
#include "DarcGameState.h"
#include "DarcWorldMemorySubsystem.h"
#include "RareEventManagerComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UTaskManagerComponent::UTaskManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UTaskManagerComponent* UTaskManagerComponent::GetTaskManager(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UTaskManagerComponent>() : nullptr;
}

void UTaskManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTaskManagerComponent, Tasks);
	DOREPLIFETIME(UTaskManagerComponent, CurrentMission);
	DOREPLIFETIME(UTaskManagerComponent, MissionSeed);
}

void UTaskManagerComponent::OnRep_Tasks()
{
	OnTasksChanged.Broadcast();
}

void UTaskManagerComponent::NotifyChanged()
{
	// На сервере OnRep не вызывается сам — оповещаем HUD хоста вручную.
	OnTasksChanged.Broadcast();
}

FDarcTaskState* UTaskManagerComponent::FindTask(FName TaskId)
{
	return Tasks.FindByPredicate([TaskId](const FDarcTaskState& T) { return T.GetId() == TaskId; });
}

const FDarcTaskState* UTaskManagerComponent::FindTask(FName TaskId) const
{
	return Tasks.FindByPredicate([TaskId](const FDarcTaskState& T) { return T.GetId() == TaskId; });
}

EDarcTaskStatus UTaskManagerComponent::GetTaskStatus(FName TaskId) const
{
	const FDarcTaskState* Task = FindTask(TaskId);
	return Task ? Task->Status : EDarcTaskStatus::Locked;
}

TArray<FDarcTaskState> UTaskManagerComponent::GetVisibleTasks() const
{
	return Tasks.FilterByPredicate([](const FDarcTaskState& T)
	{
		return !(T.Definition.bHiddenUntilActive && T.Status == EDarcTaskStatus::Locked)
			&& T.Status != EDarcTaskStatus::Obsolete;
	});
}

// ---------------------------------------------------------------------------
// Сервер
// ---------------------------------------------------------------------------

void UTaskManagerComponent::StartMission(UDarcMissionDefinition* Mission, int32 Seed)
{
	if (!GetOwner()->HasAuthority() || !Mission)
	{
		return;
	}

	CurrentMission = Mission;
	Tasks.Reset();
	for (const FDarcTaskDefinition& Def : Mission->Tasks)
	{
		if (Def.TaskId.IsNone() || FindTask(Def.TaskId))
		{
			UE_LOG(LogTemp, Warning, TEXT("TaskManager: empty or duplicate TaskId in %s"), *Mission->GetName());
			continue;
		}
		FDarcTaskState& State = Tasks.AddDefaulted_GetRef();
		State.Definition = Def;
	}

	MissionSeed = Seed != 0 ? Seed : FMath::RandRange(1, MAX_int32 - 1);

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->BeginMission(Mission->LevelIndex, MissionSeed);
	}

	bMissionRunning = true;
	if (ADarcGameState* GameState = Cast<ADarcGameState>(GetOwner()))
	{
		GameState->SetMissionPhase(EMissionPhase::InProgress);
	}

	Refresh();
	NotifyChanged();
	FlushMemoryRecords();

	// Редкие события — последними: им уже нужны задачи и фаза выезда.
	if (URareEventManagerComponent* RareEvents = URareEventManagerComponent::GetRareEventManager(this))
	{
		RareEvents->BeginMission(Mission->LevelIndex, Mission->bAllowStrongRareEvents, MissionSeed);
	}
}

bool UTaskManagerComponent::CompleteTask(FName TaskId, AActor* ByActor)
{
	if (!GetOwner()->HasAuthority() || !bMissionRunning)
	{
		return false;
	}

	FDarcTaskState* Task = FindTask(TaskId);
	if (!Task || Task->IsClosed())
	{
		return false;
	}

	// Засчитываем и из Locked: игроки могли сделать это раньше, чем «узнали» о задаче.
	SetCompleted(*Task, ByActor);
	Refresh();
	NotifyChanged();
	FlushMemoryRecords();
	return true;
}

void UTaskManagerComponent::SetCompleted(FDarcTaskState& Task, AActor* ByActor)
{
	Task.Status = EDarcTaskStatus::Completed;

	// В память пишем не сразу, а после пересчёта (FlushMemoryRecords): запись будит
	// RareEventManager, а Blueprint события может добавить задачу — нельзя менять
	// массив Tasks, пока Refresh по нему идёт.
	PendingMemoryRecords.Emplace(Task.GetId(), ByActor);
}

void UTaskManagerComponent::FlushMemoryRecords()
{
	if (PendingMemoryRecords.Num() == 0)
	{
		return;
	}

	const TArray<TPair<FName, TWeakObjectPtr<AActor>>> Records = MoveTemp(PendingMemoryRecords);
	PendingMemoryRecords.Reset();

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		for (const TPair<FName, TWeakObjectPtr<AActor>>& Record : Records)
		{
			Memory->RecordTaskCompleted(Record.Key);
			if (AActor* ByActor = Record.Value.Get())
			{
				Memory->RecordInteraction(Record.Key, TEXT("TaskCompleted"), ByActor);
			}
		}
	}
}

bool UTaskManagerComponent::FailTask(FName TaskId)
{
	if (!GetOwner()->HasAuthority() || !bMissionRunning)
	{
		return false;
	}

	FDarcTaskState* Task = FindTask(TaskId);
	if (!Task || Task->IsClosed())
	{
		return false;
	}

	Task->Status = EDarcTaskStatus::Failed;
	Refresh();
	NotifyChanged();
	FlushMemoryRecords();
	return true;
}

bool UTaskManagerComponent::AddEmergentTask(const FDarcTaskDefinition& Task, FName BlocksTaskId)
{
	if (!GetOwner()->HasAuthority() || !bMissionRunning || Task.TaskId.IsNone())
	{
		return false;
	}

	if (FDarcTaskState* Existing = FindTask(Task.TaskId))
	{
		if (!Existing->IsClosed())
		{
			return false; // проблема уже висит
		}
		// Проблема повторилась (например, предохранитель сгорел второй раз) — открываем заново.
		Existing->Definition = Task;
		Existing->Status = EDarcTaskStatus::Locked;
		Existing->bEmergent = true;
	}
	else
	{
		FDarcTaskState& State = Tasks.AddDefaulted_GetRef();
		State.Definition = Task;
		State.bEmergent = true;
	}

	if (!BlocksTaskId.IsNone())
	{
		if (FDarcTaskState* Blocked = FindTask(BlocksTaskId); Blocked && !Blocked->IsClosed())
		{
			Blocked->Definition.Prerequisites.AddUnique(Task.TaskId);
			Blocked->Status = EDarcTaskStatus::Locked; // Refresh вернёт Active, когда проблему решат
		}
	}

	Refresh();
	NotifyChanged();
	FlushMemoryRecords();
	return true;
}

void UTaskManagerComponent::FailMission()
{
	if (GetOwner()->HasAuthority() && bMissionRunning)
	{
		FinishMission(false);
		NotifyChanged();
	}
}

// ---------------------------------------------------------------------------
// Пересчёт статусов
// ---------------------------------------------------------------------------

void UTaskManagerComponent::MarkSubtreeObsolete(FName ParentId)
{
	for (FDarcTaskState& Task : Tasks)
	{
		if (Task.Definition.ParentTaskId == ParentId && !Task.IsClosed())
		{
			Task.Status = EDarcTaskStatus::Obsolete;
			MarkSubtreeObsolete(Task.GetId());
		}
	}
}

void UTaskManagerComponent::Refresh()
{
	// Повторяем, пока что-то меняется: выполнение подзадачи может закрыть родителя,
	// а тот — разблокировать следующую задачу цепочки. Ограничение — защита от
	// ошибок в данных (циклические предпосылки).
	for (int32 Pass = 0; Pass < 32 && bMissionRunning; ++Pass)
	{
		bool bChanged = false;

		for (FDarcTaskState& Task : Tasks)
		{
			if (Task.IsClosed())
			{
				continue;
			}

			// --- Родитель по правилу подзадач ---
			if (Task.Definition.Rule != EDarcTaskRule::Manual)
			{
				int32 Children = 0, RequiredChildren = 0, RequiredDone = 0, AnyDone = 0, Failed = 0;
				for (const FDarcTaskState& Child : Tasks)
				{
					if (Child.Definition.ParentTaskId != Task.GetId())
					{
						continue;
					}
					Children++;
					AnyDone += Child.Status == EDarcTaskStatus::Completed;
					Failed += Child.Status == EDarcTaskStatus::Failed;
					if (Child.Definition.bRequired)
					{
						RequiredChildren++;
						RequiredDone += Child.Status == EDarcTaskStatus::Completed;
					}
				}

				const bool bDone = Children > 0 && (Task.Definition.Rule == EDarcTaskRule::AnyChild
					? AnyDone > 0
					: RequiredDone == RequiredChildren && (RequiredChildren > 0 || AnyDone > 0));

				if (bDone)
				{
					SetCompleted(Task, nullptr);
					if (Task.Definition.Rule == EDarcTaskRule::AnyChild)
					{
						MarkSubtreeObsolete(Task.GetId()); // остальные пути больше не нужны
					}
					bChanged = true;
					continue;
				}

				// Все пути провалены — провалена и сама задача.
				if (Task.Definition.Rule == EDarcTaskRule::AnyChild && Children > 0 && Failed == Children)
				{
					Task.Status = EDarcTaskStatus::Failed;
					bChanged = true;
					continue;
				}
			}

			// --- Разблокировка ---
			if (Task.Status == EDarcTaskStatus::Locked)
			{
				bool bReady = true;
				for (const FName& PrereqId : Task.Definition.Prerequisites)
				{
					const FDarcTaskState* Prereq = FindTask(PrereqId);
					if (!Prereq || Prereq->Status != EDarcTaskStatus::Completed)
					{
						bReady = false;
						break;
					}
				}

				// Подзадача открывается только вместе с родителем.
				if (bReady && !Task.Definition.ParentTaskId.IsNone())
				{
					const FDarcTaskState* Parent = FindTask(Task.Definition.ParentTaskId);
					bReady = Parent && Parent->Status == EDarcTaskStatus::Active;
				}

				if (bReady)
				{
					Task.Status = EDarcTaskStatus::Active;
					bChanged = true;
				}
			}
		}

		if (!bChanged)
		{
			break;
		}
	}

	// --- Итог выезда ---
	bool bAllRequiredDone = true;
	bool bAnyTopLevel = false;
	for (const FDarcTaskState& Task : Tasks)
	{
		if (Task.Status == EDarcTaskStatus::Failed && Task.Definition.bFailsMission)
		{
			FinishMission(false);
			return;
		}
		if (Task.Definition.ParentTaskId.IsNone() && Task.Definition.bRequired)
		{
			bAnyTopLevel = true;
			bAllRequiredDone &= Task.Status == EDarcTaskStatus::Completed;
		}
	}

	if (bAnyTopLevel && bAllRequiredDone)
	{
		FinishMission(true);
	}
}

void UTaskManagerComponent::FinishMission(bool bSucceeded)
{
	if (!bMissionRunning)
	{
		return;
	}
	bMissionRunning = false;

	if (URareEventManagerComponent* RareEvents = URareEventManagerComponent::GetRareEventManager(this))
	{
		RareEvents->EndMission();
	}
	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->EndMission(bSucceeded);
	}
	if (ADarcGameState* GameState = Cast<ADarcGameState>(GetOwner()))
	{
		GameState->SetMissionPhase(bSucceeded ? EMissionPhase::Completed : EMissionPhase::Failed);
	}
}
