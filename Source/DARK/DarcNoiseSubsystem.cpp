// DarcNoiseSubsystem.cpp
#include "DarcNoiseSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void UDarcNoiseSubsystem::Report(const UObject* WorldContextObject, const FVector& Location, float Loudness)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	UDarcNoiseSubsystem* Self = World ? World->GetSubsystem<UDarcNoiseSubsystem>() : nullptr;
	if (!Self || Loudness <= 0.f)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	Self->Noises.RemoveAll([Now](const FNoise& N) { return Now - N.Time > 60.f; });
	if (Self->Noises.Num() > 64)
	{
		Self->Noises.RemoveAt(0);
	}
	Self->Noises.Add({ Location, FMath::Clamp(Loudness, 0.f, 1.f), Now });
}

bool UDarcNoiseSubsystem::FindRecent(const FVector& Near, float Radius, float MaxAge, FVector& OutLocation, float& OutLoudness) const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	OutLoudness = 0.f;
	for (const FNoise& N : Noises)
	{
		const float Age = Now - N.Time;
		if (Age > MaxAge || FVector::DistSquared(N.Location, Near) > Radius * Radius)
		{
			continue;
		}
		const float Effective = N.Loudness * (1.f - Age / MaxAge);
		if (Effective > OutLoudness)
		{
			OutLoudness = Effective;
			OutLocation = N.Location;
		}
	}
	return OutLoudness > 0.f;
}
