// DarcHeavyObject.cpp
#include "DarcHeavyObject.h"
#include "DarcAssetSettings.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADarcHeavyObject::ADarcHeavyObject()
{
	bReplicates = true;
	SetReplicateMovement(true); // физику считает сервер, клиенты получают позицию
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetCollisionProfileName(TEXT("PhysicsActor"));
	Collision->SetBoxExtent(FVector(50.f));
	RootComponent = Collision;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetMobility(EComponentMobility::Movable);
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
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); // модель с карты могла принести свою коллизию
	FitCollisionToVisual();

	// Масса — после размера тела. Угловое затухание — чтобы не кувыркался от каждого толчка.
	Collision->SetSimulatePhysics(true);
	Collision->SetMassOverrideInKg(NAME_None, MassKg, true);
	Collision->SetAngularDamping(1.5f);
	ApplyCableDrag();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(CheckTimer, this, &ADarcHeavyObject::ServerCheck, 0.1f, true);
	}
}

void ADarcHeavyObject::FitCollisionToVisual()
{
	if (!VisualSpec.CopyFrom && !VisualSpec.Size.IsNearlyZero())
	{
		// Коробка = размер из спека; модель вписана с низом в начале координат — опускаем
		// её на полвысоты, чтобы она стояла внутри коробки.
		const FVector Extent = VisualSpec.Size * 0.5f;
		Collision->SetBoxExtent(Extent);
		Body->AddRelativeLocation(FVector(0.f, 0.f, -Extent.Z));
		return;
	}
	if (!Body->GetStaticMesh())
	{
		return;
	}
	// Модель с карты: коробка — по её габаритам, центр тела — в центре модели (модель на месте).
	const FTransform MeshWorld = Body->GetComponentTransform();
	const FBox Local = Body->GetStaticMesh()->GetBoundingBox();
	const FVector Extent = (Local.GetExtent() * MeshWorld.GetScale3D().GetAbs()).ComponentMax(FVector(5.f));
	Collision->SetBoxExtent(Extent);
	Collision->SetWorldLocationAndRotation(MeshWorld.TransformPosition(Local.GetCenter()), MeshWorld.GetRotation(),
		false, nullptr, ETeleportType::TeleportPhysics);
	Body->SetWorldTransform(MeshWorld);
}

void ADarcHeavyObject::ServerCheck()
{
	// Рывок — кабель срывается.
	if (bHasFragileCable && bCableAttached && Collision->GetPhysicsLinearVelocity().Size() > BreakSpeed)
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
	Collision->SetLinearDamping(bCableAttached ? 0.2f : 25.f);
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
