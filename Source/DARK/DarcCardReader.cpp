// DarcCardReader.cpp
#include "DarcCardReader.h"
#include "CarryableItem.h"
#include "InteractableDoor.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"

ADarcCardReader::ADarcCardReader()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

bool ADarcCardReader::CanInteract_Implementation(AActor* Interactor) const
{
	return true; // без карты тоже можно приложить «пустую руку» — получить отказ
}

void ADarcCardReader::OnInteract_Implementation(AActor* Interactor)
{
	// Сервер: проверяем, что у игрока в руках именно карта.
	const ACarryableItem* Held = ACarryableItem::FindItemHeldBy(Interactor);
	const bool bGranted = AcceptedClass && Held && Held->IsA(AcceptedClass);

	Multicast_Result(bGranted);
	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(GetFName(), bGranted ? TEXT("AccessGranted") : TEXT("AccessDenied"), Interactor);
	}
	if (!bGranted)
	{
		return;
	}

	if (DoorToUnlock)
	{
		DoorToUnlock->SetLocked(false);
	}
	if (!TaskIdOnAccess.IsNone())
	{
		if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
		{
			Tasks->CompleteTask(TaskIdOnAccess, Interactor);
		}
	}
}

void ADarcCardReader::Multicast_Result_Implementation(bool bGranted)
{
	OnAccessResult(bGranted);
}

FText ADarcCardReader::GetInteractionPrompt_Implementation() const
{
	return PromptSwipe;
}
