// InteractableDoor.cpp
#include "InteractableDoor.h"
#include "DarcWorldMemorySubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "DarcAssetSettings.h"

AInteractableDoor::AInteractableDoor()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;           // только для анимации створки
    PrimaryActorTick.bStartWithTickEnabled = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
    Hinge->SetupAttachment(RootComponent);
    Hinge->SetMobility(EComponentMobility::Movable);
    Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
    Panel->SetupAttachment(Hinge);
    Panel->SetMobility(EComponentMobility::Movable);
    Panel->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void AInteractableDoor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Чисто визуально и локально на каждой машине: состояние двери — реплицируемый bIsOpen.
    const float Target = bIsOpen ? OpenAngle : 0.f;
    const float Current = Hinge->GetRelativeRotation().Yaw;
    const float Next = FMath::FixedTurn(Current, Target, SwingSpeed * DeltaSeconds);
    Hinge->SetRelativeRotation(FRotator(0.f, Next, 0.f));
    if (FMath::IsNearlyEqual(Next, Target, 0.5f))
    {
        SetActorTickEnabled(false);
    }
}

void AInteractableDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(AInteractableDoor, VisualSpec, COND_InitialOnly);
    DOREPLIFETIME(AInteractableDoor, bIsOpen);
    DOREPLIFETIME(AInteractableDoor, bIsLocked);
}

bool AInteractableDoor::CanInteract_Implementation(AActor* Interactor) const
{
    // Взаимодействовать можно и с запертой: игрок видит «Заперто» и может дёрнуть ручку.
    // Открыть её — только альтернативными способами (ключ, код, отключение системы), они меняют bIsLocked.
    return true;
}

void AInteractableDoor::OnInteract_Implementation(AActor* Interactor)
{
    // Этот код выполняется ТОЛЬКО на сервере (вызывается из InteractionComponent::Server_Interact).
    if (bIsLocked)
    {
        Multicast_LockedAttempt(); // дёрнули ручку — звук «заперто» у всех рядом
        if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
        {
            Memory->RecordInteraction(GetMemoryId(), TEXT("LockedAttempt"), Interactor);
        }
        return;
    }

    SetDoorOpen(!bIsOpen, Interactor);
}

void AInteractableDoor::SetDoorOpen(bool bNewIsOpen, AActor* ByActor)
{
    if (!HasAuthority() || bIsOpen == bNewIsOpen)
    {
        return;
    }

    bIsOpen = bNewIsOpen;
    OnRep_IsOpen(); // на сервере OnRep не вызывается автоматически - дергаем сами для консистентности

    if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
    {
        Memory->RecordDoorState(GetMemoryId(), bIsOpen, ByActor);
    }
}

void AInteractableDoor::SetLocked(bool bNewLocked)
{
    if (HasAuthority())
    {
        bIsLocked = bNewLocked;
    }
}

FText AInteractableDoor::GetInteractionPrompt_Implementation() const
{
    return bIsLocked ? PromptLocked : (bIsOpen ? PromptClose : PromptOpen);
}

void AInteractableDoor::OnRep_IsOpen()
{
    // Вызывается на клиентах при получении новой реплики bIsOpen,
    // и вручную на сервере из SetDoorOpen.
    if (bNativeSwing)
    {
        SetActorTickEnabled(true);
    }
    if (HasActorBegunPlay())
    {
        UDarcAssetSettings::PlaySound(this, bIsOpen ? TEXT("DoorOpen") : TEXT("DoorClose"), GetActorLocation());
    }
    OnDoorStateChanged(bIsOpen);
}

void AInteractableDoor::Multicast_LockedAttempt_Implementation()
{
    UDarcAssetSettings::PlaySound(this, TEXT("DoorLocked"), GetActorLocation());
    OnLockedAttempt();
}

void AInteractableDoor::BeginPlay()
{
    Super::BeginPlay();
    VisualSpec.ApplyTo(Panel); // модель — у каждой машины сама
    if (bIsOpen && bNativeSwing)
    {
        SetActorTickEnabled(true); // дверь, открытая с самого начала, — довернуть створку
    }
}
