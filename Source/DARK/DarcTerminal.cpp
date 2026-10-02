// DarcTerminal.cpp
#include "DarcTerminal.h"
#include "DarcAssetSettings.h"
#include "Components/StaticMeshComponent.h"
#include "DarcPowerConsumerComponent.h"
#include "DarcPlayerState.h"
#include "InteractableDoor.h"
#include "DarcPlayerController.h"
#include "DarcGameplayLibrary.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADarcTerminal::ADarcTerminal()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Power = CreateDefaultSubobject<UDarcPowerConsumerComponent>(TEXT("Power"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ADarcTerminal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcTerminal, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME(ADarcTerminal, State);
	DOREPLIFETIME(ADarcTerminal, CurrentUser);
}

void ADarcTerminal::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual); // модель — у каждой машины сама

	if (HasAuthority())
	{
		bUnlocked = bStartsUnlocked;
		Power->OnPowerChanged.AddDynamic(this, &ADarcTerminal::HandlePowerChanged);
		HandlePowerChanged(Power->IsPowered(), Power->GetSource());
		GetWorldTimerManager().SetTimer(UserCheckTimer, this, &ADarcTerminal::CheckUserStillNear, 0.5f, true);
	}
	OnRep_State();
}

void ADarcTerminal::HandlePowerChanged(bool bPowered, EDarcPowerSource Source)
{
	if (!HasAuthority())
	{
		return;
	}

	// Без контура терминал работает всегда (автономный).
	const bool bHasPower = Power->CircuitId.IsNone() || bPowered;
	if (!bHasPower)
	{
		// Экран погас — пользователь «выброшен», введённое не сохраняется.
		ServerReleaseUser(CurrentUser);
		SetState(EDarcTerminalState::NoPower);
		return;
	}
	SetState(bUnlocked ? EDarcTerminalState::Unlocked : EDarcTerminalState::Locked);
}

void ADarcTerminal::SetState(EDarcTerminalState NewState)
{
	if (State != NewState)
	{
		State = NewState;
		OnRep_State();
	}
}

FString ADarcTerminal::GetExpectedCode() const
{
	return FixedCode.IsEmpty() ? UDarcGameplayLibrary::GetMissionCode(this, CodeKey, 4) : FixedCode;
}

bool ADarcTerminal::IsValidUser(const APlayerState* Player) const
{
	if (!Player || Player != CurrentUser || State == EDarcTerminalState::NoPower)
	{
		return false;
	}
	// Погибший («дух») терминалом не пользуется.
	if (const ADarcPlayerState* DarcPS = Cast<ADarcPlayerState>(Player); DarcPS && !DarcPS->bIsAlive)
	{
		return false;
	}
	const APawn* Pawn = Player->GetPawn();
	return Pawn && FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) <= UseRange;
}

void ADarcTerminal::CheckUserStillNear()
{
	if (CurrentUser && !IsValidUser(CurrentUser))
	{
		ServerReleaseUser(CurrentUser);
	}
}

bool ADarcTerminal::CanInteract_Implementation(AActor* Interactor) const
{
	if (State == EDarcTerminalState::NoPower)
	{
		return false;
	}
	const APawn* Pawn = Cast<APawn>(Interactor);
	const APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
	return !CurrentUser || CurrentUser == PS;
}

void ADarcTerminal::OnInteract_Implementation(AActor* Interactor)
{
	// Сервер. Занять терминал (или освободить, если это тот же игрок).
	const APawn* Pawn = Cast<APawn>(Interactor);
	APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
	if (!PS || State == EDarcTerminalState::NoPower)
	{
		return;
	}

	if (CurrentUser == PS)
	{
		ServerReleaseUser(PS);
		return;
	}
	if (!CurrentUser)
	{
		CurrentUser = PS;
		OnRep_User();
	}
}

void ADarcTerminal::ServerReleaseUser(APlayerState* FromPlayer)
{
	if (HasAuthority() && CurrentUser && CurrentUser == FromPlayer)
	{
		CurrentUser = nullptr;
		OnRep_User();
	}
}

