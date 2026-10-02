// CarryableItem.cpp
#include "CarryableItem.h"
#include "DarcWorldMemorySubsystem.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DarcAssetSettings.h"
#include "EngineUtils.h"

ACarryableItem::ACarryableItem()
{
    bReplicates = true;
    // Позиция и привязка реплицируются с сервера. Без этого после «положить»
    // каждый клиент оставлял бы предмет там, где посчитал сам, и позиции расходились бы.
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    RootComponent = Mesh;
}

void ACarryableItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ACarryableItem, VisualSpec, COND_InitialOnly);
    DOREPLIFETIME(ACarryableItem, CurrentHolder);
}

bool ACarryableItem::CanInteract_Implementation(AActor* Interactor) const
{
    // Свой предмет можно положить обратно.
    if (CurrentHolder == Interactor)
    {
        return true;
    }
    // Поднять можно только свободный предмет и только пустыми руками — два предмета сразу не носим.
    return CurrentHolder == nullptr && FindItemHeldBy(Interactor) == nullptr;
}

void ACarryableItem::OnInteract_Implementation(AActor* Interactor)
{
    // Выполняется только на сервере.
    if (CurrentHolder == Interactor)
    {
        ServerRelease(Interactor);
        return;
    }

    if (CurrentHolder == nullptr && FindItemHeldBy(Interactor) == nullptr)
    {
        ServerTransferTo(Interactor, Interactor);
    }
}

void ACarryableItem::ServerTransferTo(AActor* NewHolder, AActor* ByActor)
{
    if (!HasAuthority() || NewHolder == CurrentHolder)
    {
        return;
    }
    if (!NewHolder)
    {
        ServerRelease(ByActor);
        return;
    }

    // Исходное место запоминаем, только когда предмет поднимают с пола.
    if (!CurrentHolder)
    {
        PickUpTransform = GetActorTransform();
    }

    CurrentHolder = NewHolder;
    OnRep_Holder(); // на сервере OnRep не срабатывает сам - дергаем вручную для консистентности
}

void ACarryableItem::ServerRelease(AActor* ByActor)
{
    if (!HasAuthority() || !CurrentHolder)
    {
        return;
    }

    DetachFromHolder();
    CurrentHolder = nullptr;
    OnRep_Holder();

    if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
    {
        Memory->RecordItemMoved(GetMemoryId(), PickUpTransform, GetActorTransform(), ByActor);
    }
}

FText ACarryableItem::GetInteractionPrompt_Implementation() const
{
    return CurrentHolder ? PromptDrop : PromptPickUp;
}

void ACarryableItem::ForceDrop()
{
    // Публичный метод для сценариев вроде "игрок потерял сознание - предмет падает".
    // Только сервер: владение предметом — серверное состояние.
    ServerRelease(CurrentHolder);
}

void ACarryableItem::AttachToHolder(AActor* Holder)
{
    // Пока предмет в руке или в слоте — без физики и без коллизии: иначе физическое тело
    // оторвётся от руки, а трейс взгляда упрётся в предмет вместо слота под ним.
    if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(GetRootComponent()))
    {
        if (Body->IsSimulatingPhysics())
        {
            bRestorePhysicsOnDetach = true;
            Body->SetSimulatePhysics(false);
        }
    }
    SetActorEnableCollision(false);

    ACharacter* Character = Cast<ACharacter>(Holder);
    if (Character && Character->IsLocallyControlled())
    {
        // Свой персонаж: модель тела у себя не видна, поэтому держим предмет перед камерой —
        // «в руке» в нижней правой части экрана. У остальных игроков — в руке модели (ниже).
        if (UCameraComponent* Camera = Character->FindComponentByClass<UCameraComponent>())
        {
            AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            SetActorRelativeLocation(FirstPersonHoldOffset);
            SetActorRelativeRotation(FirstPersonHoldRotation);
            return;
        }
    }
    if (Character && Character->GetMesh())
    {
        // Первый существующий сокет/кость руки: свой, шаблонный HandGrip_R, кость hand_r.
        for (const FName Socket : { CarrySocketName, FName(TEXT("HandGrip_R")), FName(TEXT("hand_r")) })
        {
            if (!Socket.IsNone() && Character->GetMesh()->DoesSocketExist(Socket))
            {
                AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
                return;
            }
        }
    }
    if (Character)
    {
        // Ни сокета, ни камеры (серые коробки) — держим перед собой, чтобы было видно.
        AttachToComponent(Character->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        SetActorRelativeLocation(HoldOffset);
    }
    else if (Holder && Holder->GetRootComponent())
    {
        // Слот/устройство: предмет встаёт в корень держателя (точка вставки).
        AttachToComponent(Holder->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    }
}

void ACarryableItem::OnRep_AttachmentReplication()
{
    // Предмет в руке игрока крепится у каждой машины по-своему (у себя — к камере, у других —
    // к руке модели), поэтому серверную привязку к персонажу не применяем — это делает OnRep_Holder.
    if (Cast<APawn>(GetAttachmentReplication().AttachParent) || Cast<APawn>(CurrentHolder))
    {
        return;
    }
    Super::OnRep_AttachmentReplication();
}

void ACarryableItem::DetachFromHolder()
{
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    SetActorEnableCollision(true);

    if (bRestorePhysicsOnDetach || bPhysicsWhenFree)
    {
        bRestorePhysicsOnDetach = false;
        if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(GetRootComponent()))
        {
            Body->SetSimulatePhysics(true);
        }
    }
}

void ACarryableItem::OnRep_Holder()
{
    // Вызывается на клиентах при получении новой реплики CurrentHolder,
    // и вручную на сервере из OnInteract_Implementation.
    if (CurrentHolder)
    {
        AttachToHolder(CurrentHolder);
    }
    else
    {
        DetachFromHolder();
    }

    UDarcAssetSettings::PlaySound(this, CurrentHolder ? TEXT("PickUp") : TEXT("Drop"), GetActorLocation());
    OnHolderChanged(CurrentHolder);
}

ACarryableItem* ACarryableItem::FindItemHeldBy(const AActor* Holder)
{
    if (!Holder || !Holder->GetWorld())
    {
        return nullptr;
    }

    for (TActorIterator<ACarryableItem> It(Holder->GetWorld()); It; ++It)
    {
        if (It->CurrentHolder == Holder)
        {
            return *It;
        }
    }
    return nullptr;
}

void ACarryableItem::BeginPlay()
{
    Super::BeginPlay();
    VisualSpec.ApplyTo(Mesh); // модель — у каждой машины сама
    UDarcAssetSettings::EnsurePhysicsCollision(Mesh, VisualSpec);
    if (bPhysicsWhenFree && !CurrentHolder && Mesh->GetStaticMesh())
    {
        Mesh->SetSimulatePhysics(true);
    }
}
