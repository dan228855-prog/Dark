// DarcItemSlot.cpp
#include "DarcItemSlot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "CarryableItem.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ADarcItemSlot::ADarcItemSlot()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	// Только для взгляда: разъём может быть закреплён на физическом объекте (стойке) —
	// физическая коллизия разъёма толкала бы его собственного «носителя».
	Visual->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	InsertPoint = CreateDefaultSubobject<USceneComponent>(TEXT("InsertPoint"));
	InsertPoint->SetupAttachment(RootComponent);
	InsertPoint->ComponentTags.Add(TEXT("InsertPoint"));

	// Только для взгляда (канал Visibility), ни с чем не сталкивается.
	AimBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AimBox"));
	AimBox->SetupAttachment(RootComponent);
	AimBox->SetCollisionProfileName(TEXT("NoCollision"));
	AimBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AimBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	AimBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ADarcItemSlot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcItemSlot, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME(ADarcItemSlot, bAllowRemove);
}

void ADarcItemSlot::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual); // модель — у каждой машины сама

	// Точка вставки — центр лицевой стороны модели (лицо — локальная +X), зона прицеливания —
	// габариты модели с запасом. Считаем у каждой машины по фактической модели.
	const FBox Box = Visual->Bounds.GetBox().TransformBy(GetActorTransform().Inverse());
	InsertPoint->SetRelativeLocation(FVector(Box.Max.X, Box.GetCenter().Y, Box.GetCenter().Z));
	AimBox->SetRelativeLocation(Box.GetCenter());
	AimBox->SetBoxExtent(Box.GetExtent() + FVector(12.f));

	if (HasAuthority() && InitialItem && !InitialItem->CurrentHolder)
	{
		InitialItem->ServerTransferTo(this, nullptr);
	}
}

ACarryableItem* ADarcItemSlot::GetInsertedItem() const
{
	// Держатель предмета — сам слот; отдельного поля не храним, чтобы не было двух источников правды.
	return ACarryableItem::FindItemHeldBy(this);
}

bool ADarcItemSlot::Accepts(const ACarryableItem* Item) const
{
	return Item && (!AcceptedClass || Item->IsA(AcceptedClass));
}

bool ADarcItemSlot::CanInteract_Implementation(AActor* Interactor) const
{
	if (HasItem())
	{
		// Вынуть — только пустыми руками и если устройство разрешает.
		return bAllowRemove && ACarryableItem::FindItemHeldBy(Interactor) == nullptr;
	}
	return Accepts(ACarryableItem::FindItemHeldBy(Interactor));
}

void ADarcItemSlot::OnInteract_Implementation(AActor* Interactor)
{
	// Только сервер. Перепроверяем всё, что клиент видел в CanInteract.
	if (ACarryableItem* Inserted = GetInsertedItem())
	{
		if (!bAllowRemove || ACarryableItem::FindItemHeldBy(Interactor))
		{
			return;
		}
		Inserted->ServerTransferTo(Interactor, Interactor);
		CompleteTaskIfSet(TaskIdOnRemove, Interactor);
		OnSlotChanged.Broadcast(nullptr);
		return;
	}

	ACarryableItem* Held = ACarryableItem::FindItemHeldBy(Interactor);
	if (!Accepts(Held))
	{
		return;
	}

	Held->ServerTransferTo(this, Interactor);
	if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
	{
		Memory->RecordInteraction(GetFName(), TEXT("ItemInserted"), Interactor);
	}
	CompleteTaskIfSet(TaskIdOnInsert, Interactor);
	OnSlotChanged.Broadcast(Held);
}

void ADarcItemSlot::CompleteTaskIfSet(FName TaskId, AActor* ByActor)
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

FText ADarcItemSlot::GetInteractionPrompt_Implementation() const
{
	return HasItem() ? PromptRemove : PromptInsert;
}
