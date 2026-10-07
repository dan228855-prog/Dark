// DarcGenerator.cpp
#include "DarcGenerator.h"
#include "DarcAssetSettings.h"
#include "DarcPowerSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Components/AudioComponent.h"
#include "DarcGameState.h"
#include "DarcGameplayLibrary.h"
#include "DarcWorldMemorySubsystem.h"

ADarcGenerator::ADarcGenerator()
{
	// Хрупкого кабеля у генератора нет — кабель к вводу проверяется по длине в ADarcPowerInlet.
	bHasFragileCable = false;
	MassKg = 70.f; // один в коопе не сдвинет, вдвоём — потащат, в соло — медленно волоком
}

void ADarcGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADarcGenerator, bRunning);
	DOREPLIFETIME_CONDITION(ADarcGenerator, Capacity, COND_InitialOnly);
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

void ADarcGenerator::TripOverload()
{
	if (!HasAuthority() || !bRunning)
	{
		return;
	}
	Multicast_Overload();
	if (ADarcGameState* GS = GetWorld()->GetGameState<ADarcGameState>())
	{
		GS->Say(FText::GetEmpty(), UDarcGameplayLibrary::UIText(TEXT("Generator_Overload")), 5.f, GetActorLocation(), 1500.f);
	}
	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(GetFName(), TEXT("GeneratorOverload"), nullptr);
	}
	SetRunning(false);
}

void ADarcGenerator::Multicast_Overload_Implementation()
{
	UDarcAssetSettings::PlaySound(this, TEXT("FuseBlow"), GetActorLocation());
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
