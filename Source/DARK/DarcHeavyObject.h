// DarcHeavyObject.h
// Тяжёлый объект класса A (сервер-авторитетная физика): серверная стойка, генератор, шкаф.
//
// Двигают его физическим захватом (UDarcGrabComponent, ЛКМ). Масса подобрана так, что
// один игрок в коопе его не сдвинет, а вдвоём — тащат; в соло сила игрока больше.
// Здесь — только свойства самого объекта:
// - «хрупкий кабель»: при резком рывке (скорость выше BreakSpeed) кабель отсоединяется,
//   объект «вязнет» и почти не двигается, пока кабель не закрепят (E);
// - доставка: оказался у цели — выполняется задача.
//
// Сеть: физику считает сервер, позиция реплицируется (ReplicateMovement).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcAssetSettings.h"
#include "DarcHeavyObject.generated.h"

class UStaticMeshComponent;

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

	/** Масса, кг. ~80 — нужно двое (или один в соло). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy", meta = (ClampMin = "1"))
	float MassKg = 80.f;

	/** Есть ли кабель, который отсоединяется при рывке (у генератора — нет). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy")
	bool bHasFragileCable = true;

	/** Скорость, при которой кабель срывается, см/с. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy", meta = (ClampMin = "10"))
	float BreakSpeed = 260.f;

	/** Куда объект нужно доставить (пусто — без задачи доставки). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Tasks")
	TObjectPtr<AActor> DeliveryTarget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Tasks", meta = (ClampMin = "10"))
	float DeliveryRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Tasks")
	FName TaskIdOnDelivered;

	UPROPERTY(ReplicatedUsing = OnRep_Cable, BlueprintReadOnly, Category = "Heavy")
	bool bCableAttached = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Text")
	FText PromptReattachCable;

	/** Сервер: оборвать кабель (рывок, событие). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heavy")
	void DetachCable();

	// --- IInteractable (только «закрепить кабель») ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Cable();

	/** Кабель отсоединился/закреплён — звук, визуал (Blueprint). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Heavy")
	void OnCableStateChanged(bool bAttached);

	void ServerCheck();
	void ApplyCableDrag();

	FTimerHandle CheckTimer;
	bool bDelivered = false;
};
