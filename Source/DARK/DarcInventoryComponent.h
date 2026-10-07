// DarcInventoryComponent.h
// Маленький инвентарь игрока: 4 слота (клавиши 1–4). Предмет в руке занимает активный слот,
// остальные убраны (ACarryableItem::StowedBy — скрыты, едут с игроком). Выбор слота убирает
// то, что в руке, и достаёт выбранное. Поднять предмет с занятыми руками — старый уходит в
// свободный слот; слотов нет — выпадает.
//
// Сервер авторитетен: клиент только просит выбрать слот (Server_SelectSlot). Предметы могут
// уйти из руки и мимо инвентаря (вставили в щиток/слот, отдали NPC) — раз в 0.25 с сервер
// сверяет слоты с тем, что реально в руке и в кармане.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarcInventoryComponent.generated.h"

class ACarryableItem;

UCLASS(ClassGroup = (DARC), meta = (BlueprintSpawnableComponent))
class DARK_API UDarcInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarcInventoryComponent();

	static constexpr int32 NumSlots = 4;

	/** Своя машина: выбрать слот 0..3 (клавиши 1–4). */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SelectSlot(int32 Index);

	/** Сервер: перед подбором нового предмета освободить руку (в свободный слот или на пол). */
	void ServerPrepareForPickup();

	/** Сервер: предмет только что взят в руку — записать в активный слот. */
	void ServerNoteHeld(ACarryableItem* Item);

	/** Сервер: высыпать всё (смерть игрока). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void ServerDropAll();

	UFUNCTION(BlueprintPure, Category = "Inventory")
	ACarryableItem* GetSlotItem(int32 Index) const { return Slots.IsValidIndex(Index) ? Slots[Index].Get() : nullptr; }
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetActiveSlot() const { return ActiveSlot; }

	/** Есть ли у игрока (в руке или в кармане) предмет этого класса — например, рация. */
	template <class T>
	T* FindItemOfClass() const
	{
		for (const TObjectPtr<ACarryableItem>& Item : Slots)
		{
			if (T* Typed = Cast<T>(Item.Get()))
			{
				return Typed;
			}
		}
		return nullptr;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void Server_SelectSlot(int32 Index);

	/** Сервер: убрать из слотов то, что ушло мимо инвентаря; дописать предмет, взятый в обход. */
	void Sanitize();

	int32 FindFreeSlot() const;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<ACarryableItem>> Slots;

	UPROPERTY(Replicated)
	int32 ActiveSlot = 0;

	FTimerHandle SanitizeTimer;
};
