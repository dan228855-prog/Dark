// DarcPowerInlet.cpp
#include "DarcPowerInlet.h"
#include "DarcGenerator.h"
#include "DarcPowerSubsystem.h"
#include "DarcWorldMemorySubsystem.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ADarcPowerInlet::ADarcPowerInlet()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADarcPowerInlet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADarcPowerInlet, ConnectedGenerator);
}

bool ADarcPowerInlet::IsInCableRange(const ADarcGenerator* Generator) const
{
	return Generator && FVector::Dist(Generator->GetActorLocation(), GetActorLocation()) <= CableLength;
}

bool ADarcPowerInlet::IsSupplying() const
{
	return ConnectedGenerator && ConnectedGenerator->bRunning && IsInCableRange(ConnectedGenerator);
}

void ADarcPowerInlet::ValidateConnection()
{
	if (HasAuthority() && ConnectedGenerator && !IsInCableRange(ConnectedGenerator))
	{
		ConnectedGenerator = nullptr; // кабель выдернулся
		OnRep_Connected();
	}
}

bool ADarcPowerInlet::CanInteract_Implementation(AActor* Interactor) const
{
	return true;
}

void ADarcPowerInlet::OnInteract_Implementation(AActor* Interactor)
{
	// Только сервер.
	if (ConnectedGenerator)
	{
		ConnectedGenerator = nullptr;
	}
	else
	{
		// Ближайший генератор, до которого достаёт кабель.
		ADarcGenerator* Best = nullptr;
		float BestDist = TNumericLimits<float>::Max();
		for (TActorIterator<ADarcGenerator> It(GetWorld()); It; ++It)
		{
			const float Dist = FVector::Dist(It->GetActorLocation(), GetActorLocation());
			if (Dist <= CableLength && Dist < BestDist)
			{
				Best = *It;
				BestDist = Dist;
			}
		}
		ConnectedGenerator = Best;
	}

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(CircuitId, ConnectedGenerator ? TEXT("GeneratorConnected") : TEXT("GeneratorDisconnected"), Interactor);
	}

	OnRep_Connected();
	if (UDarcPowerSubsystem* Power = UDarcPowerSubsystem::GetPower(this))
	{
		Power->Recompute();
	}
}

void ADarcPowerInlet::OnRep_Connected()
{
	OnConnectionChanged(ConnectedGenerator);
}

FText ADarcPowerInlet::GetInteractionPrompt_Implementation() const
{
	return ConnectedGenerator ? PromptDisconnect : PromptConnect;
}
