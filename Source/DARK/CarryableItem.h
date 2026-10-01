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

    // ID предмета для WorldMemory. Если пусто — имя актора в уровне.
    // Процедурный генератор и заспавненные в рантайме предметы обязаны задавать его явно.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
    FName MemoryId;

    // Тексты подсказок — из String Table (ST_UI: Carry_PickUp / Carry_Drop), не из C++.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry|Text")
    FText PromptPickUp;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry|Text")
    FText PromptDrop;

    UFUNCTION(BlueprintPure, Category = "Carry")
    FName GetMemoryId() const { return MemoryId.IsNone() ? GetFName() : MemoryId; }

    // Игрок вызывает это же взаимодействие и на подбор, и на выброс (переключение).
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual void OnInteract_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionPrompt_Implementation() const override;

    // Что сейчас держит этот игрок (nullptr — ничего). Для механик «вставить то, что в руках».
    UFUNCTION(BlueprintPure, Category = "Carry", meta = (DefaultToSelf = "Holder"))
    static ACarryableItem* FindItemHeldBy(const AActor* Holder);

    // Сервер: передать предмет новому держателю — игроку или слоту/устройству (nullptr = положить).
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Carry")
    void ServerTransferTo(AActor* NewHolder, AActor* ByActor);

    // Явный сброс предмета (например, если игрок погиб/потерял сознание - см. решение про "дух").
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Carry")
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

    // Предмет был физическим до того, как его взяли, — вернуть физику при отпускании.
    bool bRestorePhysicsOnDetach = false;

    // Где предмет лежал до того, как его подняли, — для записи перемещения в WorldMemory.
    FTransform PickUpTransform;

    // Отпустить предмет на сервере и записать перемещение в память мира.
    void ServerRelease(AActor* ByActor);
};
