// DarcItemSlot.h
// Гнездо, в которое вставляют предмет из рук: интерфейсный модуль в сервер,
// носитель в разъём, деталь в механизм. Корень актора — точка, где встаёт предмет.
// Всё серверное: вставить/вынуть решает сервер, клиенты видят результат репликацией предмета.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcAssetSettings.h"
#include "DarcItemSlot.generated.h"

class ACarryableItem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDarcSlotChanged, ACarryableItem*, Item);

class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class DARK_API ADarcItemSlot : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcItemSlot();

	/** Какая модель у объекта (реплицируется при появлении, применяется у каждого игрока). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
	FDarcVisualSpec VisualSpec;

	/** Видимая модель (по умолчанию серая коробка, см. DarcAssetSettings). Нужна и для трейса взгляда. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Куда встаёт вставленный предмет (центром): лицевая сторона разъёма. Тег InsertPoint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USceneComponent> InsertPoint;

	/** Невидимая зона прицеливания вокруг разъёма: мелкий разъём легко «поймать» взглядом. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UBoxComponent> AimBox;

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
