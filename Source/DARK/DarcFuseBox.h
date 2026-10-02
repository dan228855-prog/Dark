// DarcFuseBox.h
// Щиток одного контура: гнездо предохранителя + рубильник. «Электрощит» в уровне —
// это несколько таких щитков рядом (по одному на контур).
//
// Правильный порядок: рубильник ВЫКЛ → вставить предохранитель → рубильник ВКЛ.
// Если вставить предохранитель под нагрузкой (рубильник включён) — он сгорает и
// выбивает соседние контуры (CollateralCircuits). Это и есть «ошибка → новая проблема».
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcTaskTypes.h"
#include "DarcAssetSettings.h"
#include "DarcFuseBox.generated.h"

class UStaticMeshComponent;

UCLASS()
class DARK_API ADarcFuseBox : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcFuseBox();

	/** Какая модель у объекта (реплицируется при появлении, применяется у каждого игрока). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
	FDarcVisualSpec VisualSpec;

	/** Видимая модель (по умолчанию серая коробка, см. DarcAssetSettings). Нужна и для трейса взгляда. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	FName CircuitId;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Power")
	bool bHasFuse = true;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Power")
	bool bBreakerOn = true;

	/** Какие контуры выбивает предохранитель, сгоревший в этом щитке. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	TArray<FName> CollateralCircuits;

	/** Добавить внезапную задачу, когда предохранитель здесь сгорел (например, «Восстановить свет в коридоре»). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Tasks")
	bool bAddTaskOnBlow = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Tasks", meta = (EditCondition = "bAddTaskOnBlow"))
	FDarcTaskDefinition TaskOnBlow;

	// Подсказки — строки из String Table.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Text")
	FText PromptInsertFuse;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Text")
	FText PromptBreakerOn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Text")
	FText PromptBreakerOff;

	UFUNCTION(BlueprintPure, Category = "Power")
	bool IsSupplying() const { return bHasFuse && bBreakerOn; }

	/** Сервер: предохранитель сгорел (от ошибки здесь или от соседнего щитка, или по событию). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Power")
	void BurnFuse();

	// --- Видимое состояние (у каждой машины): вставленный предохранитель, рычаг, индикатор ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power|Visual")
	TObjectPtr<UStaticMeshComponent> FuseIndicator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power|Visual")
	TObjectPtr<UStaticMeshComponent> Lever;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power|Visual")
	TObjectPtr<UStaticMeshComponent> StatusLamp;

	/** Сервер: «дух» щёлкнул рубильником. Только рубильник — вставлять предохранители дух не может. */
	void SpiritToggleBreaker(AActor* Spirit);

	// --- IInteractable ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_State();

	/** Показ состояния (лампочка на щитке, положение рычага) — Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Power")
	void OnStateChanged();

	/** Искры, хлопок, звук — у всех клиентов. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_FuseBlown();

	UFUNCTION(BlueprintImplementableEvent, Category = "Power")
	void OnFuseBlownFX();

	/** Расставить индикаторы по лицевой стороне модели и показать текущее состояние. */
	void LayoutIndicators();
	void UpdateIndicators();
	/** Сервер: сообщение игрокам рядом (субтитр без говорящего). */
	void TellNearby(const TCHAR* Key) const;

	void HandleBlowFromMistake(AActor* Interactor);
	void NotifyStateChanged();
};
