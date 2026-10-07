// DarcPowerInlet.cpp
#include "DarcPowerInlet.h"
#include "Components/StaticMeshComponent.h"
#include "DarcGameState.h"
#include "DarcGameplayLibrary.h"
#include "DarcGenerator.h"
#include "DarcPowerSubsystem.h"
#include "DarcWorldMemorySubsystem.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace DarcInletCable
{
	constexpr int32 SegmentCount = 12;
}

ADarcPowerInlet::ADarcPowerInlet()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true; // только ради видимого кабеля, пока генератор подключён
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.05f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	StatusLamp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusLamp"));
	StatusLamp->SetupAttachment(RootComponent);
	StatusLamp->SetMobility(EComponentMobility::Movable);
	StatusLamp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StatusLamp->SetCastShadow(false);
	StatusLamp->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	StatusLamp->SetRelativeScale3D(FVector(0.05f)); // Ø5 см
}

void ADarcPowerInlet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcPowerInlet, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME(ADarcPowerInlet, ConnectedGenerator);
	DOREPLIFETIME_CONDITION(ADarcPowerInlet, Load, COND_InitialOnly);
}

bool ADarcPowerInlet::IsInCableRange(const ADarcGenerator* Generator) const
{
	return Generator && FVector::Dist(Generator->GetActorLocation(), GetActorLocation()) <= CableLength;
}

bool ADarcPowerInlet::IsSupplying() const
{
	return ConnectedGenerator && ConnectedGenerator->bRunning && IsInCableRange(ConnectedGenerator);
}

ADarcGenerator* ADarcPowerInlet::FindGeneratorInRange() const
{
	ADarcGenerator* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<ADarcGenerator> It(GetWorld()); It; ++It)
	{
		const float Dist = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (Dist <= CableLength && Dist < BestDist)
		{
			Best = *It;
			BestDist = Dist;
		}
	}
	return Best;
}

void ADarcPowerInlet::ValidateConnection()
{
	if (HasAuthority() && ConnectedGenerator && !IsInCableRange(ConnectedGenerator))
	{
		// Кабель выдернулся — генератор продолжает работать вхолостую, сервер теряет питание.
		ConnectedGenerator = nullptr;
		OnRep_Connected();
	}
}

namespace DarcInletLoad
{
	/** Суммарная нагрузка всех вводов, подключённых к этому генератору (и у сервера, и у клиента). */
	int32 Of(const UWorld* World, const ADarcGenerator* Generator)
	{
		int32 Total = 0;
		for (TActorIterator<ADarcPowerInlet> It(World); It; ++It)
		{
			if (It->ConnectedGenerator == Generator)
			{
				Total += FMath::Max(It->Load, 0);
			}
		}
		return Total;
	}
}

bool ADarcPowerInlet::CanInteract_Implementation(AActor* Interactor) const
{
	// Всегда: если генератор далеко, подсказка скажет, что его нужно подтащить.
	return true;
}

void ADarcPowerInlet::OnInteract_Implementation(AActor* Interactor)
{
	// Только сервер.
	// Подключён, но заглох (перегрузка или заглушили на другом вводе): запустить снова, если он
	// потянет всё подключённое; иначе этот ввод отключается (ниже) — иначе перегрузка по кругу.
	if (ConnectedGenerator && !ConnectedGenerator->bRunning && IsInCableRange(ConnectedGenerator)
		&& DarcInletLoad::Of(GetWorld(), ConnectedGenerator) <= ConnectedGenerator->Capacity)
	{
		ConnectedGenerator->SetRunning(true);
		if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
		{
			Memory->RecordInteraction(CircuitId, TEXT("GeneratorRestarted"), Interactor);
		}
		return;
	}
	if (ConnectedGenerator)
	{
		// Отключить = отсоединить; заглушить, если к генератору больше ничего не подключено.
		ADarcGenerator* Generator = ConnectedGenerator;
		ConnectedGenerator = nullptr;
		bool bStillUsed = false;
		for (TActorIterator<ADarcPowerInlet> It(GetWorld()); It; ++It)
		{
			bStillUsed |= (*It != this && It->ConnectedGenerator == Generator);
		}
		if (!bStillUsed)
		{
			Generator->SetRunning(false);
		}
	}
	else
	{
		// Ближайший генератор, до которого достаёт кабель.
		ADarcGenerator* Best = FindGeneratorInRange();
		if (!Best)
		{
			// Раньше E здесь молча ничего не делал — было непонятно, что не так.
			if (ADarcGameState* GS = GetWorld()->GetGameState<ADarcGameState>())
			{
				GS->Say(FText::GetEmpty(), UDarcGameplayLibrary::UIText(TEXT("Inlet_TooFarSay")), 3.5f, GetActorLocation(), 1200.f);
			}
			return;
		}
		ConnectedGenerator = Best;
		Best->SetRunning(true); // «подключить и запустить» — одно действие
	}

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(CircuitId, ConnectedGenerator ? TEXT("GeneratorConnected") : TEXT("GeneratorDisconnected"), Interactor);
	}

	OnRep_Connected();
	if (UDarcPowerSubsystem* Power = UDarcPowerSubsystem::GetPower(this))
	{
		Power->Recompute();
	}
}

