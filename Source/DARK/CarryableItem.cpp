// CarryableItem.cpp
#include "CarryableItem.h"
#include "DarcWorldMemorySubsystem.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"

ACarryableItem::ACarryableItem()
{
    bReplicates = true;
    // Позиция и привязка реплицируются с сервера. Без этого после «положить»
    // каждый клиент оставлял бы предмет там, где посчитал сам, и позиции расходились бы.
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = false;
}

void ACarryableItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
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

    if (ACharacter* Character = Cast<ACharacter>(Holder))
    {
        AttachToComponent(
            Character->GetMesh(),
            FAttachmentTransformRules::SnapToTargetIncludingScale,
            CarrySocketName
        );
    }
    else if (Holder && Holder->GetRootComponent())
    {
        // Слот/устройство: предмет встаёт в корень держателя (точка вставки).
        AttachToComponent(Holder->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    }
}

void ACarryableItem::DetachFromHolder()
{
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    SetActorEnableCollision(true);

    if (bRestorePhysicsOnDetach)
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
