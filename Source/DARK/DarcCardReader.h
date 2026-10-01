// DarcCardReader.h
// Считыватель карты доступа у двери. Игрок подходит с картой в руках — дверь отпирается.
// Карта остаётся у игрока (её номер нужен ещё и для кода на терминале).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcAssetSettings.h"
#include "DarcCardReader.generated.h"

class ACarryableItem;
class AInteractableDoor;

class UStaticMeshComponent;

UCLASS()
class DARK_API ADarcCardReader : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcCardReader();

	/** Какая модель у объекта (реплицируется при появлении, применяется у каждого игрока). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
	FDarcVisualSpec VisualSpec;

	/** Видимая модель (по умолчанию серая коробка, см. DarcAssetSettings). Нужна и для трейса взгляда. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Класс карты доступа. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Access")
	TSubclassOf<ACarryableItem> AcceptedClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Access")
	TObjectPtr<AInteractableDoor> DoorToUnlock;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Access|Tasks")
	FName TaskIdOnAccess;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Access|Text")
	FText PromptSwipe;

	// --- IInteractable ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Result(bool bGranted);

	/** «Доступ разрешён / запрещён» — писк, лампочка (Blueprint). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Access")
	void OnAccessResult(bool bGranted);
};
