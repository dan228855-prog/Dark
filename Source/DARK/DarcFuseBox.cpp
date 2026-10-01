// DarcFuseBox.cpp
#include "DarcFuseBox.h"
#include "DarcAssetSettings.h"
#include "Components/StaticMeshComponent.h"
#include "DarcFuseItem.h"
#include "DarcPowerSubsystem.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ADarcFuseBox::ADarcFuseBox()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ADarcFuseBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcFuseBox, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME(ADarcFuseBox, bHasFuse);
	DOREPLIFETIME(ADarcFuseBox, bBreakerOn);
}

bool ADarcFuseBox::CanInteract_Implementation(AActor* Interactor) const
{
	return true; // рубильник можно дёргать всегда; без предохранителя он просто ничего не даст
}

void ADarcFuseBox::OnInteract_Implementation(AActor* Interactor)
{
	// Только сервер (вызов из InteractionComponent::Server_Interact).
	ADarcFuseItem* HeldFuse = Cast<ADarcFuseItem>(ACarryableItem::FindItemHeldBy(Interactor));

	if (!bHasFuse && HeldFuse)
	{
		// Предохранитель расходуется в любом случае: либо встал, либо сгорел.
		HeldFuse->ForceDrop();
		HeldFuse->Destroy();

		if (bBreakerOn)
		{
			HandleBlowFromMistake(Interactor);
			return;
		}

		bHasFuse = true;
		if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
		{
			Memory->RecordInteraction(CircuitId, TEXT("FuseInserted"), Interactor);
		}
		NotifyStateChanged();
		return;
	}

	bBreakerOn = !bBreakerOn;
	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(CircuitId, bBreakerOn ? TEXT("BreakerOn") : TEXT("BreakerOff"), Interactor);
	}
	NotifyStateChanged();
}

void ADarcFuseBox::SpiritToggleBreaker(AActor* Spirit)
{
	if (!HasAuthority())
	{
		return;
	}

	bBreakerOn = !bBreakerOn;
	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(CircuitId, bBreakerOn ? TEXT("SpiritBreakerOn") : TEXT("SpiritBreakerOff"), Spirit);
	}
	NotifyStateChanged();
}

void ADarcFuseBox::HandleBlowFromMistake(AActor* Interactor)
{
	Multicast_FuseBlown();

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(CircuitId, TEXT("FuseBlown"), Interactor);
		if (CollateralCircuits.Num() > 0)
		{
			// Последствие для кампании: в следующем брифинге — «плановое обслуживание сети».
			Memory->AddCampaignFact(TEXT("Slice.BlackoutHappened"));
		}
	}

	// Выбиваем соседние контуры: их предохранители сгорают тоже.
	for (TActorIterator<ADarcFuseBox> It(GetWorld()); It; ++It)
	{
		if (*It != this && CollateralCircuits.Contains(It->CircuitId))
		{
			It->BurnFuse();
		}
	}

	if (bAddTaskOnBlow)
	{
		if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
		{
			Tasks->AddEmergentTask(TaskOnBlow, NAME_None);
		}
	}

	NotifyStateChanged();
}

void ADarcFuseBox::BurnFuse()
{
	if (!HasAuthority() || !bHasFuse)
	{
		return;
	}

	bHasFuse = false;
	Multicast_FuseBlown();
	NotifyStateChanged();
}

void ADarcFuseBox::NotifyStateChanged()
{
	OnRep_State(); // на сервере OnRep сам не вызывается
	if (UDarcPowerSubsystem* Power = UDarcPowerSubsystem::GetPower(this))
	{
		Power->Recompute();
	}
}

void ADarcFuseBox::OnRep_State()
{
	if (HasActorBegunPlay())
	{
		UDarcAssetSettings::PlaySound(this, TEXT("BreakerSwitch"), GetActorLocation());
	}
	OnStateChanged();
}

void ADarcFuseBox::Multicast_FuseBlown_Implementation()
{
	UDarcAssetSettings::PlaySound(this, TEXT("FuseBlow"), GetActorLocation());
	OnFuseBlownFX();
}

FText ADarcFuseBox::GetInteractionPrompt_Implementation() const
{
	if (!bHasFuse)
	{
		// Подсказка не знает, что в руках у игрока, поэтому общая: «Вставить предохранитель».
		return PromptInsertFuse;
	}
	return bBreakerOn ? PromptBreakerOff : PromptBreakerOn;
}

void ADarcFuseBox::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual); // модель — у каждой машины сама
}
