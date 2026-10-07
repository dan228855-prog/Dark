// DarcShadowFigure.h
// «Тень»: изредка в стороне от взгляда, в темноте коридора или в дверном проёме, стоит чёрный
// силуэт человека. Стоит посмотреть прямо на него — исчезает. Не смотрел — сам пропадает через
// несколько секунд. Только у одного игрока и только на его машине (по сети не передаётся):
// напарник ничего не видел — «тебе показалось». Появление выбирает ADarcPlayerController.
// Не смотришь на неё — она шаг за шагом подходит (всегда вне взгляда). Подошла вплотную —
// шёпот у самого уха, фонарь мигает, тень исчезает. Урона нет: растёт только страх игрока.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarcShadowFigure.generated.h"

class USkeletalMeshComponent;

UCLASS(NotPlaceable)
class DARK_API ADarcShadowFigure : public AActor
{
	GENERATED_BODY()

public:
	ADarcShadowFigure();

	/** Сколько секунд стоит, если на него не посмотрели. */
	UPROPERTY(EditAnywhere, Category = "Shadow")
	float LifeSeconds = 16.f;

	/** Раз в сколько секунд подходит ближе, пока на неё не смотрят, и на сколько (см). */
	UPROPERTY(EditAnywhere, Category = "Shadow")
	float StepInterval = 3.f;

	UPROPERTY(EditAnywhere, Category = "Shadow")
	float StepDistance = 300.f;

	/** Ближе этого (см) к игроку — шёпот и исчезновение. */
	UPROPERTY(EditAnywhere, Category = "Shadow")
	float TouchDistance = 350.f;

	/** Насколько прямо нужно посмотреть, чтобы он исчез (угол от центра взгляда, градусы). */
	UPROPERTY(EditAnywhere, Category = "Shadow")
	float LookAngle = 16.f;

	/** Найти место рядом с игроком (вне взгляда, но в прямой видимости) и поставить тень. nullptr — места нет. */
	static ADarcShadowFigure* TrySpawnNear(APlayerController* PC, const FVector* NoiseLocation = nullptr);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Shadow")
	TObjectPtr<USkeletalMeshComponent> Body;

	float Age = 0.f;
	float NextStep = 0.f;

	/** Сделать шаг к игроку, оставаясь вне его взгляда и на полу. */
	void StepCloser(const FVector& Eye, const FVector& ViewDir, const APawn* Pawn);

	/** Исчезнуть с испугом: страх игроку, звук, мигание фонаря. */
	void Vanish(bool bTouched);

	/** Подходит ли точка: свободно на рост человека, пол на уровне ног игрока, видно с глаз. */
	static bool IsStandable(UWorld* World, const FVector& Point, const FVector& Eye, const APawn* Pawn, FVector& OutFloor);
};
