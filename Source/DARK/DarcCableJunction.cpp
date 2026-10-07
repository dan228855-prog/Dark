// DarcCableJunction.cpp
#include "DarcCableJunction.h"
#include "CarryableItem.h"
#include "DarcTool.h"
#include "DarcGameState.h"
#include "DarcGameplayLibrary.h"
#include "DarcPowerSubsystem.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ADarcCableJunction::ADarcCableJunction()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true; // только искры, пока линия повреждена
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	SparkLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("SparkLight"));
	SparkLight->SetupAttachment(RootComponent);
	SparkLight->SetMobility(EComponentMobility::Movable);
	SparkLight->SetLightColor(FLinearColor(0.55f, 0.75f, 1.f));
	SparkLight->SetAttenuationRadius(260.f);
	SparkLight->SetCastShadows(false);
	SparkLight->SetIntensityUnits(ELightUnits::Lumens);
	SparkLight->SetIntensity(0.f);
	SparkLight->SetVolumetricScatteringIntensity(0.f);
}

void ADarcCableJunction::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcCableJunction, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME(ADarcCableJunction, bDamaged);
}

void ADarcCableJunction::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual);
	// Искры — в центре коробки: лицевая сторона у моделей разная, а тени свет не отбрасывает,
	// так что из центра он светит наружу во все стороны.
	SparkLight->SetWorldLocation(Visual->Bounds.Origin);
	OnRep_Damaged();
}

void ADarcCableJunction::OnRep_Damaged()
{
	SetActorTickEnabled(bDamaged);
	if (!bDamaged)
	{
		SparkLight->SetIntensity(0.f);
	}
}

void ADarcCableJunction::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Искрит, только пока на линии есть напряжение до обрыва (иначе просто мёртвый провод).
	// У клиента источник не известен — искрим всегда, когда повреждено: это подсказка «здесь».
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bFlash = FMath::FRand() < 0.18f;
	SparkLight->SetIntensity(bFlash ? FMath::FRandRange(200.f, 900.f) : 0.f);
	if (Now >= NextCrackle)
	{
		NextCrackle = Now + FMath::FRandRange(1.2f, 4.f);
		UDarcAssetSettings::PlaySound(this, TEXT("Sparks"), SparkLight->GetComponentLocation(), 0.45f);
	}
}

void ADarcCableJunction::ServerDamage(AActor* ByActor)
{
	if (!HasAuthority() || bDamaged)
	{
		return;
	}
	bDamaged = true;
	RepairLeft = FMath::Max(RepairSteps, 1);
	OnRep_Damaged();
	Multicast_SparkBurst();

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(CircuitId, TEXT("CableTorn"), ByActor);
	}
	if (!TaskIdOnDamage.IsNone())
	{
		if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
		{
			FDarcTaskDefinition Task;
			Task.TaskId = TaskIdOnDamage;
			Task.Title = TaskTitle;
			Task.bRequired = false;
			Tasks->AddEmergentTask(Task, BlocksTaskId);
		}
	}
	if (UDarcPowerSubsystem* Power = UDarcPowerSubsystem::GetPower(this))
	{
		Power->Recompute();
	}
}

void ADarcCableJunction::Multicast_SparkBurst_Implementation()
{
	UDarcAssetSettings::PlaySound(this, TEXT("FuseBlow"), GetActorLocation());
}

void ADarcCableJunction::Tell(const FText& Line) const
{
	if (ADarcGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADarcGameState>() : nullptr)
	{
		GS->Say(FText::GetEmpty(), Line, 4.f, GetActorLocation(), 1200.f);
	}
}

bool ADarcCableJunction::CanInteract_Implementation(AActor* Interactor) const
{
	return true;
}

