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
#include "DarcGameplayLibrary.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

ADarcNpc::ADarcNpc()
{
	bReplicates = true;
	// Тик нужен только на время рывка из засады.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	GreetZone = CreateDefaultSubobject<USphereComponent>(TEXT("GreetZone"));
	GreetZone->SetupAttachment(RootComponent);
	GreetZone->SetSphereRadius(500.f);
	GreetZone->SetCollisionProfileName(TEXT("Trigger"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Visual->SetReceivesDecals(false);
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetReceivesDecals(false);
}

void ADarcNpc::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcNpc, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME(ADarcNpc, bSpeaking);
	DOREPLIFETIME(ADarcNpc, bGaveItem);
	DOREPLIFETIME(ADarcNpc, bAmbushTriggered);
}

namespace DarcNpcPreset
{
	FDarcNpcLine Line(const TCHAR* Key, float DelayBefore, float Duration)
	{
		FDarcNpcLine L;
		L.Text = UDarcGameplayLibrary::UIText(Key);
		L.DelayBefore = DelayBefore;
		L.Duration = Duration;
		return L;
	}
}

void ADarcNpc::ApplyGuardPreset()
{
	// Те же реплики, что у охранника среза (DarcSliceBuilder), — только для пустых полей.
	using DarcNpcPreset::Line;
	if (SpeakerName.IsEmpty())        SpeakerName = UDarcGameplayLibrary::UIText(TEXT("Speaker_Guard"));
	if (GreetingLines.Num() == 0)      GreetingLines = { Line(TEXT("Guard_Greeting"), 0.3f, 3.f) };
	if (GiveLines.Num() == 0)          GiveLines = { Line(TEXT("Guard_GiveCard"), 0.2f, 2.5f) };
	if (BeforeReceiveLines.Num() == 0) BeforeReceiveLines = { Line(TEXT("Guard_Finished"), 0.2f, 2.5f) };
	if (ReceiveLines.Num() == 0)       ReceiveLines = { Line(TEXT("Guard_Good"), 0.3f, 2.f), Line(TEXT("Guard_GoBack"), 1.5f, 3.f) };
	if (AmbushLines.Num() == 0)        AmbushLines = { Line(TEXT("Guard_Ambush"), 0.05f, 2.5f), Line(TEXT("Guard_Greeting"), 0.6f, 3.f) };
	if (PromptTalk.IsEmpty())          PromptTalk = UDarcGameplayLibrary::UIText(TEXT("Npc_Talk"));
	if (PromptHandOver.IsEmpty())      PromptHandOver = UDarcGameplayLibrary::UIText(TEXT("Npc_HandOver"));
}

void ADarcNpc::BeginPlay()
{
	Super::BeginPlay();
	if (bGuardPreset)
	{
		ApplyGuardPreset();
	}
	VisualSpec.ApplyTo(Visual); // модель — у каждой машины сама
	ApplyCharacterModel();

	// Засада: стоим за укрытием и не видны, пока игрок не подойдёт (у каждой машины одинаково:
	// NPC стоит на карте, исходное место — его конечная точка).
	if (bAmbush && !AmbushStart.IsNearlyZero())
	{
		AmbushEnd = GetActorTransform();
		// Землю могли переделать после расстановки: обе точки ставим на землю.
		AmbushEnd.SetLocation(OnGround(AmbushEnd.GetLocation()));
		AmbushStart = OnGround(AmbushStart);
		bAmbushDone = false;
		SetActorLocation(AmbushStart);
		SetActorHiddenInGame(true);
		if (HasAuthority())
		{
			GetWorldTimerManager().SetTimer(AmbushTimer, this, &ADarcNpc::CheckAmbush, 0.15f, true);
		}
	}

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
	Body->SetSkeletalMeshAsset(Mesh);
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

void ADarcNpc::CheckAmbush()
{
	// Сервер: первый живой игрок в радиусе от места NPC — выбегаем.
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS || bAmbushTriggered)
	{
		return;
	}
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		const ADarcPlayerState* DarcPS = Cast<ADarcPlayerState>(PS);
		if (!Pawn || (DarcPS && !DarcPS->bIsAlive))
		{
			continue;
		}
		if (FVector::Dist2D(Pawn->GetActorLocation(), AmbushEnd.GetLocation()) <= AmbushTriggerRadius)
		{
			GetWorldTimerManager().ClearTimer(AmbushTimer);
			bAmbushTriggered = true;
			bGreeted = true; // окрик вместо обычного приветствия
			OnRep_Ambush(); // на сервере OnRep сам не вызывается
			SayLines(AmbushLines);
			return;
		}
	}
}

void ADarcNpc::OnRep_Ambush()
{
	if (!bAmbushTriggered || bAmbushDone)
	{
		return;
	}
	SetActorHiddenInGame(false);
	AmbushTime = 0.f;
	SetActorTickEnabled(true);
	if (UAnimationAsset* Run = UDarcAssetSettings::FindAnimation(FName(*(VisualSpec.Slot.ToString() + TEXT("_Run")))))
	{
		Body->PlayAnimation(Run, true);
	}
	UDarcAssetSettings::PlaySound(this, TEXT("GuardShout"), GetActorLocation());
	// Резкий удар (дверь будки) там, откуда выбегает охранник: объёмный звук с этой стороны —
	// игрок поворачивает голову на источник.
	UDarcAssetSettings::PlaySound(this, TEXT("GuardAmbushSlam"), AmbushStart, 1.6f);
}

FVector ADarcNpc::OnGround(const FVector& Point) const
{
	// Ноги NPC — в начале координат актора; ищем землю под точкой (статика: ландшафт, площадка).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcNpcGround), false, this);
	FHitResult Hit;
	const FVector Start = Point + FVector(0.f, 0.f, 300.f);
	const FVector End = Point - FVector(0.f, 0.f, 3000.f);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint;
	}
	return Point;
}

void ADarcNpc::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bAmbushDone)
	{
		SetActorTickEnabled(false);
		return;
	}
	// Рывок из-за укрытия: быстро в начале, тормозит у места; смотрит туда, куда бежит,
	// в конце разворачивается к дороге (исходный поворот NPC).
	AmbushTime += DeltaSeconds;
	const float Alpha = FMath::Clamp(AmbushTime / FMath::Max(AmbushRunSeconds, 0.2f), 0.f, 1.f);
	const float Eased = 1.f - FMath::Square(1.f - Alpha);
	const FVector Location = OnGround(FMath::Lerp(AmbushStart, AmbushEnd.GetLocation(), Eased));
	const FVector Direction = (AmbushEnd.GetLocation() - AmbushStart).GetSafeNormal2D();
	const FQuat RunFacing = FRotator(0.f, Direction.Rotation().Yaw, 0.f).Quaternion();
	const FQuat Facing = FQuat::Slerp(RunFacing, AmbushEnd.GetRotation(), FMath::SmoothStep(0.7f, 1.f, Alpha));
	SetActorLocationAndRotation(Location, Facing);

	if (Alpha >= 1.f)
	{
		SetActorTransform(AmbushEnd);
		bAmbushDone = true;
		if (UAnimationAsset* Idle = UDarcAssetSettings::FindAnimation(FName(*(VisualSpec.Slot.ToString() + TEXT("_Idle")))))
		{
			Body->PlayAnimation(Idle, true);
		}
	}
}

void ADarcNpc::HandleGreetOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Пока засада не сработала, приветствия нет — его заменяет окрик на выбегании.
	if (bAmbush && !bAmbushTriggered)
	{
		return;
	}
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
	// Пока выбегает из засады — не разговаривает.
	return !bSpeaking && bAmbushDone;
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
