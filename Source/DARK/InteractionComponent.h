// InteractionComponent.h
// Вешается на Player Character. Находит объект в прицеле и отправляет
// запрос на взаимодействие серверу — сервер авторитетен, как требует baseline.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

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

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Ищет ближайший интерактивный актор перед игроком через линейный трейс. */
	AActor* FindInteractableInView() const;

	/** Server RPC: сервер сам перепроверяет дистанцию и CanInteract, клиенту не доверяем. */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Interact(AActor* TargetActor);
	void Server_Interact_Implementation(AActor* TargetActor);
	bool Server_Interact_Validate(AActor* TargetActor);
};
