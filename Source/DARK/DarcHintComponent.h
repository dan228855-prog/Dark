// DarcHintComponent.h
// ВРЕМЕННО (до полноценного интерфейса): название объекта и что с ним делать — HUD показывает
// их, когда игрок смотрит на объект: «Сервер (вставьте модуль, подайте питание и скопируйте архив)».
// Тексты — ключами String Table ST_UI. Чтобы убрать систему: удалить этот компонент, блок
// «Название под прицелом» в ADarcHUD и вызовы AddHint в ADarcSliceBuilder / UDarcMarkupSubsystem.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarcHintComponent.generated.h"

UCLASS(ClassGroup = (DARC), meta = (BlueprintSpawnableComponent))
class DARK_API UDarcHintComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarcHintComponent();

	/** Ключ названия в ST_UI (Name_Server). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Hint")
	FName NameKey;

	/** Ключ подсказки «что делать» в ST_UI (Hint_Server). Пусто — только название. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Hint")
	FName HintKey;

	/** «Название (подсказка)» для HUD. */
	FText GetDisplayText() const;

	/** Сервер: повесить подсказку на объект (реплицируется всем). */
	static UDarcHintComponent* AddHint(AActor* Actor, FName NameKey, FName HintKey = NAME_None);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
