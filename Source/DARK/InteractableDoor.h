// InteractableDoor.h
// Простая дверь с сервер-авторитетным состоянием (открыта/закрыта).
// Реализует IInteractable, использует уже готовый InteractionComponent.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "InteractableDoor.generated.h"

UCLASS()
class DARK_API AInteractableDoor : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    AInteractableDoor();

    // Открыта ли дверь. Реплицируется всем клиентам, меняется только сервером.
    UPROPERTY(ReplicatedUsing = OnRep_IsOpen, BlueprintReadOnly, Category = "Door")
    bool bIsOpen = false;

    // Заперта ли дверь (нужен ключ/код/альтернативный способ).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    bool bIsLocked = false;

    // --- IInteractable ---
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual void OnInteract_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionPrompt_Implementation() const override;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_IsOpen();

    UFUNCTION(BlueprintImplementableEvent, Category = "Door")
    void OnDoorStateChanged(bool bNewIsOpen);
};
