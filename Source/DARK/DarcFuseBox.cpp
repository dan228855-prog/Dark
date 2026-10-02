// DarcFuseBox.cpp
#include "DarcFuseBox.h"
#include "DarcAssetSettings.h"
#include "Components/StaticMeshComponent.h"
#include "DarcFuseItem.h"
#include "DarcGameState.h"
#include "DarcGameplayLibrary.h"
#include "Engine/StaticMesh.h"
#include "DarcPowerSubsystem.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ADarcFuseBox::ADarcFuseBox()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	// Индикаторы — простые фигуры движка, без коллизии; место задаёт LayoutIndicators.
	auto MakePart = [this](const TCHAR* Name, const TCHAR* Shape)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(RootComponent);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape)));
		return Part;
	};
	FuseIndicator = MakePart(TEXT("FuseIndicator"), TEXT("Cylinder"));
	Lever = MakePart(TEXT("Lever"), TEXT("Cube"));
	StatusLamp = MakePart(TEXT("StatusLamp"), TEXT("Sphere"));
}

void ADarcFuseBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcFuseBox, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME(ADarcFuseBox, bHasFuse);
	DOREPLIFETIME(ADarcFuseBox, bBreakerOn);
}

bool ADarcFuseBox::CanInteract_Implementation(AActor* Interactor) const
{
	return true; // рубильник можно дёргать всегда; без предохранителя он просто ничего не даст
}

void ADarcFuseBox::OnInteract_Implementation(AActor* Interactor)
{
	// Только сервер (вызов из InteractionComponent::Server_Interact).
	ADarcFuseItem* HeldFuse = Cast<ADarcFuseItem>(ACarryableItem::FindItemHeldBy(Interactor));

	if (!bHasFuse && HeldFuse)
	{
		// Предохранитель расходуется в любом случае: либо встал, либо сгорел.
		HeldFuse->ForceDrop();
		HeldFuse->Destroy();

		if (bBreakerOn)
		{
			// Не установлен: сгорел. Индикатор остаётся красным, предохранителя в щитке нет.
			HandleBlowFromMistake(Interactor);
			TellNearby(TEXT("Fuse_BlownBreakerOn"));
			return;
		}

		bHasFuse = true;
		if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
		{
			Memory->RecordInteraction(CircuitId, TEXT("FuseInserted"), Interactor);
		}
		NotifyStateChanged();
		// Понятная обратная связь: предохранитель исчез из рук, виден в щитке, загорелся
		// индикатор — и короткая строка, что он встал (рубильник ещё нужно включить).
		TellNearby(TEXT("Fuse_Installed"));
		return;
	}

	bBreakerOn = !bBreakerOn;
	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(CircuitId, bBreakerOn ? TEXT("BreakerOn") : TEXT("BreakerOff"), Interactor);
	}
	NotifyStateChanged();
}

void ADarcFuseBox::SpiritToggleBreaker(AActor* Spirit)
{
	if (!HasAuthority())
	{
		return;
	}

	bBreakerOn = !bBreakerOn;
	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(CircuitId, bBreakerOn ? TEXT("SpiritBreakerOn") : TEXT("SpiritBreakerOff"), Spirit);
	}
	NotifyStateChanged();
}

void ADarcFuseBox::HandleBlowFromMistake(AActor* Interactor)
{
	Multicast_FuseBlown();

	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(CircuitId, TEXT("FuseBlown"), Interactor);
		if (CollateralCircuits.Num() > 0)
		{
			// Последствие для кампании: в следующем брифинге — «плановое обслуживание сети».
			Memory->AddCampaignFact(TEXT("Slice.BlackoutHappened"));
		}
	}

	// Выбиваем соседние контуры: их предохранители сгорают тоже.
	for (TActorIterator<ADarcFuseBox> It(GetWorld()); It; ++It)
	{
		if (*It != this && CollateralCircuits.Contains(It->CircuitId))
		{
			It->BurnFuse();
		}
	}

	if (bAddTaskOnBlow)
	{
		if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
		{
			Tasks->AddEmergentTask(TaskOnBlow, NAME_None);
		}
	}

	NotifyStateChanged();
}

