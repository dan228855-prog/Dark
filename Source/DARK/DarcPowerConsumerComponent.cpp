// DarcPowerConsumerComponent.cpp
#include "DarcPowerConsumerComponent.h"
#include "TaskManagerComponent.h"
#include "Net/UnrealNetwork.h"

UDarcPowerConsumerComponent::UDarcPowerConsumerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDarcPowerConsumerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDarcPowerConsumerComponent, Source);
}

void UDarcPowerConsumerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UDarcPowerSubsystem* Power = UDarcPowerSubsystem::GetPower(this))
	{
		Power->RegisterConsumer(this); // на клиенте вызов ничего не делает
	}
	OnRep_Source(); // начальный показ (свет выключен до первого пересчёта)
}

void UDarcPowerConsumerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDarcPowerSubsystem* Power = UDarcPowerSubsystem::GetPower(this))
	{
		Power->UnregisterConsumer(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UDarcPowerConsumerComponent::SetSource(EDarcPowerSource NewSource)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Source == NewSource)
	{
		return;
	}

	Source = NewSource;
	OnRep_Source(); // на сервере OnRep сам не вызывается

	// Задачи закрываются по факту питания — неважно, кто и как его дал.
	if (Source == EDarcPowerSource::Mains)
	{
		CompleteTaskIfSet(TaskIdOnMainsPower);
	}
	else if (Source == EDarcPowerSource::Generator)
	{
		CompleteTaskIfSet(TaskIdOnGeneratorPower);
	}
	if (Source != EDarcPowerSource::None)
	{
		CompleteTaskIfSet(TaskIdOnAnyPower);
	}
}

void UDarcPowerConsumerComponent::CompleteTaskIfSet(FName TaskId)
{
	if (TaskId.IsNone())
	{
		return;
	}
	// Если задачи ещё/уже нет (например, «восстановить свет» до поломки) — CompleteTask вернёт false, это нормально.
	if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
	{
		Tasks->CompleteTask(TaskId, nullptr);
	}
}

void UDarcPowerConsumerComponent::OnRep_Source()
{
	OnPowerChanged.Broadcast(IsPowered(), Source);
}

void UDarcPowerConsumerComponent::Multicast_Flicker_Implementation(float Duration)
{
	OnFlicker.Broadcast(Duration);
}
