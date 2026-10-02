// DarcPowerLamp.h
// Лампа на электросети: горит, пока контур запитан; моргает по сигналу (аномалия, «дух»);
// гудит, пока горит. Работает без Blueprint — модель и звук из DarcAssetSettings
// (слоты "Lamp" и "LampHum"). Состояние питания реплицируется компонентом потребителя.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarcPowerSubsystem.h"
#include "DarcAssetSettings.h"
#include "DarcPowerLamp.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UAudioComponent;
class UDarcPowerConsumerComponent;

UCLASS()
class DARK_API ADarcPowerLamp : public AActor
{
	GENERATED_BODY()

public:
	ADarcPowerLamp();

	/** Какая модель у объекта (реплицируется при появлении, применяется у каждого игрока). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
	FDarcVisualSpec VisualSpec;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lamp")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Уличный натриевый фонарь: тёплый оранжевый свет, ярче и дальше комнатной лампы. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Lamp")
	bool bStreetLight = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lamp")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lamp")
	TObjectPtr<UDarcPowerConsumerComponent> Power;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandlePowerChanged(bool bPowered, EDarcPowerSource Source);

	UFUNCTION()
	void HandleFlicker(float Duration);

	void FlickerStep();
	void ApplyLit(bool bLit);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Hum;

	FTimerHandle FlickerTimer;
	float FlickerEndTime = 0.f;
};
