// DarcItemSlot.h
// Гнездо, в которое вставляют предмет из рук: интерфейсный модуль в сервер,
// носитель в разъём, деталь в механизм. Корень актора — точка, где встаёт предмет.
// Всё серверное: вставить/вынуть решает сервер, клиенты видят результат репликацией предмета.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcItemSlot.generated.h"

class ACarryableItem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDarcSlotChanged, ACarryableItem*, Item);

UCLASS()
class DARK_API ADarcItemSlot : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcItemSlot();

	/** Какие предметы подходят (класс или его наследники). Пусто — любой переносимый предмет. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	TSubclassOf<ACarryableItem> AcceptedClass;

	/** Предмет, который стоит в слоте с начала уровня (например, носитель в сервере). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	TObjectPtr<ACarryableItem> InitialItem;

	/** Можно ли сейчас вынуть предмет руками. Устройства переключают это сами (сервер). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "Slot")
	bool bAllowRemove = true;

	/** Задача при вставке подходящего предмета (пусто — нет). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot|Tasks")
	FName TaskIdOnInsert;

	/** Задача, когда предмет вынули. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot|Tasks")
	FName TaskIdOnRemove;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot|Text")
	FText PromptInsert;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot|Text")
	FText PromptRemove;

	/** Сервер: срабатывает при вставке/извлечении (nullptr — слот опустел). */
	UPROPERTY(BlueprintAssignable, Category = "Slot")
	FOnDarcSlotChanged OnSlotChanged;

	UFUNCTION(BlueprintPure, Category = "Slot")
	ACarryableItem* GetInsertedItem() const;

	UFUNCTION(BlueprintPure, Category = "Slot")
	bool HasItem() const { return GetInsertedItem() != nullptr; }

	// --- IInteractable ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool Accepts(const ACarryableItem* Item) const;
	void CompleteTaskIfSet(FName TaskId, AActor* ByActor);
};
