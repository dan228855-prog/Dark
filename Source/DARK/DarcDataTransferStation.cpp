// DarcDataTransferStation.cpp
#include "DarcDataTransferStation.h"
#include "DarcAssetSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DarcGameplayLibrary.h"
#include "Engine/StaticMesh.h"
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
	// Корпус — не кнопка: E работает только по «Пуску» у разъёма носителя.
	Visual->ComponentTags.Add(TEXT("NoInteract"));

	auto MakeShape = [this](const TCHAR* Name, const TCHAR* Shape)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(RootComponent);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCastShadow(false);
		Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape)));
		return Part;
	};
	// Кнопку ловит только взгляд (Visibility) — ни с чем не сталкивается.
	StartButton = MakeShape(TEXT("StartButton"), TEXT("Cube"));
	StartButton->SetCollisionProfileName(TEXT("NoCollision"));
	StartButton->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StartButton->SetCollisionResponseToAllChannels(ECR_Ignore);
	StartButton->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ScreenPanel = MakeShape(TEXT("ScreenPanel"), TEXT("Cube"));
	ScreenPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	auto MakeText = [this](const TCHAR* Name)
	{
		UTextRenderComponent* Text = CreateDefaultSubobject<UTextRenderComponent>(Name);
		Text->SetupAttachment(RootComponent);
		Text->SetMobility(EComponentMobility::Movable);
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Text->SetCastShadow(false);
		return Text;
	};
	ScreenTitle = MakeText(TEXT("ScreenTitle"));
	ScreenDetail = MakeText(TEXT("ScreenDetail"));
}

void ADarcDataTransferStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcDataTransferStation, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ADarcDataTransferStation, InterfaceSlot, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ADarcDataTransferStation, DriveSlot, COND_InitialOnly);
	DOREPLIFETIME(ADarcDataTransferStation, State);
	DOREPLIFETIME(ADarcDataTransferStation, ProgressPercent);
}

void ADarcDataTransferStation::OnRep_DriveSlot()
{
	// Разъём носителя пришёл по сети позже станции — переставить экран и кнопку к нему.
	LayoutControls();
}

void ADarcDataTransferStation::LayoutControls()
{
	// Лицевая сторона станции — её локальная +X. Экран и кнопка — в одной вертикали с разъёмом
	// носителя: экран над ним (на уровне глаз), «Пуск» — сразу под ним. Раньше «запустить»
	// срабатывало с любого бока корпуса, и было непонятно, где управление.
	const FTransform ToLocal = GetActorTransform().Inverse();
	const FBox Box = Visual->Bounds.GetBox().TransformBy(ToLocal);
	const float Front = Box.Max.X;
	FVector Anchor(Front, Box.GetCenter().Y, 110.f);
	if (DriveSlot)
	{
		const FVector SlotLocal = ToLocal.TransformPosition(DriveSlot->GetActorLocation());
		Anchor = FVector(FMath::Max(Front, SlotLocal.X), SlotLocal.Y, SlotLocal.Z);
	}

	StartButton->SetRelativeLocation(FVector(Anchor.X + 1.5f, Anchor.Y, Anchor.Z - 24.f));
	StartButton->SetRelativeScale3D(FVector(0.03f, 0.09f, 0.09f)); // 3 × 9 × 9 см

	ScreenPanel->SetRelativeLocation(FVector(Anchor.X + 1.f, Anchor.Y, Anchor.Z + 48.f));
	ScreenPanel->SetRelativeScale3D(FVector(0.02f, 0.56f, 0.30f)); // экран 56 × 30 см

	// Текст «смотрит» вдоль своей +X — то есть от лицевой стороны станции к игроку.
	ScreenTitle->SetRelativeLocation(FVector(Anchor.X + 2.5f, Anchor.Y, Anchor.Z + 56.f));
	ScreenTitle->SetWorldSize(3.6f);
	ScreenDetail->SetRelativeLocation(FVector(Anchor.X + 2.5f, Anchor.Y, Anchor.Z + 42.f));
	ScreenDetail->SetWorldSize(6.f);
}