void ADarcFuseBox::BurnFuse()
{
	if (!HasAuthority() || !bHasFuse)
	{
		return;
	}

	bHasFuse = false;
	Multicast_FuseBlown();
	NotifyStateChanged();
}

void ADarcFuseBox::NotifyStateChanged()
{
	OnRep_State(); // на сервере OnRep сам не вызывается
	if (UDarcPowerSubsystem* Power = UDarcPowerSubsystem::GetPower(this))
	{
		Power->Recompute();
	}
}

void ADarcFuseBox::OnRep_State()
{
	if (HasActorBegunPlay())
	{
		UDarcAssetSettings::PlaySound(this, TEXT("BreakerSwitch"), GetActorLocation());
	}
	UpdateIndicators();
	OnStateChanged();
}

void ADarcFuseBox::TellNearby(const TCHAR* Key) const
{
	if (ADarcGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADarcGameState>() : nullptr)
	{
		GS->Say(FText::GetEmpty(), UDarcGameplayLibrary::UIText(Key), 3.5f, GetActorLocation(), 1200.f);
	}
}

void ADarcFuseBox::LayoutIndicators()
{
	// Лицевая сторона — локальная +X актора (щиток смотрит в коридор). Габариты — по модели.
	const FBox Box = Visual->Bounds.GetBox().TransformBy(GetActorTransform().Inverse());
	const FVector Center = Box.GetCenter();
	const FVector Extent = Box.GetExtent();
	const float Front = Box.Max.X + 1.5f;
	FuseIndicator->SetRelativeLocationAndRotation(FVector(Front, Center.Y - Extent.Y * 0.35f, Center.Z), FRotator(0.f, 0.f, 0.f));
	FuseIndicator->SetRelativeScale3D(FVector(0.05f, 0.05f, 0.12f));
	StatusLamp->SetRelativeLocation(FVector(Front, Center.Y + Extent.Y * 0.35f, Center.Z + Extent.Z * 0.6f));
	StatusLamp->SetRelativeScale3D(FVector(0.06f));
	Lever->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.2f));
}

void ADarcFuseBox::UpdateIndicators()
{
	if (!FuseIndicator || !Lever || !StatusLamp)
	{
		return;
	}
	// Предохранитель виден, только когда вставлен.
	FuseIndicator->SetVisibility(bHasFuse);
	FuseIndicator->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(this, FLinearColor(0.75f, 0.45f, 0.15f)));

	// Рычаг: вверх — включён, вниз — выключен.
	const FBox Box = Visual->Bounds.GetBox().TransformBy(GetActorTransform().Inverse());
	const FVector Center = Box.GetCenter();
	const FVector Extent = Box.GetExtent();
	Lever->SetRelativeLocationAndRotation(FVector(Box.Max.X + 4.f, Center.Y + Extent.Y * 0.35f, Center.Z - Extent.Z * 0.2f),
		FRotator(bBreakerOn ? 35.f : -35.f, 0.f, 0.f));
	Lever->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(this, FLinearColor(0.6f, 0.05f, 0.03f)));

	// Индикатор: зелёный — цепь запитана; жёлтый — предохранитель есть, рубильник выключен;
	// красный — предохранителя нет (или сгорел).
	const FLinearColor Color = IsSupplying() ? FLinearColor(0.1f, 1.f, 0.2f)
		: (bHasFuse ? FLinearColor(1.f, 0.65f, 0.05f) : FLinearColor(1.f, 0.05f, 0.03f));
	StatusLamp->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(this, Color, 8.f));
}

void ADarcFuseBox::Multicast_FuseBlown_Implementation()
{
	UDarcAssetSettings::PlaySound(this, TEXT("FuseBlow"), GetActorLocation());
	OnFuseBlownFX();
}

FText ADarcFuseBox::GetInteractionPrompt_Implementation() const
{
	if (!bHasFuse)
	{
		// Подсказка не знает, что в руках у игрока, поэтому общая: «Вставить предохранитель».
		return PromptInsertFuse;
	}
	return bBreakerOn ? PromptBreakerOff : PromptBreakerOn;
}

void ADarcFuseBox::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual); // модель — у каждой машины сама
	LayoutIndicators();
	UpdateIndicators();
}
