// DarcGrassScatter.h
// Трава вдоль дороги и вокруг площадок: много мелких моделей одним инстансированным
// компонентом на каждую модель (HISM) — дёшево для FPS (без теней, без коллизии, гаснет
// с расстоянием). Расстановка детерминированная (сид), у всех игроков одинаковая и
// строится у каждой машины сама — по сети ничего не передаётся.
// Каждая травинка-кочка случайно повёрнута и масштабирована, чтобы квадратные куски
// не складывались в повторяющийся узор.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarcGrassScatter.generated.h"

class UStaticMesh;
class UHierarchicalInstancedStaticMeshComponent;

USTRUCT(BlueprintType)
struct FDarcGrassVariety
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Grass")
	TObjectPtr<UStaticMesh> Mesh;

	/** Относительная частота среди остальных моделей. */
	UPROPERTY(EditAnywhere, Category = "Grass", meta = (ClampMin = "0"))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, Category = "Grass")
	FVector2D ScaleRange = FVector2D(0.8f, 1.25f);

	/** Насколько наклонять по склону (0 — всегда вертикально, 1 — по нормали земли). */
	UPROPERTY(EditAnywhere, Category = "Grass", meta = (ClampMin = "0", ClampMax = "1"))
	float AlignToGround = 0.6f;
};

UCLASS()
class DARK_API ADarcGrassScatter : public AActor
{
	GENERATED_BODY()

public:
	ADarcGrassScatter();

	UPROPERTY(EditAnywhere, Category = "Grass")
	TArray<FDarcGrassVariety> Varieties;

	/** Осевая линия дороги (мировые точки): трава полосой по обе стороны. */
	UPROPERTY(EditAnywhere, Category = "Grass")
	TArray<FVector> PathPoints;

	/** Полуширина дороги без травы и ширина полосы травы за ней, см. */
	UPROPERTY(EditAnywhere, Category = "Grass")
	float RoadHalfWidth = 850.f;

	UPROPERTY(EditAnywhere, Category = "Grass")
	float BandWidth = 3500.f;

	/** Дополнительные области с травой (мировые коробки, например вокруг забора). */
	UPROPERTY(EditAnywhere, Category = "Grass")
	TArray<FBox> Areas;

	/** Где травы быть не должно (площадка внутри забора, будка КПП). */
	UPROPERTY(EditAnywhere, Category = "Grass")
	TArray<FBox> Exclusions;

	/** Шаг сетки, см: одна кочка на клетку (с разбросом). Меньше — гуще. */
	UPROPERTY(EditAnywhere, Category = "Grass", meta = (ClampMin = "20"))
	float Spacing = 110.f;

	/** Склон круче этого (нормаль Z меньше) — скала, травы нет. */
	UPROPERTY(EditAnywhere, Category = "Grass")
	float MaxSlopeNormalZ = 0.87f;

	/** Ниже этой высоты (мировой Z, см) травы нет — дно водоёмов: вода не блокирует трейс. */
	UPROPERTY(EditAnywhere, Category = "Grass")
	float MinGroundZ = -1000000.f;

	/** На каком расстоянии трава начинает и заканчивает исчезать, см. */
	UPROPERTY(EditAnywhere, Category = "Grass")
	int32 CullStart = 2500;

	UPROPERTY(EditAnywhere, Category = "Grass")
	int32 CullEnd = 4500;

	UPROPERTY(EditAnywhere, Category = "Grass")
	int32 Seed = 1337;

	/** Пересобрать траву в редакторе (посмотреть результат без запуска игры). */
	UFUNCTION(CallInEditor, Category = "Grass")
	void Regenerate();

	UFUNCTION(CallInEditor, Category = "Grass")
	void Clear();

protected:
	virtual void BeginPlay() override;

private:
	float DistanceToPath(const FVector2D& P) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GrassLayers;
};
