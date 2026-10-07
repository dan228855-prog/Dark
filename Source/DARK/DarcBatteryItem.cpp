// DarcBatteryItem.cpp
#include "DarcBatteryItem.h"
#include "DarcFlashlightComponent.h"
#include "DarcGameplayLibrary.h"
#include "DarcPlayerController.h"
#include "DarcAssetSettings.h"
#include "GameFramework/Pawn.h"

void ADarcBatteryItem::BeginPlay()
{
	if (ItemName.IsEmpty())
	{
		ItemName = UDarcGameplayLibrary::UIText(TEXT("Item_Battery"));
	}
	Super::BeginPlay();
}

void ADarcBatteryItem::OnInteract_Implementation(AActor* Interactor)
{
	// Сервер. Лежащая батарейка, фонарь подсел — сразу в фонарь; иначе — обычный подбор.
	UDarcFlashlightComponent* Flashlight = Interactor ? Interactor->FindComponentByClass<UDarcFlashlightComponent>() : nullptr;
	if (!CurrentHolder && !StowedBy && Flashlight && Flashlight->GetCharge() < InsertBelowCharge)
	{
		ServerInsertInto(Flashlight);
		return;
	}
	Super::OnInteract_Implementation(Interactor);
}

void ADarcBatteryItem::ServerInsertInto(UDarcFlashlightComponent* Flashlight)
{
	if (!HasAuthority() || !Flashlight || IsActorBeingDestroyed())
	{
		return;
	}
	Flashlight->ServerSetCharge(1.f);
	const AActor* Wearer = Flashlight->GetOwner();
	Multicast_Inserted(Wearer ? Wearer->GetActorLocation() : GetActorLocation());
	// Слот инвентаря с уничтоженным предметом очистится сам (проверка инвентаря раз в 0.25 с).
	Destroy();
}

void ADarcBatteryItem::Multicast_Inserted_Implementation(FVector Location)
{
	UDarcAssetSettings::PlaySound(this, TEXT("BatteryInsert"), Location, 0.7f);
}

FText ADarcBatteryItem::GetInteractionPrompt_Implementation() const
{
	// Подсказку рисует HUD своего игрока: смотрим на его фонарь.
	const ADarcPlayerController* PC = ADarcPlayerController::GetLocal(this);
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UDarcFlashlightComponent* Flashlight = Pawn ? Pawn->FindComponentByClass<UDarcFlashlightComponent>() : nullptr;
	if (!CurrentHolder && Flashlight && Flashlight->GetCharge() < InsertBelowCharge)
	{
		return UDarcGameplayLibrary::UIText(TEXT("Battery_Insert"));
	}
	return Super::GetInteractionPrompt_Implementation();
}
