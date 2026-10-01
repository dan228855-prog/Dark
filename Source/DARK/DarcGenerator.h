// DarcGenerator.h
// Переносной генератор — альтернативный путь подачи питания в обход щита.
// Физический объект класса A (сервер-авторитетная физика): его двигают по полу,
// позиция реплицируется с сервера. Перенос вдвоём / соло-процедура — отдельная система
// тяжёлых объектов (следующий шаг); пока генератор можно толкать физикой.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcGenerator.generated.h"

class UStaticMeshComponent;

UCLASS()
class DARK_API ADarcGenerator : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcGenerator();

	/** Корпус — физическое тело. Меш назначается в Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Power")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(ReplicatedUsing = OnRep_Running, BlueprintReadOnly, Category = "Power")
	bool bRunning = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Text")
	FText PromptStart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Text")
	FText PromptStop;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Power")
	void SetRunning(bool bNewRunning);

	// --- IInteractable ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Running();

	/** Звук запуска/работы/остановки, дым — Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Power")
	void OnRunningChanged(bool bNowRunning);
};
