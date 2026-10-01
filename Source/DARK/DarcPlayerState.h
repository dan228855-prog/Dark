// DarcPlayerState.h
// Персональное состояние игрока, которое должно быть видно всем (в т.ч. для "духа"-спектатора).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DarcPlayerState.generated.h"

UCLASS()
class DARK_API ADarcPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    // Жив ли игрок в текущем выезде. false = режим "духа"/спектатора.
    UPROPERTY(ReplicatedUsing = OnRep_IsAlive, BlueprintReadOnly, Category = "Player")
    bool bIsAlive = true;

    // Счетчик смертей за кампанию - на нем завязана нарастающая сюжетка (см. areas/igra-darc.md).
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player")
    int32 DeathCount = 0;

    UFUNCTION(BlueprintCallable, Category = "Player")
    void SetAlive(bool bNewIsAlive);

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_IsAlive();

    UFUNCTION(BlueprintImplementableEvent, Category = "Player")
    void OnAliveStateChanged(bool bNewIsAlive);
};
