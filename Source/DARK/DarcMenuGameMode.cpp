// DarcMenuGameMode.cpp
#include "DarcMenuGameMode.h"

ADarcMenuPlayerController::ADarcMenuPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
}

void ADarcMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
}

ADarcMenuGameMode::ADarcMenuGameMode()
{
	// В меню ходить некому: персонаж не нужен, камера стоит на месте.
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ADarcMenuPlayerController::StaticClass();
}
