// DarcBaseUnlock.h
// Часть базы, которая открывается по прогрессу (решение автора: валюты нет, база растёт
// по сюжету). Условие — факт кампании в WorldMemory, например "Base.ArchiveTerminal",
// который выдаётся успешным выездом (UDarcMissionDefinition::CampaignFactsOnSuccess)
// или выполненной задачей (CampaignFactOnComplete).
// Что именно открывается (комната, терминал, свет под дверью) — Blueprint по событию.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarcBaseUnlock.generated.h"

class AInteractableDoor;

UCLASS()
class DARK_API ADarcBaseUnlock : public AActor
{
	GENERATED_BODY()

public:
	ADarcBaseUnlock();

	/** Какой факт кампании открывает эту часть базы. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
	FName RequiredFact;

	/** Необязательно: дверь, которая отпирается при открытии. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
	TObjectPtr<AInteractableDoor> DoorToUnlock;

	UPROPERTY(ReplicatedUsing = OnRep_Unlocked, BlueprintReadOnly, Category = "Base")
	bool bUnlocked = false;

	/** Сервер: перепроверить факт (например, после выдачи факта прямо на базе). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base")
	void Refresh();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Unlocked();

	/** Показать/спрятать часть базы у всех клиентов. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Base")
	void OnUnlockStateChanged(bool bNowUnlocked);
};
