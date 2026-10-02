// DarcBarrierGate.h
// Шлагбаум у КПП: стрела с коллизией, плавно поднимается, когда к нему подходит игрок, и
// опускается, когда рядом никого нет. Чисто локальный (у каждой машины свой) — решение
// принимается по позициям игроков, которые и так реплицируются, поэтому у всех одинаково.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarcBarrierGate.generated.h"

class UStaticMeshComponent;

UCLASS(NotPlaceable)
class DARK_API ADarcBarrierGate : public AActor
{
	GENERATED_BODY()

public:
	ADarcBarrierGate();

	/** Длина стрелы, см (стрела уходит вдоль локальной -Y от стойки). */
	UPROPERTY(EditAnywhere, Category = "Barrier")
	float ArmLength = 560.f;

	/** На каком расстоянии от стойки игрок «открывает» шлагбаум, см. */
	UPROPERTY(EditAnywhere, Category = "Barrier")
	float TriggerRadius = 600.f;

	/** Угол подъёма, градусы, и время полного подъёма/опускания, с. */
	UPROPERTY(EditAnywhere, Category = "Barrier")
	float OpenAngle = 82.f;

	UPROPERTY(EditAnywhere, Category = "Barrier")
	float OpenSeconds = 2.2f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ArmParts;

	/** 0 — закрыт, 1 — открыт; угол — сглаженная кривая от этого значения. */
	float OpenAlpha = 0.f;
	bool bWasOpening = false;
};
