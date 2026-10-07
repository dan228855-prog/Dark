// DarcBatteryItem.h
// Батарейка для фонарика. E по батарейке: фонарь подсел (меньше 90%) — батарейка сразу идёт в
// фонарь, иначе подбирается как обычный предмет и ждёт в инвентаре: когда фонарь сядет,
// F сам поменяет её. Разметка: DARC.Battery на любом мелком меше.
#pragma once

#include "CoreMinimal.h"
#include "CarryableItem.h"
#include "DarcBatteryItem.generated.h"

class UDarcFlashlightComponent;

UCLASS()
class DARK_API ADarcBatteryItem : public ACarryableItem
{
	GENERATED_BODY()

public:
	/** Сервер: зарядить фонарь и исчезнуть. */
	void ServerInsertInto(UDarcFlashlightComponent* Flashlight);

	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Inserted(FVector Location);

	/** Ниже этого заряда E по батарейке сразу меняет её. */
	static constexpr float InsertBelowCharge = 0.9f;
};
