// RareEventManagerComponent.cpp
#include "RareEventManagerComponent.h"
#include "DarcRareEventAnchor.h"
#include "DarcPlayerState.h"
#include "InteractableDoor.h"
#include "CarryableItem.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

URareEventManagerComponent::URareEventManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true); // нужно для NetMulticast
}

URareEventManagerComponent* URareEventManagerComponent::GetRareEventManager(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<URareEventManagerComponent>() : nullptr;
}

void URareEventManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		if (UDarcWorldMemorySubsystem* Memory = GetMemory())
		{
			MemoryHandle = Memory->OnMemoryEvent.AddUObject(this, &URareEventManagerComponent::HandleMemoryEvent);
		}
	}
}

void URareEventManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDarcWorldMemorySubsystem* Memory = GetMemory())
	{
		Memory->OnMemoryEvent.Remove(MemoryHandle);
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimer);
	}
	Super::EndPlay(EndPlayReason);
}

UDarcWorldMemorySubsystem* URareEventManagerComponent::GetMemory() const
{
	return UDarcWorldMemorySubsystem::GetWorldMemory(this);
}

float URareEventManagerComponent::Now() const
{
	const UDarcWorldMemorySubsystem* Memory = GetMemory();
	return Memory ? Memory->GetMissionTime() : 0.f;
}

// ---------------------------------------------------------------------------
// Жизненный цикл выезда
// ---------------------------------------------------------------------------

void URareEventManagerComponent::BeginMission(int32 LevelIndex, bool bAllowStrongEvents, int32 Seed)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	CurrentLevel = LevelIndex;
	bStrongAllowed = bAllowStrongEvents;
	bMissionActive = true;
	StrongFiredThisMission = 0;
	LastEventTime = -1.f;
	LastFireTimeById.Reset();
	// Отдельный поток случайности от сида выезда — выезд можно воспроизвести.
	Random.Initialize(Seed ^ 0x0EFA);

	GetWorld()->GetTimerManager().SetTimer(EvaluationTimer, this, &URareEventManagerComponent::EvaluatePeriodic, EvaluationInterval, true);

	FDarcRareEventEvalContext Context;
	Context.Trigger = EDarcRareEventTrigger::MissionStart;
	Evaluate(Context);
}

void URareEventManagerComponent::EndMission()
{
	if (!GetOwner()->HasAuthority() || !bMissionActive)
	{
		return;
	}

	FDarcRareEventEvalContext Context;
	Context.Trigger = EDarcRareEventTrigger::MissionEnd;
	Evaluate(Context);

	bMissionActive = false;
	GetWorld()->GetTimerManager().ClearTimer(EvaluationTimer);
}

// ---------------------------------------------------------------------------
// Триггеры
// ---------------------------------------------------------------------------

void URareEventManagerComponent::HandleMemoryEvent(EDarcMemoryEvent Event, FName Id, int32 PlayerId)
{
	FDarcRareEventEvalContext Context;
	Context.ContextId = Id;
	Context.PlayerId = PlayerId;

	switch (Event)
	{
	case EDarcMemoryEvent::RoomEntered:   Context.Trigger = EDarcRareEventTrigger::RoomEntered; break;
	case EDarcMemoryEvent::RoomExited:    Context.Trigger = EDarcRareEventTrigger::RoomExited; break;
	case EDarcMemoryEvent::TaskCompleted: Context.Trigger = EDarcRareEventTrigger::TaskCompleted; break;
	default: return; // остальные записи памяти — только данные для условий
	}

	Evaluate(Context);
}

void URareEventManagerComponent::EvaluatePeriodic()
{
	FDarcRareEventEvalContext Context;
	Context.Trigger = EDarcRareEventTrigger::Periodic;
	Evaluate(Context);
}

