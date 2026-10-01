// DarcGenerator.cpp
#include "DarcGenerator.h"
#include "DarcPowerSubsystem.h"
#include "Net/UnrealNetwork.h"

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
	OnRunningChanged(bRunning);
}
