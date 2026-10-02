// InteractionComponent.cpp
#include "InteractionComponent.h"
#include "DarcGameplayLibrary.h"
#include "Interactable.h"
#include "DarcTerminal.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 10 раз/сек достаточно для проверки прицела
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Чисто локальная косметика для HUD - на сервере/чужих клиентах не считаем.
	FocusedActor = FindInteractableInView();
}

AActor* UInteractionComponent::FindInteractableInView() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return nullptr; // трейсим только на локальном клиенте владельца
	}

	// Луч — из настоящей камеры (см. UDarcGameplayLibrary::TraceAim).
	FHitResult Hit;
	if (UDarcGameplayLibrary::TraceAim(OwnerPawn, InteractionRange, Hit))
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
		{
			if (IInteractable::Execute_CanInteract(HitActor, Owner))
			{
				return HitActor;
			}
		}
	}

	return nullptr;
}

void UInteractionComponent::TryInteract()
{
	if (FocusedActor)
	{
		Server_Interact(FocusedActor);
	}
}

bool UInteractionComponent::Server_Interact_Validate(AActor* TargetActor)
{
	// Подробная проверка - в Implementation. Здесь только базовая защита от мусора.
	return true;
}

void UInteractionComponent::Server_Interact_Implementation(AActor* TargetActor)
{
	if (!TargetActor || !TargetActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	{
		return;
	}

	AActor* Owner = GetOwner();

	// Сервер никогда не доверяет FocusedActor клиента - перепроверяем дистанцию сами.
	const float DistSq = FVector::DistSquared(Owner->GetActorLocation(), TargetActor->GetActorLocation());
	if (DistSq > FMath::Square(InteractionRange * 1.5f)) // небольшой запас на задержку сети
	{
		return;
	}

	if (IInteractable::Execute_CanInteract(TargetActor, Owner))
	{
		IInteractable::Execute_OnInteract(TargetActor, Owner);
	}
}

void UInteractionComponent::SubmitTerminalInput(ADarcTerminal* Terminal, const FString& Input)
{
	if (Terminal)
	{
		Server_SubmitTerminalInput(Terminal, Input);
	}
}

void UInteractionComponent::LeaveTerminal(ADarcTerminal* Terminal)
{
	if (Terminal)
	{
		Server_LeaveTerminal(Terminal);
	}
}

void UInteractionComponent::Server_SubmitTerminalInput_Implementation(ADarcTerminal* Terminal, const FString& Input)
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (Terminal && OwnerPawn)
	{
		Terminal->ServerHandleInput(OwnerPawn->GetPlayerState(), Input);
	}
}

void UInteractionComponent::Server_LeaveTerminal_Implementation(ADarcTerminal* Terminal)
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (Terminal && OwnerPawn)
	{
		Terminal->ServerReleaseUser(OwnerPawn->GetPlayerState());
	}
}
