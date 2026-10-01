// DarcPowerSubsystem.cpp
#include "DarcPowerSubsystem.h"
#include "DarcPowerConsumerComponent.h"
#include "DarcFuseBox.h"
#include "DarcPowerInlet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

UDarcPowerSubsystem* UDarcPowerSubsystem::GetPower(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UDarcPowerSubsystem>() : nullptr;
}

bool UDarcPowerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Только игровые миры (PIE и игра), не превью ассетов в редакторе.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

bool UDarcPowerSubsystem::IsServer() const
{
	return GetWorld() && GetWorld()->GetNetMode() != NM_Client;
}

void UDarcPowerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (IsServer())
	{
		InWorld.GetTimerManager().SetTimer(PeriodicRecompute, this, &UDarcPowerSubsystem::Recompute, 1.f, true);
		Recompute();
	}
}

void UDarcPowerSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PeriodicRecompute);
	}
	Super::Deinitialize();
}

void UDarcPowerSubsystem::RegisterConsumer(UDarcPowerConsumerComponent* Consumer)
{
	if (IsServer() && Consumer)
	{
		Consumers.AddUnique(Consumer);
		Consumer->SetSource(GetCircuitSource(Consumer->CircuitId));
	}
}

void UDarcPowerSubsystem::UnregisterConsumer(UDarcPowerConsumerComponent* Consumer)
{
	Consumers.Remove(Consumer);
}

void UDarcPowerSubsystem::SetMainsOn(bool bOn)
{
	if (IsServer() && bMainsOn != bOn)
	{
		bMainsOn = bOn;
		Recompute();
	}
}

EDarcPowerSource UDarcPowerSubsystem::GetCircuitSource(FName CircuitId) const
{
	const EDarcPowerSource* Found = CircuitSources.Find(CircuitId);
	return Found ? *Found : EDarcPowerSource::None;
}

void UDarcPowerSubsystem::Recompute()
{
	if (!IsServer() || !GetWorld())
	{
		return;
	}

	TMap<FName, EDarcPowerSource> NewSources;

	// Сеть через щитки.
	if (bMainsOn)
	{
		for (TActorIterator<ADarcFuseBox> It(GetWorld()); It; ++It)
		{
			if (It->IsSupplying() && !It->CircuitId.IsNone())
			{
				NewSources.Add(It->CircuitId, EDarcPowerSource::Mains);
			}
		}
	}

	// Генераторы через вводы. Сеть в приоритете: если контур уже запитан от щита,
	// источником считается щит (для задачи «через щит» / «через генератор»).
	for (TActorIterator<ADarcPowerInlet> It(GetWorld()); It; ++It)
	{
		It->ValidateConnection();
		if (It->IsSupplying() && !It->CircuitId.IsNone() && !NewSources.Contains(It->CircuitId))
		{
			NewSources.Add(It->CircuitId, EDarcPowerSource::Generator);
		}
	}

	CircuitSources = MoveTemp(NewSources);

	// Раздаём потребителям. Копия — потребитель может уничтожиться в обработчике.
	const TArray<TWeakObjectPtr<UDarcPowerConsumerComponent>> Snapshot = Consumers;
	for (const TWeakObjectPtr<UDarcPowerConsumerComponent>& Weak : Snapshot)
	{
		if (UDarcPowerConsumerComponent* Consumer = Weak.Get())
		{
			Consumer->SetSource(GetCircuitSource(Consumer->CircuitId));
		}
	}
	Consumers.RemoveAll([](const TWeakObjectPtr<UDarcPowerConsumerComponent>& Weak) { return !Weak.IsValid(); });
}

void UDarcPowerSubsystem::FlickerCircuit(FName CircuitId, float Duration)
{
	if (!IsServer())
	{
		return;
	}

	for (const TWeakObjectPtr<UDarcPowerConsumerComponent>& Weak : Consumers)
	{
		UDarcPowerConsumerComponent* Consumer = Weak.Get();
		if (Consumer && Consumer->CircuitId == CircuitId && Consumer->IsPowered())
		{
			Consumer->Multicast_Flicker(Duration);
		}
	}
}
