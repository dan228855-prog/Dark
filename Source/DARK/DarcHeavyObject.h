// DarcHeavyObject.h
// Тяжёлый объект класса A (сервер-авторитетная физика): серверная стойка, генератор.
//
// Кооп: «взяться» могут несколько игроков; объект двигается, только когда держат
// RequiredCarriers игроков (но не больше, чем живых в сессии). Объект тянется к
// средней точке рук держателей с ограниченной скоростью.
// Соло (жив один игрок): последовательная упрощённая процедура — один игрок может
// тащить объект волоком, медленнее (SoloSpeed). Никаких одновременных неудобных действий.
//
// «Неправильное движение»: если держатели тянут слишком резко или в разные стороны
// (объект отстаёт от цели дальше BreakDistance) — отсоединяется кабель, все отпускают,
// и пока кабель не закрепят заново (взаимодействие с объектом), двигать нельзя.
//
// Сеть: всё считает сервер, позиция реплицируется (ReplicateMovement), список
// держателей реплицируется для анимаций и подсказок.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcAssetSettings.h"
#include "DarcHeavyObject.generated.h"

class UStaticMeshComponent;
class APawn;

UCLASS()
class DARK_API ADarcHeavyObject : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcHeavyObject();

	/** Какая модель у объекта (реплицируется при появлении, применяется у каждого игрока). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
	FDarcVisualSpec VisualSpec;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heavy")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Сколько игроков нужно в коопе, чтобы сдвинуть. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy", meta = (ClampMin = "1", ClampMax = "5"))
	int32 RequiredCarriers = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy", meta = (ClampMin = "1", ClampMax = "5"))
	int32 MaxCarriers = 3;

	/** Скорость, см/с, когда несут вместе. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy", meta = (ClampMin = "1"))
	float CarrySpeed = 220.f;

	/** Скорость волоком в соло, см/с. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy", meta = (ClampMin = "1"))
	float SoloSpeed = 110.f;

	/** Насколько объект может отстать от рук, прежде чем «рывок» оборвёт кабель, см. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy", meta = (ClampMin = "10"))
	float BreakDistance = 140.f;

	/** Есть ли у объекта кабель, который может отсоединиться (у генератора — нет). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy")
	bool bHasFragileCable = true;

	/** Куда объект нужно доставить (пусто — без задачи доставки). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Tasks")
	TObjectPtr<AActor> DeliveryTarget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Tasks", meta = (ClampMin = "10"))
	float DeliveryRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Tasks")
	FName TaskIdOnDelivered;

	UPROPERTY(ReplicatedUsing = OnRep_Carriers, BlueprintReadOnly, Category = "Heavy")
	TArray<TObjectPtr<APawn>> Carriers;

	UPROPERTY(ReplicatedUsing = OnRep_Cable, BlueprintReadOnly, Category = "Heavy")
	bool bCableAttached = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Text")
	FText PromptGrab;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Text")
	FText PromptRelease;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Text")
	FText PromptReattachCable;

	/** Хватает ли сейчас держателей, чтобы двигать (для подсказки «нужна помощь»). */
	UFUNCTION(BlueprintPure, Category = "Heavy")
	bool CanMoveNow() const;

	/** Сервер: отпустить всех (смерть, событие, обрыв). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heavy")
	void ReleaseAll();

	// --- IInteractable ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Carriers();

	UFUNCTION()
	void OnRep_Cable();

	/** Анимация держателей, звук скрежета — Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Heavy")
	void OnCarriersChanged();

	/** Кабель отсоединился/закреплён — звук, визуал. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Heavy")
	void OnCableStateChanged(bool bAttached);

	int32 GetAlivePlayerCount() const;
	int32 GetNeededCarriers() const;
	void AddCarrier(APawn* Pawn);
	void RemoveCarrier(APawn* Pawn);
	void UpdateCarriedState();
	void CheckDelivery();

	/** Смещение объекта относительно каждого держателя в его системе координат (поворот по yaw). */
	TMap<TWeakObjectPtr<APawn>, FVector> GripOffsets;

	bool bPhysicsWasOn = false;
	bool bDelivered = false;
};