void URareEventManagerComponent::Evaluate(const FDarcRareEventEvalContext& Context)
{
	if (!bMissionActive || !EventTable || !GetOwner()->HasAuthority())
	{
		return;
	}

	// Глобальная пауза: события должны оставаться редкими.
	if (LastEventTime >= 0.f && Now() - LastEventTime < MinSecondsBetweenEvents)
	{
		return;
	}

	// Собираем кандидатов, прошедших условия и бросок шанса.
	TArray<TPair<FName, const FDarcRareEventRow*>> Candidates;
	EventTable->ForeachRow<FDarcRareEventRow>(TEXT("RareEventManager"),
		[&](const FName& Key, const FDarcRareEventRow& Row)
		{
			if (Row.Trigger == Context.Trigger && CanFire(Key, Row, Context)
				&& Random.FRand() < Row.Chance * ChanceMultiplier)
			{
				Candidates.Emplace(Key, &Row);
			}
		});

	// Не больше одного события за проверку. Порядок перемешиваем, чтобы при
	// одновременно сработавших шансах не побеждала всегда первая строка таблицы.
	for (int32 i = Candidates.Num() - 1; i > 0; --i)
	{
		Candidates.Swap(i, Random.RandRange(0, i));
	}
	for (const TPair<FName, const FDarcRareEventRow*>& Candidate : Candidates)
	{
		if (Fire(Candidate.Key, *Candidate.Value, Context))
		{
			return;
		}
	}
}

bool URareEventManagerComponent::TryTriggerEvent(FName EventId, bool bIgnoreChance)
{
	if (!bMissionActive || !EventTable || !GetOwner()->HasAuthority())
	{
		return false;
	}

	const FDarcRareEventRow* Row = EventTable->FindRow<FDarcRareEventRow>(EventId, TEXT("RareEventManager"));
	if (!Row)
	{
		return false;
	}

	FDarcRareEventEvalContext Context;
	Context.Trigger = EDarcRareEventTrigger::Manual;

	if (!CanFire(EventId, *Row, Context))
	{
		return false;
	}
	if (!bIgnoreChance && Random.FRand() >= Row->Chance * ChanceMultiplier)
	{
		return false;
	}
	return Fire(EventId, *Row, Context);
}

// ---------------------------------------------------------------------------
// Проверки
// ---------------------------------------------------------------------------

bool URareEventManagerComponent::CanFire(FName EventId, const FDarcRareEventRow& Row, const FDarcRareEventEvalContext& Context) const
{
	const UDarcWorldMemorySubsystem* Memory = GetMemory();
	if (!Memory)
	{
		return false;
	}

	if (CurrentLevel < Row.MinLevel || CurrentLevel > Row.MaxLevel)
	{
		return false;
	}

	if (Row.FearLevel == EDarcFearLevel::Strong
		&& (!bStrongAllowed || StrongFiredThisMission >= MaxStrongEventsPerMission))
	{
		return false;
	}

	switch (Row.Repeat)
	{
	case EDarcRareEventRepeat::OncePerCampaign:
		if (Memory->HasRareEventEverFired(EventId)) return false;
		break;
	case EDarcRareEventRepeat::OncePerMission:
		if (Memory->HasRareEventFiredThisMission(EventId)) return false;
		break;
	case EDarcRareEventRepeat::Repeatable:
		if (const float* Last = LastFireTimeById.Find(EventId); Last && Now() - *Last < Row.Cooldown) return false;
		break;
	}

	for (const FDarcRareEventConditionData& Condition : Row.Conditions)
	{
		if (!CheckCondition(Condition))
		{
			return false;
		}
	}
	return true;
}

bool URareEventManagerComponent::CheckCondition(const FDarcRareEventConditionData& Condition) const
{
	const UDarcWorldMemorySubsystem* Memory = GetMemory();
	const int32 PlayerCount = GetPlayers(false).Num();

	switch (Condition.Type)
	{
	case EDarcRareEventCondition::MinPlayers:       return PlayerCount >= FMath::RoundToInt(Condition.Number);
	case EDarcRareEventCondition::MaxPlayers:       return PlayerCount <= FMath::RoundToInt(Condition.Number);
	case EDarcRareEventCondition::PlayersSplit:     return Memory->ArePlayersSplit();
	case EDarcRareEventCondition::RoomVisited:      return Memory->WasRoomVisited(Condition.Name);
	case EDarcRareEventCondition::RoomNotVisited:   return !Memory->WasRoomVisited(Condition.Name);
	case EDarcRareEventCondition::AnyItemMoved:     return Memory->GetMovedItems().Num() > 0;
	case EDarcRareEventCondition::AnyDoorLeftOpen:  return Memory->GetDoorsLeftOpen().Num() > 0;
	case EDarcRareEventCondition::ObjectFound:      return Memory->WasObjectFound(Condition.Name);
	case EDarcRareEventCondition::TaskCompleted:    return Memory->WasTaskCompleted(Condition.Name);
	case EDarcRareEventCondition::CampaignFact:     return Memory->HasCampaignFact(Condition.Name);
	case EDarcRareEventCondition::NoCampaignFact:   return !Memory->HasCampaignFact(Condition.Name);
	case EDarcRareEventCondition::EventFiredBefore: return Memory->HasRareEventEverFired(Condition.Name);
	case EDarcRareEventCondition::MinMissionTime:   return Memory->GetMissionTime() >= Condition.Number;
	case EDarcRareEventCondition::MinTotalDeaths:
		// Смерти за всю кампанию (PlayerState::DeathCount сбрасывается при смене карты).
		return Memory->GetCampaignDeaths() >= FMath::RoundToInt(Condition.Number);
	}
	return false;
}

