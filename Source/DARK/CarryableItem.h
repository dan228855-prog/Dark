// CarryableItem.h
// Переносимый объект (Item / Carry System, P0). Игрок подбирает предмет,
// он "прикрепляется" к руке/сокету, сервер авторитетен по владению.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "CarryableItem.generated.h"

UCLASS()
class DARK_API ACarryableItem : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    ACarryableItem();

    // Кто сейчас держит предмет. nullptr, если предмет лежит на земле.
    // Реплицируется, чтобы все клиенты видели один и тот же предмет "в руке" одного игрока.
    UPROPERTY(ReplicatedUsing = OnRep_Holder, BlueprintReadOnly, Category = "Carry")
    AActor* CurrentHolder = nullptr;

    // Имя сокета на скелете игрока, куда крепится предмет при переноске.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
    FName CarrySocketName = TEXT("hand_r_socket");

    // Игрок вызывает это же взаимодействие и на подбор, и на выброс (переключение).
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual void OnInteract_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionPrompt_Implementation() const override;

    // Явный сброс предмета (например, если игрок погиб/потерял сознание - см. решение про "дух").
    UFUNCTION(BlueprintCallable, Category = "Carry")
    void ForceDrop();

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_Holder();

    // Сюда в Blueprint добавляете физическую attach-логику / выключение физики при подборе и т.п.
    UFUNCTION(BlueprintImplementableEvent, Category = "Carry")
    void OnHolderChanged(AActor* NewHolder);

    void AttachToHolder(AActor* Holder);
    void DetachFromHolder();
};
