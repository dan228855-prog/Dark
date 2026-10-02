// DarcHeavyObject.cpp
#include "DarcHeavyObject.h"
#include "DarcAssetSettings.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADarcHeavyObject::ADarcHeavyObject()
{
	bReplicates = true;
	SetReplicateMovement(true); // физику считает сервер, клиенты получают позицию
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetSimulatePhysics(true);
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	RootComponent = Body;
}

void ADarcHeavyObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcHeavyObject, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME(ADarcHeavyObject, bCableAttached);
}

void ADarcHeavyObject::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Body); // модель — у каждой машины сама
	UDarcAssetSettings::EnsurePhysicsCollision(Body, VisualSpec);

	// Масса — после модели (смена модели пересоздаёт физическое тело).
	Body->SetSimulatePhysics(true);
	Body->SetMassOverrideInKg(NAME_None, MassKg, true);
	ApplyCableDrag();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(CheckTimer, this, &ADarcHeavyObject::ServerCheck, 0.1f, true);
	}
}

void ADarcHeavyObject::ServerCheck()
{
	// Рывок — кабель срывается.
	if (bHasFragileCable && bCableAttached && Body->GetPhysicsLinearVelocity().Size() > BreakSpeed)
	{
		DetachCable();
	}

	// Доставка.
	if (!bDelivered && DeliveryTarget && !TaskIdOnDelivered.IsNone()
		&& FVector::Dist2D(GetActorLocation(), DeliveryTarget->GetActorLocation()) <= DeliveryRadius)
	{
		bDelivered = true;
		if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
		{
			Tasks->CompleteTask(TaskIdOnDelivered, nullptr);
		}
	}
}

void ADarcHeavyObject::DetachCable()
{
	if (!HasAuthority() || !bCableAttached)
	{
		return;
	}
	bCableAttached = false;
	OnRep_Cable();
	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(GetFName(), TEXT("CableDetached"), nullptr);
	}
}

void ADarcHeavyObject::ApplyCableDrag()
{
	// Оборванный кабель «держит» объект: сильное сопротивление движению, пока не закрепят.
	Body->SetLinearDamping(bCableAttached ? 0.2f : 25.f);
}

bool ADarcHeavyObject::CanInteract_Implementation(AActor* Interactor) const
{
	return !bCableAttached; // взаимодействие только чтобы закрепить кабель; тащат — захватом (ЛКМ)
}

void ADarcHeavyObject::OnInteract_Implementation(AActor* Interactor)
{
	if (bCableAttached)
	{
		return;
	}
	bCableAttached = true;
	OnRep_Cable();
	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(GetFName(), TEXT("CableReattached"), Interactor);
	}
}

void ADarcHeavyObject::OnRep_Cable()
{
	ApplyCableDrag();
	if (HasActorBegunPlay())
	{
		UDarcAssetSettings::PlaySound(this, bCableAttached ? TEXT("CableAttach") : TEXT("CableDetach"), GetActorLocation());
	}
	OnCableStateChanged(bCableAttached);
}

FText ADarcHeavyObject::GetInteractionPrompt_Implementation() const
{
	return PromptReattachCable;
}
