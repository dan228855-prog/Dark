// DarcCablePlug.cpp
#include "DarcCablePlug.h"
#include "DarcAssetSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace DarcCable
{
	constexpr int32 SegmentCount = 14;
}

ADarcCablePlug::ADarcCablePlug()
{
	PrimaryActorTick.bCanEverTick = true; // провод перерисовывается каждый кадр (вилка двигается)
}

void ADarcCablePlug::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcCablePlug, SocketLocation, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ADarcCablePlug, CableLength, COND_InitialOnly);
}

bool ADarcCablePlug::IsPluggedIn() const
{
	return CurrentHolder && !Cast<APawn>(CurrentHolder);
}

void ADarcCablePlug::Unplug()
{
	if (HasAuthority() && CurrentHolder)
	{
		UDarcAssetSettings::PlaySound(this, TEXT("CableDetach"), GetActorLocation());
		ForceDrop();
	}
}

void ADarcCablePlug::BeginPlay()
{
	Super::BeginPlay();

	// Провод — отрезки-цилиндры без физики тела (у каждой машины свои).
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Rubber = UDarcAssetSettings::MakeColorMaterial(this, FLinearColor(0.012f, 0.012f, 0.012f));
	for (int32 i = 0; i < DarcCable::SegmentCount; ++i)
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
		Segments.Add(Segment);
	}
	// Розетка на стене.
	UStaticMeshComponent* Socket = NewObject<UStaticMeshComponent>(this);
	Socket->SetMobility(EComponentMobility::Movable);
	Socket->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Socket->SetUsingAbsoluteLocation(true);
	Socket->SetUsingAbsoluteRotation(true);
	Socket->SetUsingAbsoluteScale(true);
	Socket->SetupAttachment(GetRootComponent());
	Socket->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Socket->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(this, FLinearColor(0.55f, 0.55f, 0.5f)));
	Socket->RegisterComponent();
	Socket->SetWorldLocation(SocketLocation);
	Socket->SetWorldScale3D(FVector(0.12f));

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(CheckTimer, this, &ADarcCablePlug::ServerCheck, 0.1f, true);
	}
}

void ADarcCablePlug::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateCableVisual();
}

FVector ADarcCablePlug::CurvePoint(float T, const FVector& End, float Slack) const
{
	// Квадратичная кривая Безье с провисом; провод не уходит под пол.
	FVector Middle = (SocketLocation + End) * 0.5f - FVector(0.f, 0.f, 15.f + Slack * 0.6f);
	const float U = 1.f - T;
	FVector P = U * U * SocketLocation + 2.f * U * T * Middle + T * T * End;
	P.Z = FMath::Max(P.Z, 1.5f);
	return P;
}

void ADarcCablePlug::UpdateCableVisual()
{
	if (Segments.Num() == 0)
	{
		return;
	}
	const FVector End = GetActorLocation();
	const float Slack = FMath::Max(CableLength - FVector::Dist(SocketLocation, End), 0.f);
	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		const FVector A = CurvePoint(static_cast<float>(i) / Segments.Num(), End, Slack);
		const FVector B = CurvePoint(static_cast<float>(i + 1) / Segments.Num(), End, Slack);
		const FVector Dir = B - A;
		Segments[i]->SetWorldLocationAndRotation((A + B) * 0.5f, FRotationMatrix::MakeFromZ(Dir.GetSafeNormal()).Rotator());
		Segments[i]->SetWorldScale3D(FVector(0.025f, 0.025f, FMath::Max(Dir.Size(), 1.f) / 100.f + 0.01f));
	}
}

void ADarcCablePlug::ServerCheck()
{
	const float Span = FVector::Dist(SocketLocation, GetActorLocation());

	// Дальше длины провода — вилку вырывает (из руки или из оборудования).
	if (CurrentHolder && Span > CableLength)
	{
		Unplug();
		return;
	}

	// Натянутый подключённый провод: игрок, пересекающий его на бегу, выдёргивает вилку.
	if (!IsPluggedIn() || Span < CableLength * 0.75f)
	{
		return;
	}
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	const FVector CableDir = (GetActorLocation() - SocketLocation).GetSafeNormal2D();
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		const FVector Location = Pawn->GetActorLocation();
		const FVector Closest = FMath::ClosestPointOnSegment(Location, SocketLocation, GetActorLocation());
		const FVector Velocity = Pawn->GetVelocity();
		const float Across = (Velocity - CableDir * FVector::DotProduct(Velocity, CableDir)).Size2D();
		// Ноги на уровне провода (провод низко) и быстро поперёк — зацепился.
		if (FVector::Dist2D(Location, Closest) < 35.f && Across > TripSpeed)
		{
			Unplug();
			return;
		}
	}
}
