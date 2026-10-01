// DarcWorldMemorySubsystem.cpp
#include "DarcWorldMemorySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"

UDarcWorldMemorySubsystem* UDarcWorldMemorySubsystem::GetWorldMemory(const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UDarcWorldMemorySubsystem>() : nullptr;
}

bool UDarcWorldMemorySubsystem::IsServer() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World && World->GetNetMode() != NM_Client;
}

float UDarcWorldMemorySubsystem::Now() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World ? World->GetTimeSeconds() : 0.f;
}

float UDarcWorldMemorySubsystem::GetMissionTime() const
{
	return FMath::Max(0.f, Now() - MissionStartTime);
}

void UDarcWorldMemorySubsystem::Broadcast(EDarcMemoryEvent Event, FName Id, int32 PlayerId)
{
	OnMemoryEvent.Broadcast(Event, Id, PlayerId);
}

int32 UDarcWorldMemorySubsystem::GetPlayerIdFromActor(const AActor* Actor)
{
	if (const APlayerState* PS = Cast<APlayerState>(Actor))
	{
		return PS->GetPlayerId();
	}
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (const APlayerState* PS = Pawn->GetPlayerState())
		{
			return PS->GetPlayerId();
		}
	}
	if (const AController* Controller = Cast<AController>(Actor))
	{
		if (const APlayerState* PS = Controller->GetPlayerState<APlayerState>())
		{
			return PS->GetPlayerId();
		}
	}
	return INDEX_NONE;
}

// ---------------------------------------------------------------------------
// Жизненный цикл выезда
// ---------------------------------------------------------------------------

void UDarcWorldMemorySubsystem::BeginMission(int32 LevelIndex, int32 Seed)
{
	if (!IsServer())
	{
		return;
	}

	CurrentLevelIndex = LevelIndex;
	MissionSeed = Seed;
	MissionStartTime = Now();

	// Подробности прошлого выезда забываем — в кампанию уже ушла сводка (EndMission).
	Doors.Reset();
	Items.Reset();
	Rooms.Reset();
	PlayerRooms.Reset();
	FoundObjects.Reset();
	Interactions.Reset();
	CompletedTasks.Reset();
	MissionRareEvents.Reset();
	bTeamWasSplit = false;
	MissionDeaths = 0;
}

void UDarcWorldMemorySubsystem::EndMission(bool bSucceeded)
{
	if (!IsServer())
	{
		return;
	}

	// В кампанию переносим только сводку. Точные позиции предметов, журнал
	// взаимодействий и т.п. остаются доступны на базе до начала следующего выезда
	// (чтобы база могла «вспомнить» что-то из только что пройденного), потом стираются.
	LastMissionSummary.LevelIndex = CurrentLevelIndex;
	LastMissionSummary.bSucceeded = bSucceeded;
	LastMissionSummary.Duration = GetMissionTime();
	LastMissionSummary.CompletedTasks = CompletedTasks;
	LastMissionSummary.FoundObjects = FoundObjects;
	LastMissionSummary.RareEvents = MissionRareEvents;
	LastMissionSummary.bTeamWasSplit = bTeamWasSplit;
}

void UDarcWorldMemorySubsystem::ResetCampaign()
{
	if (!IsServer())
	{
		return;
	}

	CampaignFacts.Reset();
	CampaignRareEvents.Reset();
	CampaignDeaths = 0;
	LastMissionSummary = FDarcMissionSummary();
	BeginMission(0, 0);
}

// ---------------------------------------------------------------------------
// Запись
// ---------------------------------------------------------------------------

void UDarcWorldMemorySubsystem::RecordDoorState(FName DoorId, bool bIsOpen, AActor* ByActor)
{
	if (!IsServer() || DoorId.IsNone())
	{
		return;
	}

	FDarcDoorMemory& Door = Doors.FindOrAdd(DoorId);
	Door.DoorId = DoorId;
	Door.bIsOpen = bIsOpen;
	Door.LastPlayerId = GetPlayerIdFromActor(ByActor);
	Door.LastChangeTime = GetMissionTime();

	Broadcast(EDarcMemoryEvent::DoorChanged, DoorId, Door.LastPlayerId);
}

