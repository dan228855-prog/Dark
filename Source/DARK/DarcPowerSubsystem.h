// DarcPowerSubsystem.h
// Электросистема (решение автора: полноценная — генераторы, предохранители, щиты, отключения).
//
// Модель простая и серверная:
// - Контур (CircuitId, например "Corridor", "Server") — именованная линия.
// - Контур запитан от сети, если сеть объекта есть (bMainsOn) и хотя бы один щиток
//   этого контура исправен: предохранитель стоит и рубильник включён.
// - Либо контур запитан напрямую от генератора через ввод (ADarcPowerInlet),
//   если генератор работает и стоит в пределах длины кабеля.
// - Потребители (UDarcPowerConsumerComponent на лампе, сервере, терминале) получают
//   итог через репликацию и показывают его в Blueprint.
//
// Почему UWorldSubsystem: электросеть принадлежит конкретной карте выезда и должна
// исчезать вместе с ней. Считает только сервер; на клиентах подсистема ничего не делает.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DarcPowerSubsystem.generated.h"

class UDarcPowerConsumerComponent;

/** Откуда пришло питание — нужно задачам («через щит» / «через генератор»). */
UENUM(BlueprintType)
enum class EDarcPowerSource : uint8
{
	None      UMETA(DisplayName = "Нет питания"),
	Mains     UMETA(DisplayName = "Сеть через щит"),
	Generator UMETA(DisplayName = "Генератор")
};

UCLASS()
class DARK_API UDarcPowerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Power", meta = (WorldContext = "WorldContextObject"))
	static UDarcPowerSubsystem* GetPower(const UObject* WorldContextObject);

	/** Есть ли сеть на объекте вообще (сюжетное отключение, авария). Только сервер. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Power")
	void SetMainsOn(bool bOn);

	UFUNCTION(BlueprintPure, Category = "Power")
	bool IsMainsOn() const { return bMainsOn; }

	/** Пересчитать сеть сейчас (щитки, генераторы и вводы вызывают сами при изменениях). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Power")
	void Recompute();

	/** Сервер: есть ли питание на контуре и откуда. */
	UFUNCTION(BlueprintPure, Category = "Power")
	EDarcPowerSource GetCircuitSource(FName CircuitId) const;

	/**
	 * Сервер: кратко моргнуть всеми потребителями контура (аномалии, «дух»).
	 * Состояние питания не меняется — это только показ у всех клиентов.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Power")
	void FlickerCircuit(FName CircuitId, float Duration = 1.5f);

	void RegisterConsumer(UDarcPowerConsumerComponent* Consumer);
	void UnregisterConsumer(UDarcPowerConsumerComponent* Consumer);

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	bool IsServer() const;

	/** Генератор могут откатить от ввода — проверяем дистанцию кабеля раз в секунду. */
	FTimerHandle PeriodicRecompute;

	bool bMainsOn = true;
	TMap<FName, EDarcPowerSource> CircuitSources;
	TArray<TWeakObjectPtr<UDarcPowerConsumerComponent>> Consumers;
};
