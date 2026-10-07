// DarcPlayerOutfitComponent.h
// Внешний вид игрока: модель тела охранника (слот Characters "Player", иначе "Guard") поверх
// анимированного манекена шаблона. Манекен скрыт, но анимируется; модель повторяет его позу
// (leader pose), поэтому анимации шаблона продолжают работать. У каждой машины своя (не по сети).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarcPlayerOutfitComponent.generated.h"

class USkeletalMeshComponent;

UCLASS(ClassGroup = (DARC), meta = (BlueprintSpawnableComponent))
class DARK_API UDarcPlayerOutfitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarcPlayerOutfitComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	/** Присед, вид от первого лица: камера висит на голове рук шаблона, а у их аниматора приседа нет —
	    руки с камерой опускаются на столько же, на сколько опустилась голова тела в анимации приседа. */
	TOptional<FVector> FirstPersonBaseLocation;
	float StandingHeadAboveFeet = -1.f;
	float CrouchOffset = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Outfit;

	/** Та же модель для вида от первого лица (только у самого игрока). */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> OutfitFirstPerson;

	/** Руки от первого лица видны, только когда заняты: правая — что-то в руке, левая — фонарик включён.
	    Пустые руки не мелькают перед экраном при ходьбе. */
	void UpdateArmsVisibility();
	FTimerHandle ArmsTimer;
	TOptional<bool> bRightShown;
	TOptional<bool> bLeftShown;
};
