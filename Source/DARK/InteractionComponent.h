// InteractionComponent.h
// Вешается на Player Character. Находит объект в прицеле и отправляет
// запрос на взаимодействие серверу — сервер авторитетен, как требует baseline.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

class ADarcTerminal;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DARK_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	/** Дальность трейса взаимодействия, в см. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractionRange = 200.f;

	/** Вызывать из инпута (например, клавиша E) на локальном клиенте владельца. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

	/** Актор, на который сейчас смотрит игрок — для показа подсказки в HUD. Чисто косметика, не для логики. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	AActor* FocusedActor = nullptr;

	/** Отправить набранный в окне терминала текст (код/команду). Вызывать из UI на локальном клиенте. */
	UFUNCTION(BlueprintCallable, Category = "Interaction|Terminal")
	void SubmitTerminalInput(ADarcTerminal* Terminal, const FString& Input);

	/** Закрыть окно терминала (освободить его для других). */
	UFUNCTION(BlueprintCallable, Category = "Interaction|Terminal")
	void LeaveTerminal(ADarcTerminal* Terminal);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Ищет ближайший интерактивный актор перед игроком через линейный трейс. */
	AActor* FindInteractableInView() const;

	/** Server RPC: сервер сам перепроверяет дистанцию и CanInteract, клиенту не доверяем. */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Interact(AActor* TargetActor);
	void Server_Interact_Implementation(AActor* TargetActor);
	bool Server_Interact_Validate(AActor* TargetActor);

	// Терминалу клиент не владеет, поэтому Server RPC идёт через свою пешку (этот компонент).
	// Сам терминал перепроверяет, что игрок — его текущий пользователь и стоит рядом.
	UFUNCTION(Server, Reliable)
	void Server_SubmitTerminalInput(ADarcTerminal* Terminal, const FString& Input);

	UFUNCTION(Server, Reliable)
	void Server_LeaveTerminal(ADarcTerminal* Terminal);
};
