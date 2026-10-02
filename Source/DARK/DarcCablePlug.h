// DarcCablePlug.h
// Вилка кабеля питания: переносимый предмет на проводе от розетки на стене. Изначально
// лежит у розетки. Игрок берёт её (E), несёт к оборудованию (серверной стойке) и подключает
// (E по стойке). Провод виден всегда: от розетки до вилки — на полу, в руке или в стойке.
//
// Механика провода:
// - длина ограничена: унести/утащить дальше — вилку вырывает (из руки или из стойки);
// - натянутый подключённый провод можно задеть: если игрок на бегу пересекает его — вилка выпадает;
// - провод провисает и лежит на полу, не проходит сквозь пол.
// Сервер решает, где вилка (CurrentHolder реплицируется); провод рисует каждая машина сама.
#pragma once

#include "CoreMinimal.h"
#include "CarryableItem.h"
#include "DarcCablePlug.generated.h"

class UStaticMeshComponent;

UCLASS()
class DARK_API ADarcCablePlug : public ACarryableItem
{
	GENERATED_BODY()

public:
	ADarcCablePlug();

	/** Розетка на стене — откуда идёт провод (мировые координаты). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Cable")
	FVector SocketLocation = FVector::ZeroVector;

	/** Длина провода, см. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Cable", meta = (ClampMin = "50"))
	float CableLength = 500.f;

	/** Скорость поперёк натянутого провода, с которой игрок его выдёргивает, см/с. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cable")
	float TripSpeed = 280.f;

	/** Подключена ли вилка к оборудованию (держит не игрок). */
	UFUNCTION(BlueprintPure, Category = "Cable")
	bool IsPluggedIn() const;

	/** Сервер: выдернуть вилку (упадёт на пол). */
	void Unplug();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void UpdateCableVisual();
	void ServerCheck();
	FVector CurvePoint(float T, const FVector& End, float Slack) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Segments;

	FTimerHandle CheckTimer;
};
