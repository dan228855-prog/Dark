// DarcRemovableCover.cpp
#include "DarcRemovableCover.h"
#include "DarcNoiseSubsystem.h"
#include "DarcTool.h"
#include "DarcGameState.h"
#include "DarcGameplayLibrary.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ADarcRemovableCover::ADarcRemovableCover()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ADarcRemovableCover::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcRemovableCover, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME(ADarcRemovableCover, bRemoved);
	DOREPLIFETIME(ADarcRemovableCover, ScrewsLeft);
}

void ADarcRemovableCover::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual);
	if (HasAuthority())
	{
		ScrewsLeft = FMath::Max(Screws, 1);
	}
	OnRep_Removed();
}

void ADarcRemovableCover::OnRep_Removed()
{
	// Снятая крышка исчезает: проход свободен, взгляд проходит насквозь.
	SetActorHiddenInGame(bRemoved);
	SetActorEnableCollision(!bRemoved);
}

bool ADarcRemovableCover::CanInteract_Implementation(AActor* Interactor) const
{
	return !bRemoved;
}

void ADarcRemovableCover::OnInteract_Implementation(AActor* Interactor)
{
	// Только сервер.
	if (bRemoved)
	{
		return;
	}
	const EDarcToolType Tool = ADarcTool::GetHeldTool(Interactor);
	if (Tool == EDarcToolType::Crowbar)
	{
		Remove(Interactor, true);
		return;
	}
	if (Tool == EDarcToolType::Wrench)
	{
		if (--ScrewsLeft > 0)
		{
			Multicast_Sound(TEXT("Unscrew"), 0.35f);
			return;
		}
		Remove(Interactor, false);
		return;
	}
	if (ADarcGameState* GS = GetWorld()->GetGameState<ADarcGameState>())
	{
		GS->Say(FText::GetEmpty(), UDarcGameplayLibrary::UIText(TEXT("Cover_NeedTool")), 3.5f, GetActorLocation(), 600.f);
	}
}

void ADarcRemovableCover::Remove(AActor* ByActor, bool bLoud)
{
	bRemoved = true;
	OnRep_Removed();
	Multicast_Sound(bLoud ? FName(TEXT("CoverPry")) : FName(TEXT("Unscrew")), bLoud ? 1.f : 0.4f);
	if (bLoud)
	{
		if (APawn* Pawn = Cast<APawn>(ByActor))
		{
			MakeNoise(1.f, Pawn, GetActorLocation());
		}
	}
	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(MemoryId.IsNone() ? GetFName() : MemoryId, bLoud ? TEXT("CoverPried") : TEXT("CoverUnscrewed"), ByActor);
	}
	if (!TaskIdOnRemove.IsNone())
	{
		if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
		{
			Tasks->CompleteTask(TaskIdOnRemove, ByActor);
		}
	}
}

void ADarcRemovableCover::Multicast_Sound_Implementation(FName Slot, float Volume)
{
	UDarcAssetSettings::PlaySound(this, Slot, GetActorLocation(), Volume);
	UDarcNoiseSubsystem::Report(this, GetActorLocation(), 0.6f * Volume);
}

FText ADarcRemovableCover::GetInteractionPrompt_Implementation() const
{
	if (ADarcTool::LocalPlayerHolds(this, EDarcToolType::Crowbar))
	{
		return UDarcGameplayLibrary::UIText(TEXT("Cover_Pry"));
	}
	if (ADarcTool::LocalPlayerHolds(this, EDarcToolType::Wrench))
	{
		return FText::Format(UDarcGameplayLibrary::UIText(TEXT("Cover_Unscrew")), ScrewsLeft);
	}
	return UDarcGameplayLibrary::UIText(TEXT("Cover_Screwed"));
}
