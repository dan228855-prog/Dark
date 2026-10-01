// DarcRoomVolume.cpp
#include "DarcRoomVolume.h"
#include "DarcWorldMemorySubsystem.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"

ADarcRoomVolume::ADarcRoomVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetBoxExtent(FVector(400.f, 400.f, 150.f));
	Bounds->SetCollisionProfileName(TEXT("Trigger"));
	Bounds->SetGenerateOverlapEvents(true);
	RootComponent = Bounds;

	// Объём нужен только серверу — реплицировать нечего.
	bReplicates = false;
}

void ADarcRoomVolume::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Bounds->OnComponentBeginOverlap.AddDynamic(this, &ADarcRoomVolume::HandleBeginOverlap);
		Bounds->OnComponentEndOverlap.AddDynamic(this, &ADarcRoomVolume::HandleEndOverlap);
	}
}

void ADarcRoomVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	// Только пешки игроков и только их корневой компонент (капсула), иначе меш
	// и прочие компоненты дадут повторные срабатывания.
	if (!Pawn || !Pawn->GetPlayerState() || OtherComp != Pawn->GetRootComponent())
	{
		return;
	}

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordPlayerEnteredRoom(RoomId.IsNone() ? GetFName() : RoomId, OtherActor);
	}
}

void ADarcRoomVolume::HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->GetPlayerState() || OtherComp != Pawn->GetRootComponent())
	{
		return;
	}

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordPlayerExitedRoom(RoomId.IsNone() ? GetFName() : RoomId, OtherActor);
	}
}