void ADarcPowerInlet::OnRep_Connected()
{
	// Кабель виден, пока генератор подключён; тик нужен только для него.
	SetActorTickEnabled(ConnectedGenerator != nullptr);
	UpdateCableVisual();
	UpdateStatusLamp();
	OnConnectionChanged(ConnectedGenerator);
}

FText ADarcPowerInlet::GetInteractionPrompt_Implementation() const
{
	if (ConnectedGenerator)
	{
		const bool bCanRestart = !ConnectedGenerator->bRunning && DarcInletLoad::Of(GetWorld(), ConnectedGenerator) <= ConnectedGenerator->Capacity;
		return (bCanRestart && !PromptRestart.IsEmpty()) ? PromptRestart : PromptDisconnect;
	}
	// Подсказка считается у клиента: генераторы и их позиции реплицируются.
	return (FindGeneratorInRange() || PromptTooFar.IsEmpty()) ? PromptConnect : PromptTooFar;
}

void ADarcPowerInlet::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual); // модель — у каждой машины сама

	// Лампа — на лицевой стороне (+X) у верха модели.
	const FBox Box = Visual->Bounds.GetBox().TransformBy(GetActorTransform().Inverse());
	StatusLamp->SetRelativeLocation(FVector(Box.Max.X + 1.5f, Box.GetCenter().Y, Box.Max.Z - 4.f));

	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Rubber = UDarcAssetSettings::MakeColorMaterial(this, FLinearColor(0.015f, 0.015f, 0.015f));
	for (int32 i = 0; i < DarcInletCable::SegmentCount; ++i)
	{
		UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this);
		Segment->SetMobility(EComponentMobility::Movable);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetCastShadow(false);
		Segment->SetUsingAbsoluteLocation(true);
		Segment->SetUsingAbsoluteRotation(true);
		Segment->SetUsingAbsoluteScale(true);
		Segment->SetupAttachment(GetRootComponent());
		Segment->SetStaticMesh(Cylinder);
		Segment->SetMaterial(0, Rubber);
		Segment->RegisterComponent();
		CableSegments.Add(Segment);
	}
	OnRep_Connected();
}

void ADarcPowerInlet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateCableVisual(); // генератор могут тащить — кабель тянется за ним
	UpdateStatusLamp();
}

void ADarcPowerInlet::UpdateCableVisual()
{
	const bool bShow = ConnectedGenerator != nullptr;
	for (UStaticMeshComponent* Segment : CableSegments)
	{
		Segment->SetVisibility(bShow);
	}
	if (!bShow || CableSegments.Num() == 0)
	{
		return;
	}
	// От ввода — по полу с провисом — к генератору.
	const FVector Start = GetActorLocation();
	FVector End = ConnectedGenerator->GetActorLocation();
	End.Z = FMath::Max(End.Z - 20.f, 3.f);
	const float Slack = FMath::Max(CableLength - FVector::Dist(Start, End), 0.f);
	const FVector Middle = (Start + End) * 0.5f - FVector(0.f, 0.f, 20.f + Slack * 0.5f);
	auto Curve = [&](float T)
	{
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
		CableSegments[i]->SetWorldLocationAndRotation((A + B) * 0.5f, FRotationMatrix::MakeFromZ(Dir.GetSafeNormal()).Rotator());
		CableSegments[i]->SetWorldScale3D(FVector(0.03f, 0.03f, FMath::Max(Dir.Size(), 1.f) / 100.f + 0.01f));
	}
}

void ADarcPowerInlet::UpdateStatusLamp()
{
	const bool bSupplying = IsSupplying();
	if (StatusLamp && (!bLampShowsSupply.IsSet() || bLampShowsSupply.GetValue() != bSupplying))
	{
		bLampShowsSupply = bSupplying;
		StatusLamp->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(this,
			bSupplying ? FLinearColor(0.1f, 1.f, 0.2f) : FLinearColor(1.f, 0.05f, 0.03f), bSupplying ? 8.f : 3.f));
	}
}
