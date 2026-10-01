// DarcGenerator.cpp
#include "DarcGenerator.h"
#include "DarcPowerSubsystem.h"
#include "DarcWorldMemorySubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ADarcGenerator::ADarcGenerator()
{
	bReplicates = true;
	SetReplicateMovement(true); // физику считает сервер, клиенты получают позицию
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetSimulatePhysics(true);
	Body->SetMassOverrideInKg(NAME_None, 120.f, true); // тяжёлый: одному тяжело, вдвоём нормально
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	RootComponent = Body;
}

void ADarcGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADarcGenerator, bRunning);
}

bool ADarcGenerator::CanInteract_Implementation(AActor* Interactor) const
{
	return true;
}

void ADarcGenerator::OnInteract_Implementation(AActor* Interactor)
{
	SetRunning(!bRunning);

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(GetFName(), bRunning ? TEXT("GeneratorStart") : TEXT("GeneratorStop"), Interactor);
	}
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

FText ADarcGenerator::GetInteractionPrompt_Implementation() const
{
	return bRunning ? PromptStop : PromptStart;
}
