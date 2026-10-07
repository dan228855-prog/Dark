// DarcNoiseSubsystem.h
// Шум уровня: падение и бросок предметов, взлом ломом, сорванная решётка, сгоревший предохранитель,
// бег. Каждая машина помнит недавние шумы сама (о них сообщают те же мультикасты, что играют звук),
// поэтому «тень» — она у каждого игрока своя — тянется к шуму и появляется чаще, если вокруг шумят.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DarcNoiseSubsystem.generated.h"

UCLASS()
class DARK_API UDarcNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Сообщить о шуме: громкость 0..1 (1 — лом по двери). Вызывать у каждой машины (из мультикаста/локально). */
	static void Report(const UObject* WorldContextObject, const FVector& Location, float Loudness);

	/** Самый громкий шум за последние MaxAge секунд в радиусе Radius от точки (с поправкой на давность). */
	bool FindRecent(const FVector& Near, float Radius, float MaxAge, FVector& OutLocation, float& OutLoudness) const;

private:
	struct FNoise
	{
		FVector Location;
		float Loudness;
		float Time;
	};
	TArray<FNoise> Noises;
};