void ADarcDataTransferStation::UpdateScreen()
{
	// Нет питания — экран тёмный, кнопка не светится.
	const bool bOn = State != EDarcTransferState::NoPower;
	ScreenTitle->SetVisibility(bOn);
	ScreenDetail->SetVisibility(bOn);
	ScreenPanel->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(this, bOn ? FLinearColor(0.01f, 0.025f, 0.015f) : FLinearColor(0.01f, 0.01f, 0.01f), bOn ? 0.6f : 0.f));

	FName Title = TEXT("Screen_NoPower");
	FText Detail = FText::GetEmpty();
	FLinearColor ButtonColor(0.08f, 0.08f, 0.08f);
	float ButtonGlow = 0.f;
	switch (State)
	{
	case EDarcTransferState::StorageNotReady:
		Title = TEXT("Screen_StorageNotReady");
		Detail = UDarcGameplayLibrary::UIText(TEXT("Screen_InsertModule"));
		ButtonColor = FLinearColor(0.9f, 0.08f, 0.05f);
		ButtonGlow = 2.f;
		break;
	case EDarcTransferState::Ready:
		Title = TEXT("Screen_TransferReady");
		Detail = UDarcGameplayLibrary::UIText(TEXT("Screen_PressStart"));
		ButtonColor = FLinearColor(0.1f, 1.f, 0.2f);
		ButtonGlow = 8.f;
		break;
	case EDarcTransferState::Transferring:
		Title = TEXT("Screen_Transferring");
		Detail = FText::Format(UDarcGameplayLibrary::UIText(TEXT("Screen_PercentFormat")), FText::AsNumber(static_cast<int32>(ProgressPercent)));
		ButtonColor = FLinearColor(1.f, 0.6f, 0.05f);
		ButtonGlow = 4.f;
		break;
	case EDarcTransferState::Checking:
		Title = TEXT("Screen_CheckingData");
		Detail = FText::Format(UDarcGameplayLibrary::UIText(TEXT("Screen_PercentFormat")), FText::AsNumber(static_cast<int32>(ProgressPercent)));
		ButtonColor = FLinearColor(1.f, 0.6f, 0.05f);
		ButtonGlow = 4.f;
		break;
	case EDarcTransferState::Complete:
		Title = TEXT("Screen_TransferComplete");
		Detail = UDarcGameplayLibrary::UIText(TEXT("Screen_RemoveDrive"));
		ButtonColor = FLinearColor(0.2f, 0.6f, 1.f);
		ButtonGlow = 4.f;
		break;
	default:
		break;
	}
	ScreenTitle->SetText(UDarcGameplayLibrary::UIText(Title));
	ScreenDetail->SetText(Detail);
	StartButton->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(this, ButtonColor, ButtonGlow));
}

void ADarcDataTransferStation::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual); // модель — у каждой машины сама

	// Текст экрана светится сам (неосвещаемый материал) — читается и в тёмной серверной.
	for (UTextRenderComponent* Text : { ScreenTitle.Get(), ScreenDetail.Get() })
	{
		Text->SetTextMaterial(UDarcAssetSettings::GetUnlitTextMaterial());
		Text->SetTextRenderColor(FColor(120, 255, 140));
	}
	LayoutControls();

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
	// Кнопку видно всегда: подсказка объясняет, чего не хватает (питание, модуль) или что делать дальше.
	return true;
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
	UpdateScreen();
	OnTransferStateChanged(State, ProgressPercent);
}

FText ADarcDataTransferStation::GetInteractionPrompt_Implementation() const
{
	switch (State)
	{
	case EDarcTransferState::NoPower:         return PromptNoPower;
	case EDarcTransferState::StorageNotReady: return PromptNeedModule;
	case EDarcTransferState::Ready:           return PromptStart;
	case EDarcTransferState::Complete:        return PromptTakeDrive;
	default:                                  return PromptInProgress;
	}
}
