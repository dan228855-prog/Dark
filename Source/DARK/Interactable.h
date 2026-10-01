// Interactable.h
// Базовый интерфейс Interaction System (P0). Реализуют его: дверь, терминал,
// переносимый объект и всё остальное, с чем игрок может взаимодействовать.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Реализуйте этот интерфейс на любом акторе, с которым игрок может
 * взаимодействовать: IDoor, ITerminal, ICarryableItem и т.д.
 */
class DARK_API IInteractable
{
	GENERATED_BODY()

public:
	/** Можно ли сейчас взаимодействовать с этим актором? Проверяется и на клиенте (для UI), и на сервере (для защиты). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* Interactor) const;

	/** Вызывается ТОЛЬКО на сервере после подтверждения взаимодействия. Сюда идёт вся игровая логика. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnInteract(AActor* Interactor);

	/** Текст подсказки в UI, например "Открыть дверь" / "Подключить накопитель". */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt() const;
};
