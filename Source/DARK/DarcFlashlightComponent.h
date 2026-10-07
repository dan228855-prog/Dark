// DarcFlashlightComponent.h
// Фонарик игрока (F): прожектор по направлению взгляда. Включение — через сервер
// (Server RPC), состояние реплицируется — луч видят все игроки. Свет у каждой машины свой.
// Батарейка: включённый фонарь садится (заряд считает сервер), ниже четверти — тускнеет и мигает,
// в ноль — гаснет. Новая батарейка — подобрать её (E) или нажать F с батарейкой в инвентаре.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarcFlashlightComponent.generated.h"

class USpotLightComponent;
class UStaticMeshComponent;

UCLASS(ClassGroup = (DARC), Config = Game, meta = (BlueprintSpawnableComponent))
class DARK_API UDarcFlashlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarcFlashlightComponent();

	/** Локальный игрок: включить/выключить (запрос на сервер). */
	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void Toggle();

	UFUNCTION(BlueprintPure, Category = "Flashlight")
	bool IsOn() const { return bOn; }

	/** Заряд батарейки 0..1. */
	UFUNCTION(BlueprintPure, Category = "Flashlight")
	float GetCharge() const { return Charge; }

	/** На сколько секунд включённого света хватает полной батарейки. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Flashlight")
	float BatterySeconds = 420.f;

	/** Сервер: поставить заряд (новая батарейка — 1). */
	void ServerSetCharge(float NewCharge);

	/** Своя машина: несколько секунд фонарь мигает (рядом «тень»). */
	void FlickerFor(float Seconds);

	/** Яркость (люмены), дальность (см) и угол конуса (градусы). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	float Lumens = 100.f;

	/** Ближе этого (см) к стене луч плавно тускнеет — без пересвета «в упор». */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	float DimDistance = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	float Range = 2200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	float ConeAngle = 24.f;

	/**
	 * Фонарь в левой руке: сдвиг и поворот корпуса от кости кисти hand_l в её осях (луч всегда по
	 * взгляду). Поворот (0,0,0) — корпус смотрит по взгляду; любой другой — зафиксирован в кисти.
	 * Оба можно менять у компонента Flashlight во время игры — применяются каждый кадр.
	 * Настраивается в Config/DefaultGame.ini, раздел [/Script/DARK.DarcFlashlightComponent].
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Flashlight")
	FVector HandOffset = FVector(0.16f, -3.5f, -3.f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Flashlight")
	FRotator HandRotation = FRotator::ZeroRotator;

	/** Заново вложить фонарь в руку (поза рук сменилась — например, игрок вышел из машины). */
	void ResetHandPlacement();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void Server_SetOn(bool bNewOn);

	UFUNCTION()
	void OnRep_On();

	UPROPERTY(ReplicatedUsing = OnRep_On)
	bool bOn = false;

	UPROPERTY(Replicated)
	float Charge = 1.f;

	/** Сервер: батарейка из инвентаря или из руки — в фонарь. false — батарейки нет. */
	bool ServerUseSpareBattery();

	/** Мигание: до какого времени и текущий множитель яркости (у каждой машины своё). */
	float FlickerUntil = 0.f;
	float FlickerFactor = 1.f;
	float NextFlickerChange = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<USpotLightComponent> Spot;

	/** Видимый фонарь: корпус и светящаяся линза. У себя — у камеры слева внизу, у других — в руке. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Housing;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Lens;

	void BuildHousing();
	/** Вложить фонарь в левую руку (первый кадр, когда поза рук уже посчитана). */
	void PlaceInHand();
	bool bPlacedInHand = false;
	bool bPlacedForLocal = false;
	void UpdateLensGlow();
};