// ---------------------------------------------------------------------------
// Запуск
// ---------------------------------------------------------------------------

bool URareEventManagerComponent::Fire(FName EventId, const FDarcRareEventRow& Row, const FDarcRareEventEvalContext& Context)
{
	UDarcWorldMemorySubsystem* Memory = GetMemory();
	ADarcPlayerState* Target = PickTargetPlayer(Context);
	if (!Memory || !Target)
	{
		return false;
	}

	ADarcRareEventAnchor* Anchor = PickAnchor(Row, Context, Target);
	if (Row.bRequiresAnchor && !Anchor)
	{
		return false; // негде показать — событие просто не случается, игра идёт дальше
	}

	// Эхо меняет мир последним, после всех проверок: если подходящей двери/предмета
	// нет, событие отменяется без побочных эффектов.
	FName ContextId = Context.ContextId;
	if (Row.EchoAction != EDarcEchoAction::None)
	{
		ContextId = ApplyEcho(Row.EchoAction);
		if (ContextId.IsNone())
		{
			return false;
		}
	}

	FDarcRareEventPayload Payload;
	Payload.EventId = EventId;
	Payload.EventType = Row.EventType;
	Payload.FearLevel = Row.FearLevel;
	Payload.Anchor = Anchor;
	Payload.TargetPlayer = Target;
	Payload.PlayerCount = GetPlayers(false).Num();
	Payload.ContextId = ContextId;
	Payload.Seed = Random.RandHelper(MAX_int32);

	// Учёт — до доставки, чтобы повторный вход в Evaluate (из записи в память) уже видел лимиты.
	LastEventTime = Now();
	LastFireTimeById.Add(EventId, LastEventTime);
	if (Row.FearLevel == EDarcFearLevel::Strong)
	{
		StrongFiredThisMission++;
	}
	Memory->RecordRareEvent(EventId);
	Memory->AddCampaignFact(Row.CampaignFactOnFire);

	if (Anchor)
	{
		Anchor->OnEventFiredOnServer(Payload);
	}

	switch (Row.Audience)
	{
	case EDarcRareEventAudience::AllPlayers:
		Multicast_RareEvent(Payload);
		break;
	case EDarcRareEventAudience::SinglePlayer:
		Target->Client_ReceiveRareEvent(Payload);
		break;
	case EDarcRareEventAudience::AllExceptOne:
		for (ADarcPlayerState* PS : GetPlayers(false))
		{
			if (PS != Target)
			{
				PS->Client_ReceiveRareEvent(Payload);
			}
		}
		break;
	}
	return true;
}

ADarcPlayerState* URareEventManagerComponent::PickTargetPlayer(const FDarcRareEventEvalContext& Context)
{
	TArray<ADarcPlayerState*> Alive = GetPlayers(true);
	if (Alive.Num() == 0)
	{
		return nullptr;
	}

	// Для триггеров «по игроку» — тот, кто его вызвал (если жив).
	if (Context.PlayerId != INDEX_NONE)
	{
		for (ADarcPlayerState* PS : Alive)
		{
			if (PS->GetPlayerId() == Context.PlayerId)
			{
				return PS;
			}
		}
	}
	return Alive[Random.RandRange(0, Alive.Num() - 1)];
}

