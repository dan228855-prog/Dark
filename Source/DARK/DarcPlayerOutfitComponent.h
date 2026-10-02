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

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Outfit;
};
