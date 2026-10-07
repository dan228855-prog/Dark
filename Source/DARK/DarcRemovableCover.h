// DarcRemovableCover.h
// Снимаемая крышка: решётка вентиляции, панель щитка, лючок. Закрывает проход/доступ, пока не снята.
//   Лом  — сорвать сразу, громко (слышно далеко, пишется в память мира).
//   Ключ — тихо открутить: по винту за нажатие (Screws штук).
//   Рукой — «прикручено, нужен инструмент».
// Снятая крышка исчезает (у всех) — проход открыт. Вентиляция: проход низкий, лезть пригнувшись (Ctrl).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcAssetSettings.h"
#include "DarcRemovableCover.generated.h"

class UStaticMeshComponent;

UCLASS()
class DARK_API ADarcRemovableCover : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcRemovableCover();

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
	FDarcVisualSpec VisualSpec;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cover")
	int32 Screws = 4;

	/** ID для памяти мира. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cover")
	FName MemoryId;

	/** Задача, засчитываемая снятием (например, EnterServerRoom — путь через вентиляцию). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cover")
	FName TaskIdOnRemove;

	UPROPERTY(ReplicatedUsing = OnRep_Removed, BlueprintReadOnly, Category = "Cover")
	bool bRemoved = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Cover")
	int32 ScrewsLeft = 4;

	// --- IInteractable ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Removed();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Sound(FName Slot, float Volume);

	void Remove(AActor* ByActor, bool bLoud);
};
