// DarcBaseUnlock.cpp
#include "DarcBaseUnlock.h"
#include "DarcWorldMemorySubsystem.h"
#include "InteractableDoor.h"
#include "Net/UnrealNetwork.h"

ADarcBaseUnlock::ADarcBaseUnlock()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADarcBaseUnlock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADarcBaseUnlock, bUnlocked);
}

void ADarcBaseUnlock::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Refresh();
	}
	OnRep_Unlocked(); // начальный показ
}

void ADarcBaseUnlock::Refresh()
{
	if (!HasAuthority())
	{
		return;
	}

	// Факты кампании живут только на сервере (память хоста), клиентам уходит итог.
	const UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this);
	const bool bNow = RequiredFact.IsNone() || (Memory && Memory->HasCampaignFact(RequiredFact));

	if (DoorToUnlock)
	{
		DoorToUnlock->SetLocked(!bNow);
	}
	if (bNow != bUnlocked)
	{
		bUnlocked = bNow;
		OnRep_Unlocked();
	}
}

void ADarcBaseUnlock::OnRep_Unlocked()
{
	OnUnlockStateChanged(bUnlocked);
}
