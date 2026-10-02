// DarcNpc.cpp
#include "DarcNpc.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "DarcAssetSettings.h"
#include "CarryableItem.h"
#include "DarcGameState.h"
#include "DarcPlayerState.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADarcNpc::ADarcNpc()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	GreetZone = CreateDefaultSubobject<USphereComponent>(TEXT("GreetZone"));
	GreetZone->SetupAttachment(RootComponent);
	GreetZone->SetSphereRadius(500.f);
	GreetZone->SetCollisionProfileName(TEXT("Trigger"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADarcNpc::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcNpc, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME(ADarcNpc, bSpeaking);
	DOREPLIFETIME(ADarcNpc, bGaveItem);
}

void ADarcNpc::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual); // модель — у каждой машины сама
	ApplyCharacterModel();

	if (HasAuthority())
	{
		GreetZone->OnComponentBeginOverlap.AddDynamic(this, &ADarcNpc::HandleGreetOverlap);

		// Выдаваемый предмет до выдачи «у NPC в кармане»: скрыт и его нельзя подобрать самому.
		if (ItemToGive && !ItemToGive->CurrentHolder)
		{
			ItemToGive->ServerTransferTo(this, nullptr);
			ItemToGive->SetActorHiddenInGame(true);
		}
	}
}

void ADarcNpc::ApplyCharacterModel()
{
	USkeletalMesh* Mesh = UDarcAssetSettings::FindCharacter(VisualSpec.Slot);
	if (!Mesh)
	{
		return; // нет модели — остаётся коробка
	}
	Body->SetSkeletalMesh(Mesh);
	// Рост — по высоте коробки, ноги — на пол. Модели обычно смотрят вдоль +Y, NPC — вдоль +X.
	const FBox Bounds = Mesh->GetBounds().GetBox();
	const float Height = FMath::Max(Bounds.GetSize().Z, 1.f);
	const float TargetHeight = VisualSpec.Size.Z > 1.f ? VisualSpec.Size.Z : 180.f;
	const float Scale = FMath::Clamp(TargetHeight / Height, 0.5f, 2.f);
	Body->SetRelativeScale3D(FVector(Scale));
	Body->SetRelativeLocation(FVector(0.f, 0.f, -Bounds.Min.Z * Scale));
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Visual->SetHiddenInGame(true);

	if (UAnimationAsset* Idle = UDarcAssetSettings::FindAnimation(FName(*(VisualSpec.Slot.ToString() + TEXT("_Idle")))))
	{
		Body->PlayAnimation(Idle, true);
	}
}

void ADarcNpc::HandleGreetOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	const ADarcPlayerState* PS = Pawn ? Pawn->GetPlayerState<ADarcPlayerState>() : nullptr;
	if (!PS || !PS->bIsAlive)
	{
		return;
	}

	// Подошёл с принимаемым предметом — «Закончили?» (один раз), иначе — приветствие (один раз).
	const ACarryableItem* Held = ACarryableItem::FindItemHeldBy(OtherActor);
	if (AcceptedClass && Held && Held->IsA(AcceptedClass))
	{
		if (!bAskedBeforeReceive)
		{
			bAskedBeforeReceive = true;
			SayLines(BeforeReceiveLines);
		}
		return;
	}

	if (!bGreeted)
	{
		bGreeted = true;
		SayLines(GreetingLines);
	}
}

bool ADarcNpc::CanInteract_Implementation(AActor* Interactor) const
{
	return !bSpeaking;
}

void ADarcNpc::OnInteract_Implementation(AActor* Interactor)
{
	// Сервер. Порядок: принять предмет → выдать предмет → просто поговорить.
	if (bSpeaking)
	{
		return;
	}

	ACarryableItem* Held = ACarryableItem::FindItemHeldBy(Interactor);
	if (AcceptedClass && Held && Held->IsA(AcceptedClass))
	{
		// NPC забирает предмет себе (держатель — NPC, предмет крепится к нему и исчезает из игры).
		Held->ServerTransferTo(this, Interactor);
		Held->SetActorHiddenInGame(true);
		CompleteTaskIfSet(TaskIdOnReceive, Interactor);
		SayLines(ReceiveLines);
		OnItemReceived(Held, Interactor);
		return;
	}

	if (!bGaveItem && ItemToGive && !Held)
	{
		bGaveItem = true;
		ItemToGive->SetActorHiddenInGame(false);
		ItemToGive->ServerTransferTo(Interactor, Interactor);
		if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
		{
			Memory->RecordObjectFound(ItemToGive->GetMemoryId(), Interactor);
		}
		CompleteTaskIfSet(TaskIdOnGive, Interactor);
		SayLines(GiveLines);
		OnItemGiven(ItemToGive, Interactor);
		return;
	}

	SayLines(IdleLines);
}

void ADarcNpc::SayLines(const TArray<FDarcNpcLine>& Lines)
{
	if (!HasAuthority() || Lines.Num() == 0)
	{
		return;
	}

	LineQueue.Append(Lines);
	if (!bSpeaking)
	{
		bSpeaking = true;
		OnRep_Speaking();
		const float FirstDelay = LineQueue[0].DelayBefore;
		GetWorldTimerManager().SetTimer(LineTimer, this, &ADarcNpc::PlayNextLine, FMath::Max(0.01f, FirstDelay), false);
	}
}

void ADarcNpc::PlayNextLine()
{
	if (LineQueue.Num() == 0)
	{
		bSpeaking = false;
		OnRep_Speaking();
		return;
	}

	const FDarcNpcLine Line = LineQueue[0];
	LineQueue.RemoveAt(0);

	if (ADarcGameState* GS = GetWorld()->GetGameState<ADarcGameState>())
	{
		GS->Say(SpeakerName, Line.Text, Line.Duration, GetActorLocation(), HearingRadius);
	}

	// Следующая — после того как дочитали эту, плюс её собственная пауза.
	const float Next = Line.Duration + (LineQueue.Num() > 0 ? LineQueue[0].DelayBefore : 0.f);
	GetWorldTimerManager().SetTimer(LineTimer, this, &ADarcNpc::PlayNextLine, FMath::Max(0.01f, Next), false);
}

void ADarcNpc::OnRep_Speaking()
{
	OnSpeakingChanged(bSpeaking);
}

void ADarcNpc::CompleteTaskIfSet(FName TaskId, AActor* ByActor)
{
	if (TaskId.IsNone())
	{
		return;
	}
	if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
	{
		Tasks->CompleteTask(TaskId, ByActor);
	}
}

FText ADarcNpc::GetInteractionPrompt_Implementation() const
{
	// Подсказка считается на клиенте: смотрим, что в руках у локального игрока.
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ACarryableItem* Held = PC ? ACarryableItem::FindItemHeldBy(PC->GetPawn()) : nullptr;
	return (AcceptedClass && Held && Held->IsA(AcceptedClass)) ? PromptHandOver : PromptTalk;
}
