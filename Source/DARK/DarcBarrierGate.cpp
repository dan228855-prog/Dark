// DarcBarrierGate.cpp
#include "DarcBarrierGate.h"
#include "DarcAssetSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

ADarcBarrierGate::ADarcBarrierGate()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(RootComponent);
	Pivot->SetMobility(EComponentMobility::Movable);
	Pivot->SetRelativeLocation(FVector(0.f, 0.f, 100.f)); // ось стрелы — на высоте 1 м
}

void ADarcBarrierGate::BeginPlay()
{
	Super::BeginPlay();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	auto MakePart = [this](UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Location, const FVector& Size, const FLinearColor& Color, bool bCollide)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
		Part->SetupAttachment(Parent);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Size / 100.f);
		Part->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(Part, Color));
		Part->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		Part->RegisterComponent();
		return Part;
	};

	// Стойка с противовесом.
	MakePart(Cylinder, RootComponent, FVector(0.f, 0.f, 55.f), FVector(28.f, 28.f, 110.f), FLinearColor(0.55f, 0.45f, 0.08f), true);
	MakePart(Cube, Pivot, FVector(0.f, 35.f, 0.f), FVector(16.f, 60.f, 22.f), FLinearColor(0.08f, 0.08f, 0.09f), true);

	// Стрела: красно-белые полосы вдоль -Y, у каждой — коллизия (закрытый шлагбаум не пройти).
	const int32 Stripes = FMath::Max(2, FMath::RoundToInt(ArmLength / 80.f));
	const float StripeLength = ArmLength / Stripes;
	for (int32 i = 0; i < Stripes; ++i)
	{
		const FLinearColor Color = (i % 2 == 0) ? FLinearColor(0.7f, 0.04f, 0.03f) : FLinearColor(0.78f, 0.78f, 0.74f);
		ArmParts.Add(MakePart(Cube, Pivot, FVector(0.f, -StripeLength * (i + 0.5f), 0.f), FVector(9.f, StripeLength, 9.f), Color, true));
	}
	// Упор на другой стороне дороги, куда ложится стрела.
	MakePart(Cylinder, RootComponent, FVector(0.f, -ArmLength + 10.f, 45.f), FVector(10.f, 10.f, 90.f), FLinearColor(0.08f, 0.08f, 0.09f), true);
}

void ADarcBarrierGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Есть ли живой игрок рядом (по горизонтали от середины стрелы или от стойки).
	bool bSomeoneNear = false;
	const FVector Base = GetActorLocation();
	const FVector Middle = GetActorTransform().TransformPosition(FVector(0.f, -ArmLength * 0.5f, 0.f));
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
			if (Pawn && (FVector::Dist2D(Pawn->GetActorLocation(), Base) < TriggerRadius
				|| FVector::Dist2D(Pawn->GetActorLocation(), Middle) < TriggerRadius))
			{
				bSomeoneNear = true;
				break;
			}
		}
	}

	// Плавно: линейный ход «прогресса» + сглаженная кривая угла (медленный старт и мягкая остановка).
	const float Target = bSomeoneNear ? 1.f : 0.f;
	OpenAlpha = FMath::FInterpConstantTo(OpenAlpha, Target, DeltaSeconds, 1.f / FMath::Max(OpenSeconds, 0.1f));
	const float Angle = OpenAngle * FMath::InterpEaseInOut(0.f, 1.f, OpenAlpha, 2.f);

	// Подъём: поворот вокруг оси X стойки так, чтобы конец стрелы (−Y) уходил вверх.
	Pivot->SetRelativeRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-Angle)));

	if (bSomeoneNear && !bWasOpening && OpenAlpha < 0.05f)
	{
		UDarcAssetSettings::PlaySound(this, TEXT("BarrierMotor"), Base, 0.7f);
	}
	bWasOpening = bSomeoneNear;
}
