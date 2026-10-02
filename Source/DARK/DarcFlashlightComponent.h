// DarcFlashlightComponent.h
// Фонарик игрока (F): прожектор по направлению взгляда. Включение — через сервер
// (Server RPC), состояние реплицируется — луч видят все игроки. Свет у каждой машины свой.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarcFlashlightComponent.generated.h"

class USpotLightComponent;
class UStaticMeshComponent;

UCLASS(ClassGroup = (DARC), meta = (BlueprintSpawnableComponent))
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

	/** Яркость (люмены), дальность (см) и угол конуса (градусы). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	float Lumens = 900.f;

	/** Ближе этого (см) к стене луч плавно тускнеет — без пересвета «в упор». */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	float DimDistance = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	float Range = 2200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
	float ConeAngle = 24.f;

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

	UPROPERTY(Transient)
	TObjectPtr<USpotLightComponent> Spot;

	/** Видимый фонарь: корпус и светящаяся линза. У себя — у камеры слева внизу, у других — в руке. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Housing;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Lens;

	void BuildHousing();
	void UpdateLensGlow();
};
