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
	PrimaryActorTick.bCanEverTick = true; // только ради видимого кабеля; без кабеля тик выключен
	PrimaryActorTick.bStartWithTickEnabled = false;

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
	DOREPLIFETIME_CONDITION(ADarcHeavyObject, CableAnchor, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ADarcHeavyObject, CableLength, COND_InitialOnly);
}

void ADarcHeavyObject::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.bLocalUV = true; // объект двигается — текстура привязана к нему
	VisualSpec.ApplyTo(Body); // модель — у каждой машины сама
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); // модель с карты могла принести свою коллизию
	FitCollisionToVisual();

	// Масса — после размера тела. Угловое затухание — чтобы не кувыркался от каждого толчка.
	Collision->SetSimulatePhysics(true);
	Collision->SetMassOverrideInKg(NAME_None, MassKg, true);
	Collision->SetAngularDamping(1.5f);
	ApplyCableDrag();
	BuildCableVisual();

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
	// Рывок или натяжение сильнее длины кабеля — вилку выдёргивает.
	if (bHasFragileCable && bCableAttached)
	{
		const bool bJerk = Collision->GetPhysicsLinearVelocity().Size() > BreakSpeed;
		const bool bOverstretched = HasVisibleCable() && FVector::Dist(CableAnchor, GetCablePlugPoint()) > CableLength;
		if (bJerk || bOverstretched)
		{
			DetachCable();
		}
	}

	// Доставка (с кабелем — только подключённой).
	if (!bDelivered && DeliveryTarget && !TaskIdOnDelivered.IsNone() && (!bHasFragileCable || bCableAttached)
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
	// До розетки не достать — не подключаем (подсказка об этом уже в тексте на экране).
	if (HasVisibleCable() && FVector::Dist(CableAnchor, GetCablePlugPoint()) > CableLength * 0.95f)
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
	UpdateCableVisual();
	if (HasActorBegunPlay())
	{
		UDarcAssetSettings::PlaySound(this, bCableAttached ? TEXT("CableAttach") : TEXT("CableDetach"), GetActorLocation());
	}
	OnCableStateChanged(bCableAttached);
}

FText ADarcHeavyObject::GetInteractionPrompt_Implementation() const
{
	if (HasVisibleCable() && FVector::Dist(CableAnchor, GetCablePlugPoint()) > CableLength * 0.95f && !PromptCableTooFar.IsEmpty())
	{
		return PromptCableTooFar;
	}
	return PromptReattachCable;
}

FVector ADarcHeavyObject::GetCablePlugPoint() const
{
	// Сзади (локальная -X) у верха тела.
	const FVector Extent = Collision->GetScaledBoxExtent();
	return Collision->GetComponentTransform().TransformPosition(FVector(-Extent.X, 0.f, Extent.Z * 0.6f));
}

void ADarcHeavyObject::BuildCableVisual()
{
	if (!HasVisibleCable() || CableSegments.Num() > 0)
	{
		return;
	}
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Rubber = UDarcAssetSettings::MakeColorMaterial(this, FLinearColor(0.015f, 0.015f, 0.015f));
	auto MakePart = [this, Rubber](UStaticMesh* Shape)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->SetUsingAbsoluteLocation(true);
		Part->SetUsingAbsoluteRotation(true);
		Part->SetUsingAbsoluteScale(true);
		Part->SetupAttachment(GetRootComponent());
		Part->SetStaticMesh(Shape);
		Part->SetMaterial(0, Rubber);
		Part->RegisterComponent();
		return Part;
	};
	for (int32 i = 0; i < 12; ++i)
	{
		CableSegments.Add(MakePart(Cylinder));
	}
	// Розетка на стене — где начинается кабель.
	CableSocketVisual = MakePart(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	CableSocketVisual->SetWorldLocation(CableAnchor);
	CableSocketVisual->SetWorldScale3D(FVector(0.12f, 0.12f, 0.16f));
	CableSocketVisual->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(this, FLinearColor(0.5f, 0.5f, 0.48f)));
	SetActorTickEnabled(true);
	UpdateCableVisual();
}

void ADarcHeavyObject::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateCableVisual();
}

void ADarcHeavyObject::UpdateCableVisual()
{
	if (CableSegments.Num() == 0)
	{
		return;
	}
	// Подключён — от розетки к объекту, с провисом (чем свободнее кабель, тем ниже); выдернут —
	// лежит на полу от розетки, вилкой в сторону объекта.
	const FVector Start = CableAnchor;
	FVector End = GetCablePlugPoint();
	if (!bCableAttached)
	{
		FVector Toward = End - Start;
		Toward.Z = 0.f;
		End = FVector(Start.X, Start.Y, 2.f) + Toward.GetSafeNormal() * FMath::Min(Toward.Size(), CableLength * 0.5f);
	}
	const float Span = FVector::Dist(Start, End);
	const float Slack = FMath::Max(CableLength - Span, 0.f);
	FVector Middle = (Start + End) * 0.5f - FVector(0.f, 0.f, 20.f + Slack * 0.6f);
	Middle.Z = FMath::Max(Middle.Z, 2.f); // не уходит под пол

	auto Curve = [&](float T)
	{
		// Квадратичная кривая Безье: розетка → провис → вилка.
		const float U = 1.f - T;
		FVector P = U * U * Start + 2.f * U * T * Middle + T * T * End;
		P.Z = FMath::Max(P.Z, 2.f);
		return P;
	};
	const int32 Count = CableSegments.Num();
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector A = Curve(static_cast<float>(i) / Count);
		const FVector B = Curve(static_cast<float>(i + 1) / Count);
		const FVector Dir = B - A;
		UStaticMeshComponent* Segment = CableSegments[i];
		Segment->SetWorldLocationAndRotation((A + B) * 0.5f, FRotationMatrix::MakeFromZ(Dir.GetSafeNormal()).Rotator());
		Segment->SetWorldScale3D(FVector(0.025f, 0.025f, FMath::Max(Dir.Size(), 1.f) / 100.f + 0.01f));
	}
}
