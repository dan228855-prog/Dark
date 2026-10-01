// DarcPowerInlet.h
// Ввод для подключения генератора напрямую к контуру (например, ввод сервера).
// Игрок взаимодействует с вводом — сервер ищет ближайший генератор в пределах кабеля
// и подключает его. Если генератор откатили дальше кабеля — соединение рвётся.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcPowerInlet.generated.h"

class ADarcGenerator;

UCLASS()
class DARK_API ADarcPowerInlet : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcPowerInlet();

	/** Какой контур питает ввод. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	FName CircuitId;

	/** Длина кабеля генератора, см. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power", meta = (ClampMin = "50"))
	float CableLength = 600.f;

	UPROPERTY(ReplicatedUsing = OnRep_Connected, BlueprintReadOnly, Category = "Power")
	TObjectPtr<ADarcGenerator> ConnectedGenerator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Text")
	FText PromptConnect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Text")
	FText PromptDisconnect;

	/** Сервер: даёт ли ввод питание сейчас (генератор подключён, работает и кабель достаёт). */
	bool IsSupplying() const;

	/** Сервер: разорвать соединение, если кабель не достаёт. Вызывает подсистема питания. */
	void ValidateConnection();

	// --- IInteractable ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Connected();

	/** Показ кабеля/разъёма — Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Power")
	void OnConnectionChanged(ADarcGenerator* Generator);

	bool IsInCableRange(const ADarcGenerator* Generator) const;
};