void ADarcTerminal::ServerHandleInput(APlayerState* FromPlayer, const FString& Input)
{
	if (!HasAuthority() || !IsValidUser(FromPlayer))
	{
		return;
	}

	// Ограничение длины — защита от мусора с клиента.
	const FString Clean = Input.Left(64).TrimStartAndEnd();
	UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this);

	if (State == EDarcTerminalState::Locked)
	{
		const bool bCorrect = Clean == GetExpectedCode();
		if (Memory)
		{
			Memory->RecordInteraction(GetFName(), bCorrect ? TEXT("CodeAccepted") : TEXT("CodeRejected"), FromPlayer);
		}
		Multicast_InputResult(bCorrect, Clean);

		if (bCorrect)
		{
			ForceUnlock(FromPlayer->GetPawn());
		}
		return;
	}

	if (State == EDarcTerminalState::Unlocked)
	{
		bool bAccepted = false;
		// Команды без учёта регистра: «copy», «COPY», «Copy» — одно и то же.
		const FName* TaskId = CommandTasks.Find(Clean);
		if (!TaskId)
		{
			TaskId = CommandTasks.Find(Clean.ToUpper());
		}
		if (TaskId)
		{
			if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
			{
				Tasks->CompleteTask(*TaskId, FromPlayer->GetPawn());
			}
			bAccepted = true;
		}
		else
		{
			bAccepted = HandleCustomCommand(FromPlayer, Clean);
		}

		if (Memory)
		{
			Memory->RecordInteraction(GetFName(), FName(*(TEXT("Cmd_") + Clean.Left(24))), FromPlayer);
		}
		Multicast_InputResult(bAccepted, Clean);
	}
}

void ADarcTerminal::ForceUnlock(AActor* ByActor)
{
	if (!HasAuthority() || bUnlocked)
	{
		return;
	}

	bUnlocked = true;
	if (DoorToUnlock)
	{
		DoorToUnlock->SetLocked(false);
	}
	if (State != EDarcTerminalState::NoPower)
	{
		SetState(EDarcTerminalState::Unlocked);
	}

	if (!TaskIdOnUnlock.IsNone())
	{
		if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
		{
			Tasks->CompleteTask(TaskIdOnUnlock, ByActor);
		}
	}
}

bool ADarcTerminal::HandleCustomCommand_Implementation(APlayerState* FromPlayer, const FString& Command)
{
	return false; // по умолчанию неизвестная команда — отказ; сюжетные команды добавляет Blueprint
}

bool ADarcTerminal::IsLocalPlayerUser() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC && PC->IsLocalController() && CurrentUser && PC->PlayerState == CurrentUser;
}

TArray<FText> ADarcTerminal::GetScreenLines() const
{
	TArray<FText> Result;
	if (ScrambledWords.Num() > 0)
	{
		// Слова перемешаны по сиду выезда (у всех игроков одинаково, не меняется при обновлении
		// экрана) и показаны не текстом, а двоичным кодом UTF-8: прочитать можно, только
		// расшифровав байты. Сама фраза на экране не появляется.
		const UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this);
		FRandomStream Stream(Tasks ? Tasks->GetMissionSeed() ^ 0x5EED : 0x5EED);
		TArray<FText> Words = ScrambledWords;
		for (int32 i = Words.Num() - 1; i > 0; --i)
		{
			Words.Swap(i, Stream.RandRange(0, i));
		}
		Result.Add(FText::FromString(UDarcGameplayLibrary::EncodeBinary(FText::Join(FText::FromString(TEXT(" ")), Words).ToString(), 6)));
		Result.Add(FText::GetEmpty());
	}
	Result.Append(ScreenLines);
	return Result;
}

void ADarcTerminal::OnRep_State()
{
	if (bLocalWindowOpen)
	{
		if (ADarcPlayerController* PC = ADarcPlayerController::GetLocal(this))
		{
			PC->RefreshTerminalUI();
		}
	}
	OnTerminalStateChanged(State);
}

void ADarcTerminal::OnRep_User()
{
	// Окно ввода открывается только на машине того, кто сел за терминал.
	const bool bLocal = IsLocalPlayerUser();
	if (bLocal != bLocalWindowOpen)
	{
		if (ADarcPlayerController* PC = ADarcPlayerController::GetLocal(this))
		{
			if (bLocal)
			{
				PC->OpenTerminalUI(this);
			}
			else
			{
				PC->CloseTerminalUI(false);
			}
		}
		bLocalWindowOpen = bLocal;
	}
	OnUserChanged(CurrentUser, bLocal);
}

void ADarcTerminal::Multicast_InputResult_Implementation(bool bAccepted, const FString& EchoInput)
{
	if (bLocalWindowOpen)
	{
		if (ADarcPlayerController* PC = ADarcPlayerController::GetLocal(this))
		{
			PC->ShowTerminalResult(bAccepted, EchoInput);
		}
	}
	UDarcAssetSettings::PlaySound(this, bAccepted ? TEXT("AccessGranted") : TEXT("AccessDenied"), GetActorLocation());
	OnInputResult(bAccepted, EchoInput);
}

FText ADarcTerminal::GetInteractionPrompt_Implementation() const
{
	if (State == EDarcTerminalState::NoPower)
	{
		return PromptNoPower;
	}
	return CurrentUser && !IsLocalPlayerUser() ? PromptBusy : PromptUse;
}
