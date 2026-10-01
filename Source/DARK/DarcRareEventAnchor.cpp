// DarcRareEventAnchor.cpp
#include "DarcRareEventAnchor.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "DarcAssetSettings.h"
#include "InteractableDoor.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "EngineUtils.h"

ADarcRareEventAnchor::ADarcRareEventAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Реплицируется, чтобы ссылку на якорь можно было передать клиентам в RPC.
	// Состояния у него нет, поэтому трафика почти нет.
	bReplicates = true;
	SetReplicateMovement(false);
	// Всегда релевантен: иначе у далёкого клиента ссылка на якорь в RPC придёт пустой
	// (например, «шаги» в комнате, из которой игрок уже ушёл далеко).
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	EventLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EventLight"));
	EventLight->SetupAttachment(RootComponent);
	EventLight->SetMobility(EComponentMobility::Movable);
	EventLight->SetIntensity(3000.f);
	EventLight->SetLightColor(FLinearColor(1.f, 0.85f, 0.6f)); // тёплый свет окна
	EventLight->SetVisibility(false);
}

void ADarcRareEventAnchor::OnEventFiredOnServer_Implementation(const FDarcRareEventPayload& Payload)
{
	// Серверная часть: только то, что меняет общее состояние мира (дверь — реплицируется).
	if (Behavior == EDarcAnchorBehavior::VanishingRoom)
	{
		if (RoomDoor)
		{
			// Дверь закрыта и не открывается картой; «признаков взлома нет».
			RoomDoor->SetDoorOpen(false, nullptr);
			RoomDoor->SetLocked(true);
		}
		GetWorldTimerManager().SetTimer(ServerEffectTimer, this, &ADarcRareEventAnchor::EndServerEffect, EffectDuration, false);
	}
}

void ADarcRareEventAnchor::EndServerEffect()
{
	// Через время дверь снова открыта, а внутри шкаф стоит иначе.
	if (RoomDoor)
	{
		RoomDoor->SetLocked(false);
		RoomDoor->SetDoorOpen(true, nullptr);
	}
	if (TargetActor && TargetActor->HasAuthority())
	{
		TargetActor->AddActorWorldOffset(FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(60.f, 120.f), 0.f));
		TargetActor->AddActorWorldRotation(FRotator(0.f, FMath::FRandRange(-25.f, 25.f), 0.f));
	}
}

void ADarcRareEventAnchor::PlayEventLocally_Implementation(const FDarcRareEventPayload& Payload)
{
	// Звук события: слот звука = ID события (например "Slice_SpeakerMorse").
	UDarcAssetSettings::PlaySound(this, Payload.EventId, GetActorLocation());

	switch (Behavior)
	{
	case EDarcAnchorBehavior::LightOn:
		EventLight->SetVisibility(true); // и остаётся — без объяснений
		break;
	case EDarcAnchorBehavior::Levitate:
		// Локально у того, кто это видит: предмет висит в паре сантиметров над столом.
		if (AActor* Target = ResolveTarget())
		{
			if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
			{
				Body->SetSimulatePhysics(false); // иначе физика сразу «уронит» его обратно
			}
			TargetOriginalLocation = Target->GetActorLocation();
			Target->SetActorLocation(TargetOriginalLocation + FVector(0.f, 0.f, 2.5f));
			GetWorldTimerManager().SetTimer(LocalEffectTimer, this, &ADarcRareEventAnchor::EndLocalEffect, EffectDuration, false);
		}
		break;
	default:
		break;
	}
}

void ADarcRareEventAnchor::EndLocalEffect()
{
	if (AActor* Target = ResolveTarget())
	{
		Target->SetActorLocation(TargetOriginalLocation);
		if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
		{
			Body->SetSimulatePhysics(true);
		}
	}
}

AActor* ADarcRareEventAnchor::ResolveTarget()
{
	if (TargetActor || TargetTag.IsNone())
	{
		return TargetActor;
	}
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TargetTag))
		{
			const float Dist = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
			if (Dist < BestDist)
			{
				BestDist = Dist;
				TargetActor = *It; // запоминаем локально (на этой машине)
			}
		}
	}
	return TargetActor;
}

void ADarcRareEventAnchor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcRareEventAnchor, VisualSpec, COND_InitialOnly);
}

void ADarcRareEventAnchor::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual); // модель — у каждой машины сама
}