void UDarcWorldMemorySubsystem::RecordItemMoved(FName ItemId, const FTransform& From, const FTransform& To, AActor* ByActor)
{
	if (!IsServer() || ItemId.IsNone())
	{
		return;
	}

	FDarcItemMemory* Existing = Items.Find(ItemId);
	if (!Existing)
	{
		Existing = &Items.Add(ItemId);
		Existing->ItemId = ItemId;
		Existing->OriginalTransform = From; // исходное место запоминаем один раз
	}

	Existing->LastTransform = To;
	Existing->LastPlayerId = GetPlayerIdFromActor(ByActor);
	Existing->MoveCount++;
	Existing->LastMoveTime = GetMissionTime();

	Broadcast(EDarcMemoryEvent::ItemMoved, ItemId, Existing->LastPlayerId);
}

void UDarcWorldMemorySubsystem::RecordPlayerEnteredRoom(FName RoomId, AActor* PlayerActor)
{
	const int32 PlayerId = GetPlayerIdFromActor(PlayerActor);
	if (!IsServer() || RoomId.IsNone() || PlayerId == INDEX_NONE)
	{
		return;
	}

	FDarcRoomMemory& Room = Rooms.FindOrAdd(RoomId);
	Room.RoomId = RoomId;
	Room.VisitedBy.AddUnique(PlayerId);
	Room.VisitCount++;
	if (Room.FirstVisitTime < 0.f)
	{
		Room.FirstVisitTime = GetMissionTime();
	}

	PlayerRooms.Add(PlayerId, RoomId);
	if (ArePlayersSplit())
	{
		bTeamWasSplit = true;
	}

	Broadcast(EDarcMemoryEvent::RoomEntered, RoomId, PlayerId);
}

void UDarcWorldMemorySubsystem::RecordPlayerExitedRoom(FName RoomId, AActor* PlayerActor)
{
	const int32 PlayerId = GetPlayerIdFromActor(PlayerActor);
	if (!IsServer() || RoomId.IsNone() || PlayerId == INDEX_NONE)
	{
		return;
	}

	FDarcRoomMemory& Room = Rooms.FindOrAdd(RoomId);
	Room.RoomId = RoomId;
	Room.LastExitTime = GetMissionTime();
	Room.LastExitPlayerId = PlayerId;

	// Комнаты-объёмы могут перекрываться на стыках: снимаем игрока, только если
	// он числился именно в этой комнате, иначе затрём уже новую.
	if (const FName* Current = PlayerRooms.Find(PlayerId); Current && *Current == RoomId)
	{
		PlayerRooms.Remove(PlayerId);
	}

	Broadcast(EDarcMemoryEvent::RoomExited, RoomId, PlayerId);
}

void UDarcWorldMemorySubsystem::RecordObjectFound(FName ObjectId, AActor* ByActor)
{
	if (!IsServer() || ObjectId.IsNone() || FoundObjects.Contains(ObjectId))
	{
		return;
	}

	FoundObjects.Add(ObjectId);
	Broadcast(EDarcMemoryEvent::ObjectFound, ObjectId, GetPlayerIdFromActor(ByActor));
}

void UDarcWorldMemorySubsystem::RecordInteraction(FName TargetId, FName Tag, AActor* ByActor)
{
	if (!IsServer() || TargetId.IsNone())
	{
		return;
	}

	if (Interactions.Num() >= MaxInteractionRecords)
	{
		Interactions.RemoveAt(0, Interactions.Num() - MaxInteractionRecords + 1);
	}

	FDarcInteractionRecord& Record = Interactions.AddDefaulted_GetRef();
	Record.TargetId = TargetId;
	Record.Tag = Tag;
	Record.PlayerId = GetPlayerIdFromActor(ByActor);
	Record.Time = GetMissionTime();

	Broadcast(EDarcMemoryEvent::Interaction, TargetId, Record.PlayerId);
}

void UDarcWorldMemorySubsystem::RecordTaskCompleted(FName TaskId)
{
	if (!IsServer() || TaskId.IsNone() || CompletedTasks.Contains(TaskId))
	{
		return;
	}

	// Порядок важен: последовательность задач — тоже часть памяти.
	CompletedTasks.Add(TaskId);
	Broadcast(EDarcMemoryEvent::TaskCompleted, TaskId, INDEX_NONE);
}

