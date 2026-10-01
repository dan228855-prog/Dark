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

    // Заперта ли дверь (нужен ключ/код/альтернативный способ). Реплицируется: подсказка
    // «Заперто» и CanInteract считаются на клиенте. Менять — только на сервере (SetLocked).
    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Door")
    bool bIsLocked = false;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Door")
    void SetLocked(bool bNewLocked);

    // ID двери для WorldMemory. Если пусто — берётся имя актора в уровне (оно стабильно
    // для расставленных вручную дверей). Процедурный генератор обязан задавать его явно.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
    FName MemoryId;

    // Тексты подсказок. В редакторе выбрать строки из String Table (ST_UI: Door_Open / Door_Close / Door_Locked).
    // В C++ текст для игрока не хардкодим — правило проекта (RU + EN через локализацию).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Text")
    FText PromptOpen;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Text")
    FText PromptClose;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Text")
    FText PromptLocked;

    UFUNCTION(BlueprintPure, Category = "Door")
    FName GetMemoryId() const { return MemoryId.IsNone() ? GetFName() : MemoryId; }

    // Серверная смена состояния не игроком: события, «эхо» WorldMemory, электрозамки.
    // ByActor = nullptr означает «это сделал мир, а не игрок».
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Door")
    void SetDoorOpen(bool bNewIsOpen, AActor* ByActor = nullptr);

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

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_LockedAttempt();

    // Дёрнули запертую дверь — звук ручки/замка (Blueprint).
    UFUNCTION(BlueprintImplementableEvent, Category = "Door")
    void OnLockedAttempt();
};
