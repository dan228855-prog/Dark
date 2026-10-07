// DarcInventoryComponent.cpp
#include "DarcInventoryComponent.h"
#include "CarryableItem.h"
#include "DarcAssetSettings.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UDarcInventoryComponent::UDarcInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	Slots.SetNum(NumSlots);
}

void UDarcInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDarcInventoryComponent, Slots);
	DOREPLIFETIME(UDarcInventoryComponent, ActiveSlot);
}

void UDarcInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	Slots.SetNum(NumSlots);
	if (GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(SanitizeTimer, this, &UDarcInventoryComponent::Sanitize, 0.25f, true);
	}
}

void UDarcInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SanitizeTimer);
	}
	// Игрок ушёл/пешку убрали — карманные предметы не должны остаться невидимыми навсегда.
	if (GetOwner()->HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed)
	{
		ServerDropAll();
	}
	Super::EndPlay(EndPlayReason);
}

void UDarcInventoryComponent::Sanitize()
{
	AActor* Owner = GetOwner();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		ACarryableItem* Item = Slots[i];
		if (!Item)
		{
			continue;
		}
		// Активный слот — то, что в руке; остальные — в кармане.
		const bool bValid = IsValid(Item) && (i == ActiveSlot ? Item->CurrentHolder == Owner : Item->StowedBy == Owner);
		if (!bValid)
		{
			Slots[i] = nullptr;
		}
	}
	if (!Slots[ActiveSlot])
	{
		// Взяли в руку в обход инвентаря (выдал NPC, вытолкнуло устройство).
		Slots[ActiveSlot] = ACarryableItem::FindItemHeldBy(Owner);
	}
}

int32 UDarcInventoryComponent::FindFreeSlot() const
{
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (i != ActiveSlot && !Slots[i])
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UDarcInventoryComponent::ServerPrepareForPickup()
{
	Sanitize();
	ACarryableItem* Hand = Slots[ActiveSlot];
	if (!Hand)
	{
		return;
	}
	const int32 Free = FindFreeSlot();
	if (Free != INDEX_NONE && Hand->bCanStow)
	{
		Hand->ServerStow(GetOwner());
		ActiveSlot = Free;
	}
	else
	{
		Hand->ForceDrop();
		Slots[ActiveSlot] = nullptr;
	}
}

void UDarcInventoryComponent::ServerNoteHeld(ACarryableItem* Item)
{
	if (Item && Item->CurrentHolder == GetOwner())
	{
		Slots[ActiveSlot] = Item;
	}
}

void UDarcInventoryComponent::SelectSlot(int32 Index)
{
	if (Index >= 0 && Index < NumSlots)
	{
		Server_SelectSlot(Index);
	}
}

void UDarcInventoryComponent::Server_SelectSlot_Implementation(int32 Index)
{
	if (Index < 0 || Index >= NumSlots || Index == ActiveSlot)
	{
		return;
	}
	Sanitize();
	AActor* Owner = GetOwner();
	if (ACarryableItem* Hand = Slots[ActiveSlot])
	{
		if (Hand->bCanStow)
		{
			Hand->ServerStow(Owner);
		}
		else
		{
			Hand->ForceDrop(); // вилку на проводе в карман не убрать — выпускаем из руки
			Slots[ActiveSlot] = nullptr;
		}
	}
	ActiveSlot = Index;
	if (ACarryableItem* Next = Slots[ActiveSlot])
	{
		Next->ServerUnstowTo(Owner);
	}
	else
	{
		UDarcAssetSettings::PlaySound(this, TEXT("Stow"), Owner->GetActorLocation(), 0.6f);
	}
}

void UDarcInventoryComponent::ServerDropAll()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	for (TObjectPtr<ACarryableItem>& Item : Slots)
	{
		if (IsValid(Item))
		{
			if (Item->StowedBy == GetOwner())
			{
				Item->ServerDropFromStow(GetOwner());
			}
			else if (Item->CurrentHolder == GetOwner())
			{
				Item->ForceDrop();
			}
		}
		Item = nullptr;
	}
}