void UDarcWorldMemorySubsystem::RecordRareEvent(FName EventId)
{
	if (!IsServer() || EventId.IsNone())
	{
		return;
	}

	MissionRareEvents.AddUnique(EventId);
	CampaignRareEvents.Add(EventId);
	Broadcast(EDarcMemoryEvent::RareEventFired, EventId, INDEX_NONE);
}

void UDarcWorldMemorySubsystem::RecordPlayerDeath(AActor* PlayerActor)
{
	if (!IsServer())
	{
		return;
	}

	MissionDeaths++;
	CampaignDeaths++;
	// Погибший больше не «стоит в комнате» — иначе команда навсегда считалась бы разделённой.
	PlayerRooms.Remove(GetPlayerIdFromActor(PlayerActor));
	RecordInteraction(TEXT("Player"), TEXT("Death"), PlayerActor);
}

void UDarcWorldMemorySubsystem::AddCampaignFact(FName Fact)
{
	if (IsServer() && !Fact.IsNone())
	{
		CampaignFacts.Add(Fact);
	}
}

// ---------------------------------------------------------------------------
// Чтение
// ---------------------------------------------------------------------------

bool UDarcWorldMemorySubsystem::GetDoorMemory(FName DoorId, FDarcDoorMemory& OutMemory) const
{
	if (const FDarcDoorMemory* Found = Doors.Find(DoorId))
	{
		OutMemory = *Found;
		return true;
	}
	return false;
}

TArray<FName> UDarcWorldMemorySubsystem::GetDoorsLeftOpen() const
{
	TArray<FName> Result;
	for (const TPair<FName, FDarcDoorMemory>& Pair : Doors)
	{
		// Только двери, которые открыл игрок, а не событие.
		if (Pair.Value.bIsOpen && Pair.Value.LastPlayerId != INDEX_NONE)
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

bool UDarcWorldMemorySubsystem::GetItemMemory(FName ItemId, FDarcItemMemory& OutMemory) const
{
	if (const FDarcItemMemory* Found = Items.Find(ItemId))
	{
		OutMemory = *Found;
		return true;
	}
	return false;
}

TArray<FName> UDarcWorldMemorySubsystem::GetMovedItems() const
{
	TArray<FName> Result;
	Items.GetKeys(Result);
	return Result;
}

bool UDarcWorldMemorySubsystem::WasRoomVisited(FName RoomId) const
{
	const FDarcRoomMemory* Room = Rooms.Find(RoomId);
	return Room && Room->VisitCount > 0;
}

bool UDarcWorldMemorySubsystem::GetRoomMemory(FName RoomId, FDarcRoomMemory& OutMemory) const
{
	if (const FDarcRoomMemory* Found = Rooms.Find(RoomId))
	{
		OutMemory = *Found;
		return true;
	}
	return false;
}

TArray<FName> UDarcWorldMemorySubsystem::GetRecentlyExitedRooms(float WithinSeconds) const
{
	TArray<FName> Result;
	const float MissionTime = GetMissionTime();
	for (const TPair<FName, FDarcRoomMemory>& Pair : Rooms)
	{
		if (Pair.Value.LastExitTime >= 0.f && MissionTime - Pair.Value.LastExitTime <= WithinSeconds)
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

FName UDarcWorldMemorySubsystem::GetPlayerRoom(int32 PlayerId) const
{
	const FName* Room = PlayerRooms.Find(PlayerId);
	return Room ? *Room : NAME_None;
}

bool UDarcWorldMemorySubsystem::ArePlayersSplit() const
{
	TSet<FName> Occupied;
	for (const TPair<int32, FName>& Pair : PlayerRooms)
	{
		Occupied.Add(Pair.Value);
	}
	return Occupied.Num() > 1;
}

bool UDarcWorldMemorySubsystem::WasObjectFound(FName ObjectId) const
{
	return FoundObjects.Contains(ObjectId);
}

int32 UDarcWorldMemorySubsystem::GetInteractionCount(FName TargetId) const
{
	int32 Count = 0;
	for (const FDarcInteractionRecord& Record : Interactions)
	{
		if (Record.TargetId == TargetId)
		{
			Count++;
		}
	}
	return Count;
}