void ADarcCableJunction::OnInteract_Implementation(AActor* Interactor)
{
	// Только сервер.
	const EDarcToolType Tool = ADarcTool::GetHeldTool(Interactor);
	if (Tool == EDarcToolType::Tester)
	{
		Tell(MakeTesterReport(this, CircuitId, bDamaged));
		return;
	}
	if (!bDamaged)
	{
		Tell(UDarcGameplayLibrary::UIText(TEXT("Junction_Intact")));
		return;
	}
	if (Tool == EDarcToolType::Wrench)
	{
		UDarcAssetSettings::PlaySound(this, TEXT("CableAttach"), GetActorLocation()); // у сервера; остальным — через смену состояния
		if (--RepairLeft > 0)
		{
			Tell(FText::Format(UDarcGameplayLibrary::UIText(TEXT("Junction_RepairStep")), RepairLeft));
			return;
		}
		bDamaged = false;
		OnRep_Damaged();
		Tell(UDarcGameplayLibrary::UIText(TEXT("Junction_Repaired")));
		if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
		{
			Memory->RecordInteraction(CircuitId, TEXT("CableRepaired"), Interactor);
		}
		if (!TaskIdOnDamage.IsNone())
		{
			if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
			{
				Tasks->CompleteTask(TaskIdOnDamage, Interactor);
			}
		}
		if (UDarcPowerSubsystem* Power = UDarcPowerSubsystem::GetPower(this))
		{
			Power->Recompute();
		}
		return;
	}

	// Не тем инструментом / голой рукой. Под напряжением — бьёт током.
	const UDarcPowerSubsystem* Power = UDarcPowerSubsystem::GetPower(this);
	if (Power && Power->GetRawCircuitSource(CircuitId) != EDarcPowerSource::None)
	{
		Multicast_SparkBurst();
		ShockPlayer(Interactor, this, ShockDamage, TEXT("Junction_Shock"));
		return;
	}
	Tell(UDarcGameplayLibrary::UIText(TEXT("Junction_NeedWrench")));
}

FText ADarcCableJunction::GetInteractionPrompt_Implementation() const
{
	if (ADarcTool::LocalPlayerHolds(this, EDarcToolType::Tester))
	{
		return UDarcGameplayLibrary::UIText(TEXT("Tester_Check"));
	}
	if (!bDamaged)
	{
		return UDarcGameplayLibrary::UIText(TEXT("Junction_PromptIntact"));
	}
	return ADarcTool::LocalPlayerHolds(this, EDarcToolType::Wrench)
		? UDarcGameplayLibrary::UIText(TEXT("Junction_PromptRepair"))
		: UDarcGameplayLibrary::UIText(TEXT("Junction_PromptDamaged"));
}

void ADarcCableJunction::ShockPlayer(AActor* Victim, AActor* Source, float Damage, const TCHAR* MessageKey)
{
	if (!Victim || !Victim->HasAuthority() || !Source)
	{
		return;
	}
	// Удар отбрасывает от источника, предмет из руки выпадает.
	if (ACarryableItem* Held = ACarryableItem::FindItemHeldBy(Victim))
	{
		Held->ForceDrop();
	}
	if (ACharacter* Character = Cast<ACharacter>(Victim))
	{
		const FVector Away = (Victim->GetActorLocation() - Source->GetActorLocation()).GetSafeNormal2D();
		Character->LaunchCharacter(Away * 380.f + FVector(0.f, 0.f, 160.f), true, true);
	}
	UGameplayStatics::ApplyDamage(Victim, Damage, nullptr, Source, UDamageType::StaticClass());

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(Source))
	{
		Memory->RecordInteraction(Source->GetFName(), TEXT("ElectricShock"), Victim);
	}
	if (ADarcGameState* GS = Source->GetWorld()->GetGameState<ADarcGameState>())
	{
		GS->Say(FText::GetEmpty(), UDarcGameplayLibrary::UIText(MessageKey), 4.f, Source->GetActorLocation(), 1200.f);
	}
}

FText ADarcCableJunction::MakeTesterReport(const UObject* WorldContextObject, FName Circuit, bool bLineDamaged)
{
	const UDarcPowerSubsystem* Power = UDarcPowerSubsystem::GetPower(WorldContextObject);
	const EDarcPowerSource Raw = Power ? Power->GetRawCircuitSource(Circuit) : EDarcPowerSource::None;
	const bool bCut = bLineDamaged || (Power && Power->IsCircuitCut(Circuit));
	const TCHAR* StateKey = TEXT("Tester_None");
	if (bCut)
	{
		StateKey = Raw != EDarcPowerSource::None ? TEXT("Tester_CutLive") : TEXT("Tester_CutDead");
	}
	else if (Raw == EDarcPowerSource::Mains)
	{
		StateKey = TEXT("Tester_Mains");
	}
	else if (Raw == EDarcPowerSource::Generator)
	{
		StateKey = TEXT("Tester_Generator");
	}
	return FText::Format(UDarcGameplayLibrary::UIText(TEXT("Tester_Report")),
		UDarcGameplayLibrary::UIText(*(TEXT("Circuit_") + Circuit.ToString())), UDarcGameplayLibrary::UIText(StateKey));
}
