// DarcFootstepComponent.h
// Звуки тела персонажа: шаги по поверхности, прыжок, приземление.
//
// Чисто косметика: работает у каждой машины сама по реплицированному движению персонажа
// (скорость, режим «на земле / в воздухе»), без RPC. Поверхность определяется трейсом
// вниз по имени материала/модели пола (ключевые слова: metal, tile, wood, carpet, ...).
// Звуки — слоты DarcAssetSettings: Footstep_<Поверхность> (+ варианты _1.._8),
// запасной Footstep, а также Jump и Land.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarcFootstepComponent.generated.h"

UCLASS(ClassGroup = (DARC), meta = (BlueprintSpawnableComponent))
class DARK_API UDarcFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarcFootstepComponent();

	/** Длина шага при ходьбе / беге, см пройденного пути на один звук. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps")
	float WalkStride = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps")
	float RunStride = 210.f;

	/** Медленнее этого (см/с) шагов не слышно. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps")
	float MinSpeed = 60.f;

	/** Скорость падения (см/с), с которой приземление уже слышно. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps")
	float LandMinFallSpeed = 250.f;

	/** Определить поверхность под ногами: Concrete, Metal, Tile, Wood, Carpet, Dirt, Grass. */
	UFUNCTION(BlueprintPure, Category = "Footsteps")
	FName DetectSurface() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	void PlayBodySound(FName Slot, float Volume) const;
	void PlayStep(float Volume) const;

	float DistanceSinceStep = 0.f;
	float LastVerticalSpeed = 0.f;
	bool bWasOnGround = true;
};
