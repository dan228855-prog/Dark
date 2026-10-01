// DarcGenerator.h
// Переносной генератор — альтернативный путь подачи питания в обход щита.
// Это тяжёлый объект (ADarcHeavyObject): в коопе его тащат вдвоём, в соло — волоком.
// Взаимодействие с самим генератором = взяться/отпустить. Запускается он через ввод
// (ADarcPowerInlet): «подключить и запустить» — одно действие, чтобы не было двух
// разных действий на одном объекте.
#pragma once

#include "CoreMinimal.h"
#include "DarcHeavyObject.h"
#include "DarcGenerator.generated.h"

class UAudioComponent;

UCLASS()
class DARK_API ADarcGenerator : public ADarcHeavyObject
{
	GENERATED_BODY()

public:
	ADarcGenerator();

	UPROPERTY(ReplicatedUsing = OnRep_Running, BlueprintReadOnly, Category = "Power")
	bool bRunning = false;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Power")
	void SetRunning(bool bNewRunning);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Running();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RunningLoop;

	/** Звук запуска/работы/остановки, дым — Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Power")
	void OnRunningChanged(bool bNowRunning);
};
