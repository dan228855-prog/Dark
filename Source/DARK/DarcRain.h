// DarcRain.h
// Еле заметный дождь вокруг камеры своего игрока: тонкие полупрозрачные струйки (один
// инстансированный меш, без частиц) и тихий звук. Под крышей (над камерой статика) — струйки
// прячутся, звук глохнет. Каждая машина считает свой дождь сама, по сети ничего не идёт.
// Ставится на карту одним актором (папка Environment).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarcRain.generated.h"

class UInstancedStaticMeshComponent;
class UAudioComponent;

UCLASS()
class DARK_API ADarcRain : public AActor
{
	GENERATED_BODY()

public:
	ADarcRain();

	/** Сколько струек вокруг камеры. */
	UPROPERTY(EditAnywhere, Category = "Rain", meta = (ClampMin = "0", ClampMax = "4000"))
	int32 DropCount = 450;

	/** Радиус области дождя вокруг камеры и её высота, см. */
	UPROPERTY(EditAnywhere, Category = "Rain")
	float Radius = 2200.f;

	UPROPERTY(EditAnywhere, Category = "Rain")
	float Height = 1400.f;

	/** Ближе этого к камере по горизонтали струек нет (не «сквозь салон машины»), см. */
	UPROPERTY(EditAnywhere, Category = "Rain")
	float InnerRadius = 160.f;

	/** Скорость падения, см/с, и снос ветром (X/Y), см/с. */
	UPROPERTY(EditAnywhere, Category = "Rain")
	float FallSpeed = 950.f;

	UPROPERTY(EditAnywhere, Category = "Rain")
	FVector2D Wind = FVector2D(60.f, 25.f);

	/** Длина и толщина струйки, см. */
	UPROPERTY(EditAnywhere, Category = "Rain")
	float StreakLength = 55.f;

	UPROPERTY(EditAnywhere, Category = "Rain")
	float StreakWidth = 0.35f;

	/** Прозрачность струйки (0..1) — «еле видно». */
	UPROPERTY(EditAnywhere, Category = "Rain", meta = (ClampMin = "0", ClampMax = "1"))
	float Opacity = 0.1f;

	/** Звук дождя (слот Sounds) и громкость; под крышей — тише и глуше. */
	UPROPERTY(EditAnywhere, Category = "Rain")
	FName SoundSlot = TEXT("AmbientRain");

	UPROPERTY(EditAnywhere, Category = "Rain")
	float Volume = 0.3f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Drops;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Sound;

	/** Уличный фон уровня (AmbientSound с тегом Outdoor): глохнет вместе с дождём. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> OutdoorSounds;
	TArray<float> OutdoorBaseVolumes;

	/** Туман уровня: в здании плавно убирается (у каждой машины своё — это только вид). */
	UPROPERTY(Transient)
	TObjectPtr<class UExponentialHeightFogComponent> Fog;
	float FogBaseDensity = 0.f;
	bool bFogVolumetric = false;
	float FogAlpha = 1.f;

	/** Громкость улицы 0..1: в здании/машине плавно уходит в 0, звуки останавливаются, снаружи — снова запускаются. */
	float StreetAlpha = 1.f;
	void ApplyStreetVolume();

	/** Объёмы «помещение» (акторы с тегом Indoor: здание, пристройка, гараж). Камера внутри — мы в здании.
	    Нет таких на карте — по старинке: крыша над головой и стены вокруг. */
	TArray<FBox> IndoorBoxes;

	TArray<FVector> Positions;    // мировые позиции струек
	TArray<FTransform> Transforms; // буфер для обновления инстансов
	bool bIndoors = false;
	bool bMuted = false;
	float IndoorCheckTimer = 0.f;
	FRandomStream Random;
};