ADarcRareEventAnchor* URareEventManagerComponent::PickAnchor(const FDarcRareEventRow& Row, const FDarcRareEventEvalContext& Context, const ADarcPlayerState* Target)
{
	const APawn* TargetPawn = Target ? Target->GetPawn() : nullptr;
	const FVector TargetLocation = TargetPawn ? TargetPawn->GetActorLocation() : FVector::ZeroVector;

	TArray<ADarcRareEventAnchor*> Suitable;
	for (TActorIterator<ADarcRareEventAnchor> It(GetWorld()); It; ++It)
	{
		ADarcRareEventAnchor* Anchor = *It;
		if (!Anchor->SupportsType(Row.EventType))
		{
			continue;
		}
		if (Row.bAnchorInTriggerRoom && Anchor->RoomId != Context.ContextId)
		{
			continue;
		}
		if (TargetPawn && (Row.MinDistance > 0.f || Row.MaxDistance > 0.f))
		{
			const float Dist = FVector::Dist(TargetLocation, Anchor->GetActorLocation());
			if (Dist < Row.MinDistance || (Row.MaxDistance > 0.f && Dist > Row.MaxDistance))
			{
				continue;
			}
		}
		Suitable.Add(Anchor);
	}

	return Suitable.Num() > 0 ? Suitable[Random.RandRange(0, Suitable.Num() - 1)] : nullptr;
}

FName URareEventManagerComponent::ApplyEcho(EDarcEchoAction Action)
{
	UDarcWorldMemorySubsystem* Memory = GetMemory();
	if (!Memory)
	{
		return NAME_None;
	}

	// Эхо не должно происходить у игрока на глазах — только вдали от всех живых игроков.
	constexpr float MinDistanceFromPlayers = 1000.f;
	auto IsUnobserved = [this](const AActor* Actor)
	{
		for (const ADarcPlayerState* PS : GetPlayers(true))
		{
			if (const APawn* Pawn = PS->GetPawn();
				Pawn && FVector::Dist(Pawn->GetActorLocation(), Actor->GetActorLocation()) < MinDistanceFromPlayers)
			{
				return false;
			}
		}
		return true;
	};

	switch (Action)
	{
	case EDarcEchoAction::CloseDoorLeftOpen:
	{
		const TArray<FName> OpenDoors = Memory->GetDoorsLeftOpen();
		for (TActorIterator<AInteractableDoor> It(GetWorld()); It; ++It)
		{
			if (OpenDoors.Contains(It->GetMemoryId()) && It->bIsOpen && IsUnobserved(*It))
			{
				It->SetDoorOpen(false, nullptr); // nullptr = «это сделал мир», в память ляжет как не игрок
				return It->GetMemoryId();
			}
		}
		return NAME_None;
	}
	case EDarcEchoAction::NudgeMovedItem:
	{
		const TArray<FName> Moved = Memory->GetMovedItems();
		for (TActorIterator<ACarryableItem> It(GetWorld()); It; ++It)
		{
			if (Moved.Contains(It->GetMemoryId()) && !It->CurrentHolder && IsUnobserved(*It))
			{
				// «Снова слегка сдвинут»: 10–25 см в сторону и небольшой поворот.
				const FVector Offset = FVector(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal()
					* Random.FRandRange(10.f, 25.f);
				It->SetActorLocation(It->GetActorLocation() + Offset, false, nullptr, ETeleportType::TeleportPhysics);
				It->AddActorWorldRotation(FRotator(0.f, Random.FRandRange(-15.f, 15.f), 0.f), false, nullptr, ETeleportType::TeleportPhysics);
				return It->GetMemoryId();
			}
		}
		return NAME_None;
	}
	default:
		return NAME_None;
	}
}

TArray<ADarcPlayerState*> URareEventManagerComponent::GetPlayers(bool bAliveOnly) const
{
	TArray<ADarcPlayerState*> Result;
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return Result;
	}

	for (APlayerState* PS : GameState->PlayerArray)
	{
		ADarcPlayerState* DarcPS = Cast<ADarcPlayerState>(PS);
		if (DarcPS && (!bAliveOnly || DarcPS->bIsAlive))
		{
			Result.Add(DarcPS);
		}
	}
	return Result;
}

// ---------------------------------------------------------------------------
// Доставка на клиентах
// ---------------------------------------------------------------------------

void URareEventManagerComponent::Multicast_RareEvent_Implementation(const FDarcRareEventPayload& Payload)
{
	DeliverLocally(Payload);
}

void URareEventManagerComponent::DeliverLocally(const FDarcRareEventPayload& Payload)
{
	// Выделенный сервер (в будущем) ничего не показывает — только машины с игроком.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (Payload.Anchor)
	{
		Payload.Anchor->PlayEventLocally(Payload);
	}
	OnRareEventReceived.Broadcast(Payload);
}
