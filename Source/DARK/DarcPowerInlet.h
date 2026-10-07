// DarcPowerInlet.h
// Ввод для подключения генератора напрямую к контуру (например, ввод сервера).
// Игрок взаимодействует с вводом — сервер ищет ближайший генератор в пределах кабеля
// и подключает его. Если генератор откатили дальше кабеля — соединение рвётся.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcAssetSettings.h"
#include "DarcPowerInlet.generated.h"

class ADarcGenerator;

class UStaticMeshComponent;

UCLASS()
class DARK_API ADarcPowerInlet : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcPowerInlet();

	/** Какая модель у объекта (реплицируется при появлении, применяется у каждого игрока). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
	FDarcVisualSpec VisualSpec;

	/** Видимая модель (по умолчанию серая коробка, см. DarcAssetSettings). Нужна и для трейса взгляда. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Какой контур питает ввод. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	FName CircuitId;

	/** Длина кабеля генератора, см. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power", meta = (ClampMin = "50"))
	float CableLength = 600.f;

	UPROPERTY(ReplicatedUsing = OnRep_Connected, BlueprintReadOnly, Category = "Power")
	TObjectPtr<ADarcGenerator> ConnectedGenerator;

	/** Нагрузка на генератор (см. ADarcGenerator::Capacity): сервер + свет на одном генераторе — перегрузка. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Power", meta = (ClampMin = "0"))
	int32 Load = 1;

	/** Генератор подключён, но заглох (перегрузка) — E запускает его снова. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Text")
	FText PromptRestart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Text")
	FText PromptConnect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Text")
	FText PromptDisconnect;

	/** Подсказка, когда ни один генератор не достаёт кабелем (его нужно подтащить ближе). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Text")
	FText PromptTooFar;

	/** Лампа на вводе: зелёная — питание идёт от генератора, красная — нет. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> StatusLamp;

	/** Сервер: даёт ли ввод питание сейчас (генератор подключён, работает и кабель достаёт). */
	bool IsSupplying() const;

	/** Ближайший генератор, до которого достаёт кабель (и на сервере, и у клиента — для подсказки). */
	ADarcGenerator* FindGeneratorInRange() const;

	/** Сервер: разорвать соединение, если кабель не достаёт. Вызывает подсистема питания. */
	void ValidateConnection();

	// --- IInteractable ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Connected();

	/** Показ кабеля/разъёма — Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Power")
	void OnConnectionChanged(ADarcGenerator* Generator);

	bool IsInCableRange(const ADarcGenerator* Generator) const;

	virtual void Tick(float DeltaSeconds) override;

	/** Видимый кабель от ввода к подключённому генератору (у каждой машины свой). */
	void UpdateCableVisual();
	void UpdateStatusLamp();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CableSegments;

	/** Что сейчас показывает лампа (чтобы не пересоздавать материал каждый кадр). Начально — «неизвестно». */
	TOptional<bool> bLampShowsSupply;
};
