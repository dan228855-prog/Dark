// DarcDataTransferStation.cpp
#include "DarcDataTransferStation.h"
#include "DarcAssetSettings.h"
#include "Components/StaticMeshComponent.h"
#include "DarcItemSlot.h"
#include "DarcPowerConsumerComponent.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADarcDataTransferStation::ADarcDataTransferStation()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Power = CreateDefaultSubobject<UDarcPowerConsumerComponent>(TEXT("Power"));
	Power->CircuitId = TEXT("Server");
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ADarcDataTransferStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcDataTransferStation, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME(ADarcDataTransferStation, State);
	DOREPLIFETIME(ADarcDataTransferStation, ProgressPercent);
}

void ADarcDataTransferStation::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual); // модель — у каждой машины сама

	if (HasAuthority())
	{
		Power->OnPowerChanged.AddDynamic(this, &ADarcDataTransferStation::HandlePowerChanged);
		if (InterfaceSlot)
		{
			InterfaceSlot->OnSlotChanged.AddDynamic(this, &ADarcDataTransferStation::HandleSlotChanged);
		}
		if (DriveSlot)
		{
			DriveSlot->bAllowRemove = false; // носитель забирают только после передачи
		}
		UpdateIdleState();
	}
	OnRep_Transfer();
}

void ADarcDataTransferStation::HandlePowerChanged(bool bPowered, EDarcPowerSource Source)
{
	if (!bPowered && (State == EDarcTransferState::Transferring || State == EDarcTransferState::Checking))
	{
		// Питание пропало посреди передачи — встаём, прогресс сохраняется.
		GetWorldTimerManager().ClearTimer(TransferTimer);
		GetWorldTimerManager().ClearTimer(PauseTimer);
		if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
		{
			Memory->RecordInteraction(GetFName(), TEXT("TransferInterrupted"), nullptr);
		}
	}
	UpdateIdleState();
}

void ADarcDataTransferStation::HandleSlotChanged(ACarryableItem* Item)
{
	UpdateIdleState();
}

void ADarcDataTransferStation::UpdateIdleState()
{
	if (!HasAuthority() || State == EDarcTransferState::Complete)
	{
		return;
	}

	if (!Power->IsPowered())
	{
		SetState(EDarcTransferState::NoPower);
		return;
	}
	if (!InterfaceSlot || !InterfaceSlot->HasItem() || !DriveSlot || !DriveSlot->HasItem())
	{
		SetState(EDarcTransferState::StorageNotReady);
		return;
	}

	// Питание вернули после обрыва — продолжаем сами, без повторного запуска.
	if (bWasStarted && !GetWorldTimerManager().IsTimerActive(TransferTimer) && !GetWorldTimerManager().IsTimerActive(PauseTimer))
	{
		SetState(EDarcTransferState::Transferring);
		GetWorldTimerManager().SetTimer(TransferTimer, this, &ADarcDataTransferStation::TickTransfer, TickInterval, true);
		return;
	}

	if (!bWasStarted)
	{
		SetState(EDarcTransferState::Ready);
	}
}

void ADarcDataTransferStation::SetState(EDarcTransferState NewState)
{
	if (State != NewState)
	{
		State = NewState;
		OnRep_Transfer();
	}
}

bool ADarcDataTransferStation::CanInteract_Implementation(AActor* Interactor) const
{
	return State == EDarcTransferState::Ready;
}

void ADarcDataTransferStation::OnInteract_Implementation(AActor* Interactor)
{
	// Сервер. Запуск передачи.
	if (State != EDarcTransferState::Ready)
	{
		return;
	}

	bWasStarted = true;
	Progress = 0.f;
	ProgressPercent = 0;
	// Модуль во время передачи вынуть нельзя.
	if (InterfaceSlot)
	{
		InterfaceSlot->bAllowRemove = false;
	}
	SetState(EDarcTransferState::Transferring);
	GetWorldTimerManager().SetTimer(TransferTimer, this, &ADarcDataTransferStation::TickTransfer, TickInterval, true);

	if (!AnomalyFlickerCircuit.IsNone() && !bAnomalyDone)
	{
		GetWorldTimerManager().SetTimer(AnomalyTimer, this, &ADarcDataTransferStation::FireAnomaly, FMath::Max(0.1f, AnomalyDelay), false);
	}

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(GetFName(), TEXT("TransferStarted"), Interactor);
	}
	OnTransferStartedServer(Interactor);
}

void ADarcDataTransferStation::TickTransfer()
{
	Progress = FMath::Min(100.f, Progress + 100.f * TickInterval / TransferSeconds);

	if (!bPauseDone && Progress >= PauseAtPercent)
	{
		// «Задумался» на 94%: CHECKING DATA...
		Progress = PauseAtPercent;
		bPauseDone = true;
		GetWorldTimerManager().ClearTimer(TransferTimer);
		SetState(EDarcTransferState::Checking);
		GetWorldTimerManager().SetTimer(PauseTimer, this, &ADarcDataTransferStation::FinishPause, FMath::Max(0.1f, PauseSeconds), false);
	}

	const uint8 NewPercent = static_cast<uint8>(FMath::FloorToInt(Progress));
	if (NewPercent != ProgressPercent)
	{
		ProgressPercent = NewPercent;
		OnRep_Transfer();
	}

	if (Progress >= 100.f)
	{
		GetWorldTimerManager().ClearTimer(TransferTimer);
		GetWorldTimerManager().ClearTimer(AnomalyTimer);
		SetState(EDarcTransferState::Complete);

		if (DriveSlot)
		{
			DriveSlot->bAllowRemove = true;
		}
		if (InterfaceSlot)
		{
			InterfaceSlot->bAllowRemove = true;
		}
		if (!TaskIdOnComplete.IsNone())
		{
			if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
			{
				Tasks->CompleteTask(TaskIdOnComplete, nullptr);
			}
		}
		OnTransferCompletedServer();
	}
}

void ADarcDataTransferStation::FinishPause()
{
	// Пауза закончилась, но питание могло пропасть именно в ней — тогда ждём UpdateIdleState.
	if (!Power->IsPowered())
	{
		SetState(EDarcTransferState::NoPower);
		return;
	}
	SetState(EDarcTransferState::Transferring);
	GetWorldTimerManager().SetTimer(TransferTimer, this, &ADarcDataTransferStation::TickTransfer, TickInterval, true);
}

void ADarcDataTransferStation::FireAnomaly()
{
	bAnomalyDone = true;
	if (UDarcPowerSubsystem* PowerSystem = UDarcPowerSubsystem::GetPower(this))
	{
		PowerSystem->FlickerCircuit(AnomalyFlickerCircuit, 2.5f);
	}
}

void ADarcDataTransferStation::OnRep_Transfer()
{
	// Звук смены состояния (у каждой машины один раз на переход, не на каждый процент).
	if (State != LastShownState && HasActorBegunPlay())
	{
		if (State == EDarcTransferState::Transferring) UDarcAssetSettings::PlaySound(this, TEXT("TransferStart"), GetActorLocation());
		if (State == EDarcTransferState::Checking)     UDarcAssetSettings::PlaySound(this, TEXT("TransferChecking"), GetActorLocation());
		if (State == EDarcTransferState::Complete)     UDarcAssetSettings::PlaySound(this, TEXT("TransferComplete"), GetActorLocation());
	}
	LastShownState = State;
	OnTransferStateChanged(State, ProgressPercent);
}

FText ADarcDataTransferStation::GetInteractionPrompt_Implementation() const
{
	return PromptStart;
}
