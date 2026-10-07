// DarcShadowFigure.h
// «Тень»: изредка в стороне от взгляда, в темноте коридора или в дверном проёме, стоит чёрный
// силуэт человека. Стоит посмотреть прямо на него — исчезает. Не смотрел — сам пропадает через
// несколько секунд. Только у одного игрока и только на его машине (по сети не передаётся):
// напарник ничего не видел — «тебе показалось». Появление выбирает ADarcPlayerController.
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
	float LifeSeconds = 10.f;

	/** Насколько прямо нужно посмотреть, чтобы он исчез (угол от центра взгляда, градусы). */
	UPROPERTY(EditAnywhere, Category = "Shadow")
	float LookAngle = 16.f;

	/** Найти место рядом с игроком (вне взгляда, но в прямой видимости) и поставить тень. nullptr — места нет. */
	static ADarcShadowFigure* TrySpawnNear(APlayerController* PC);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Shadow")
	TObjectPtr<USkeletalMeshComponent> Body;

	float Age = 0.f;
};
