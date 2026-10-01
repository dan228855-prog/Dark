// DarcPowerConsumerComponent.h
// Вешается на всё, что работает от электричества: лампы, сервер, терминал, электрозамок.
// Сервер выставляет состояние, клиенты получают его репликацией. Подача (свет
// включён/выключен, экран погас, гул) — в Blueprint через OnPowerChanged / OnFlicker.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarcPowerSubsystem.h"
#include "DarcPowerConsumerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDarcPowerChanged, bool, bPowered, EDarcPowerSource, Source);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDarcPowerFlicker, float, Duration);

UCLASS(ClassGroup = (DARC), meta = (BlueprintSpawnableComponent))
class DARK_API UDarcPowerConsumerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarcPowerConsumerComponent();

	/** К какому контуру подключено устройство. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	FName CircuitId;

	/** Задача, которая выполняется, когда устройство получило питание от сети через щит (пусто — нет). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Tasks")
	FName TaskIdOnMainsPower;

	/** Задача, которая выполняется, когда устройство получило питание от генератора. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Tasks")
	FName TaskIdOnGeneratorPower;

	/** Задача, которая выполняется при любом питании (например, «Восстановить свет в коридоре»). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Tasks")
	FName TaskIdOnAnyPower;

	UPROPERTY(BlueprintAssignable, Category = "Power")
	FOnDarcPowerChanged OnPowerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Power")
	FOnDarcPowerFlicker OnFlicker;

	UFUNCTION(BlueprintPure, Category = "Power")
	bool IsPowered() const { return Source != EDarcPowerSource::None; }

	UFUNCTION(BlueprintPure, Category = "Power")
	EDarcPowerSource GetSource() const { return Source; }

	/** Только сервер (вызывает UDarcPowerSubsystem). */
	void SetSource(EDarcPowerSource NewSource);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Flicker(float Duration);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Source();

	void CompleteTaskIfSet(FName TaskId);

	UPROPERTY(ReplicatedUsing = OnRep_Source)
	EDarcPowerSource Source = EDarcPowerSource::None;
};
