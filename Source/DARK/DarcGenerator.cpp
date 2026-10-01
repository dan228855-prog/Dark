// DarcGenerator.cpp
#include "DarcGenerator.h"
#include "DarcAssetSettings.h"
#include "DarcPowerSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Components/AudioComponent.h"

ADarcGenerator::ADarcGenerator()
{
	// Хрупкого кабеля у генератора нет — кабель к вводу проверяется по длине в ADarcPowerInlet.
	bHasFragileCable = false;
	RequiredCarriers = 2;
}

void ADarcGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADarcGenerator, bRunning);
}

void ADarcGenerator::SetRunning(bool bNewRunning)
{
	if (!HasAuthority() || bRunning == bNewRunning)
	{
		return;
	}

	bRunning = bNewRunning;
	OnRep_Running();

	if (UDarcPowerSubsystem* Power = UDarcPowerSubsystem::GetPower(this))
	{
		Power->Recompute();
	}
}

void ADarcGenerator::OnRep_Running()
{
	// Звук у каждой машины: запуск/остановка + зацикленный гул, пока работает.
	UDarcAssetSettings::PlaySound(this, bRunning ? TEXT("GeneratorStart") : TEXT("GeneratorStop"), GetActorLocation());
	if (bRunning && !RunningLoop)
	{
		RunningLoop = UDarcAssetSettings::PlayLoopAttached(TEXT("GeneratorLoop"), GetRootComponent());
	}
	else if (!bRunning && RunningLoop)
	{
		RunningLoop->Stop();
		RunningLoop = nullptr;
	}
	OnRunningChanged(bRunning);
}
