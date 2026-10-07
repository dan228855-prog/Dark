// CarryableItem.cpp
#include "CarryableItem.h"
#include "DarcWorldMemorySubsystem.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "DARKCharacter.h"
#include "Engine/StaticMesh.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DarcAssetSettings.h"
#include "EngineUtils.h"
#include "DarcGameplayLibrary.h"
#include "TimerManager.h"
#include "DarcInventoryComponent.h"

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
    DOREPLIFETIME(ACarryableItem, StowedBy);
    DOREPLIFETIME_CONDITION(ACarryableItem, ItemName, COND_InitialOnly);
}

bool ACarryableItem::CanInteract_Implementation(AActor* Interactor) const
{
    // Убранный в инвентарь предмет скрыт — взаимодействовать не с чем.
    if (StowedBy)
    {
        return false;
    }
    // Свой предмет можно положить обратно.
    if (CurrentHolder == Interactor)
    {
        return true;
    }
    // Свободный предмет можно поднять и с занятыми руками — тогда он заменит то, что в руках.
    return CurrentHolder == nullptr;
}

void ACarryableItem::OnInteract_Implementation(AActor* Interactor)
{
    // Выполняется только на сервере.
    if (CurrentHolder == Interactor)
    {
        ServerRelease(Interactor);
        return;
    }
    if (CurrentHolder != nullptr)
    {
        return;
    }

    if (StowedBy)
    {
        return;
    }

    // Руки заняты — старый предмет уходит в свободный слот инвентаря, а если слотов нет —
    // выпадает перед игроком (с физикой). Два предмета в руке сразу не носим.
    UDarcInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UDarcInventoryComponent>() : nullptr;
    if (Inventory)
    {
        Inventory->ServerPrepareForPickup();
    }
    else if (ACarryableItem* Previous = FindItemHeldBy(Interactor); Previous && Previous != this)
    {
        Previous->ServerRelease(Interactor);
    }
    ServerTransferTo(Interactor, Interactor);
    if (Inventory)
    {
        Inventory->ServerNoteHeld(this);
    }
}

void ACarryableItem::ServerStow(AActor* ToActor)
{
    if (!HasAuthority() || !ToActor || CurrentHolder != ToActor || !bCanStow)
    {
        return;
    }
    DetachFromHolder();
    CurrentHolder = nullptr;
    StowedBy = ToActor;
    ApplyStowedState();
}

void ACarryableItem::ServerUnstowTo(AActor* ToActor)
{
    if (!HasAuthority() || !ToActor || StowedBy != ToActor)
    {
        return;
    }
    StowedBy = nullptr;
    SetActorHiddenInGame(false);
    CurrentHolder = ToActor;
    OnRep_Holder();
}

void ACarryableItem::ServerDropFromStow(AActor* ByActor)
{
    if (!HasAuthority() || !StowedBy)
    {
        return;
    }
    AActor* Previous = StowedBy;
    StowedBy = nullptr;
    SetActorHiddenInGame(false);
    DetachFromHolder();
    if (const APawn* Pawn = Cast<APawn>(Previous))
    {
        PlaceInFrontOf(Pawn);
    }
    if (UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this))
    {
        Memory->RecordItemMoved(GetMemoryId(), PickUpTransform, GetActorTransform(), ByActor ? ByActor : Previous);
    }
}

void ACarryableItem::ApplyStowedState()
{
    SetFirstPersonRendering(false);
    if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(GetRootComponent()))
    {
        Body->SetSimulatePhysics(false);
    }
    SetActorEnableCollision(false);
    if (StowedBy && StowedBy->GetRootComponent())
    {
        AttachToComponent(StowedBy->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    }
    SetActorHiddenInGame(true);
}

void ACarryableItem::OnRep_Stowed()
{
    if (StowedBy)
    {
        ApplyStowedState();
        return;
    }
    SetActorHiddenInGame(false);
    if (!CurrentHolder)
    {
        DetachFromHolder(); // выброшен из инвентаря — снова физический предмет в мире
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
        if (PickUpTransform.GetLocation().Z > FellThroughZ)
        {
            SafeTransform = PickUpTransform;
        }
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

    AActor* PreviousHolder = CurrentHolder;
    DetachFromHolder();
    CurrentHolder = nullptr;
    if (APawn* Pawn = Cast<APawn>(PreviousHolder))
    {
        // Из руки предмет выпадает перед игроком, а не там, где была кость руки: та может
        // оказаться в стене или у самого пола — тогда предмет проваливался сквозь него.
        PlaceInFrontOf(Pawn);
    }
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

void ACarryableItem::ServerEjectTo(const FVector& Location, const FVector& Velocity, AActor* ByActor)
{
    if (!HasAuthority() || !CurrentHolder)
    {
        return;
    }
    ServerRelease(ByActor ? ByActor : CurrentHolder);
    SetActorLocation(Location, false, nullptr, ETeleportType::ResetPhysics);
    if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(GetRootComponent()); Body && Body->IsSimulatingPhysics())
    {
        Body->SetPhysicsLinearVelocity(Velocity);
    }
}

void ACarryableItem::PlaceInFrontOf(const APawn* Pawn)
{
    if (!Pawn || !GetWorld())
    {
        return;
    }
    FVector Eyes;
    FRotator View;
    UDarcGameplayLibrary::GetAimViewPoint(Pawn, Eyes, View);
    const FRotator Facing(0.f, View.Yaw, 0.f);
    const FVector Forward = Facing.Vector();

    // Чуть выше пояса и на полшага вперёд; упёрлись в стену — оставляем перед ней.
    const FVector From = Pawn->GetActorLocation() + FVector(0.f, 0.f, 20.f);
    FVector Target = From + Forward * 45.f;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcDropItem), false, Pawn);
    Params.AddIgnoredActor(this);
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, From, Target, ECC_Visibility, Params))
    {
        Target = Hit.Location - Forward * 10.f;
    }
    SetActorLocationAndRotation(Target, Facing, false, nullptr, ETeleportType::ResetPhysics);
}

void ACarryableItem::CheckFellThrough()
{
    // Страховка (сервер): свободный предмет провалился под карту — возвращаем туда, где его
    // последний раз брали (или где он лежал с начала уровня). Ключевые предметы не теряются.
    if (CurrentHolder || StowedBy || GetActorLocation().Z > FellThroughZ)
    {
        return;
    }
    if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(GetRootComponent()))
    {
        Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
        Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }
    SetActorLocationAndRotation(SafeTransform.GetLocation() + FVector(0.f, 0.f, 10.f), SafeTransform.GetRotation(), false, nullptr, ETeleportType::ResetPhysics);
}

void ACarryableItem::AttachToHolder(AActor* Holder)
{
    // Предмет мог прийти из руки (вид от первого лица) — в слоте/у NPC он рисуется в мире.
    // Без этого вставленный модуль/вилка оставались «у лица» и ездили вместе с игроком.
    SetFirstPersonRendering(false);

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
        // Свой персонаж: модель тела у себя не видна — предмет в руке модели рук от первого
        // лица (сокет HandGrip_R, как оружие в шаблоне) и рисуется вместе с руками.
        const ADARKCharacter* FirstPersonCharacter = Cast<ADARKCharacter>(Character);
        USkeletalMeshComponent* Arms = FirstPersonCharacter ? FirstPersonCharacter->GetFirstPersonMesh() : nullptr;
        const UCameraComponent* Camera = Character->FindComponentByClass<UCameraComponent>();
        FName Palm = NAME_None;
        for (const FName Bone : { FName(TEXT("middle_metacarpal_r")), FName(TEXT("hand_r")), FName(TEXT("HandGrip_R")) })
        {
            if (Arms && Arms->DoesSocketExist(Bone))
            {
                Palm = Bone;
                break;
            }
        }
        if (Arms && Camera && !Palm.IsNone())
        {
            // В ладони (кость середины ладони), лицевой стороной (+Z предмета) — к глазам,
            // длинной стороной поперёк взгляда. Положение считаем один раз в мире от камеры и
            // запоминаем относительно кости: дальше предмет двигается вместе с рукой.
            AttachToComponent(Arms, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Palm);
            if (bGripInBoneSpace)
            {
                SetActorRelativeLocation(FirstPersonGripOffset);
                SetActorRelativeRotation(FirstPersonGripRotation);
                SetFirstPersonRendering(true);
                return;
            }
            const FVector Forward = Camera->GetForwardVector();
            const FVector Right = Camera->GetRightVector();
            const FVector Up = Camera->GetUpVector();
            const FVector PalmLocation = Arms->GetSocketLocation(Palm);
            // Поправки хвата (настраиваются у предмета: FirstPersonGripOffset — вперёд/вправо/вверх
            // по взгляду, см; FirstPersonGripRotation — доворот предмета).
            SetActorLocationAndRotation(PalmLocation + Forward * (FirstPersonGripOffset.X - 1.5f)
                + Right * FirstPersonGripOffset.Y + Up * (FirstPersonGripOffset.Z - 2.5f),
                FRotationMatrix::MakeFromZX(-Forward, Right).ToQuat() * FirstPersonGripRotation.Quaternion());
            SetFirstPersonRendering(true);
            return;
        }
        // Нет рук с сокетом — перед камерой, в нижней правой части экрана.
        if (UCameraComponent* CameraComp = Character->FindComponentByClass<UCameraComponent>())
        {
            AttachToComponent(CameraComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
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
        // Слот/устройство: точка вставки (компонент с тегом InsertPoint), иначе корень. Предмет
        // садится в неё ЦЕНТРОМ модели — раньше садился своей точкой опоры и висел рядом.
        USceneComponent* Point = Holder->FindComponentByTag<USceneComponent>(TEXT("InsertPoint"));
        AttachToComponent(Point ? Point : Holder->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        if (const UStaticMesh* StaticMesh = Mesh->GetStaticMesh())
        {
            const FVector Center = StaticMesh->GetBoundingBox().GetCenter() * Mesh->GetRelativeScale3D();
            SetActorRelativeLocation(-Center);
        }
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

void ACarryableItem::SetFirstPersonRendering(bool bFirstPerson)
{
    // Все видимые части предмета (модель И надписи на нём) — в одном пространстве с руками:
    // иначе модель рисуется «у лица», а надпись остаётся в мире и уползает в сторону.
    TArray<UPrimitiveComponent*> Parts;
    GetComponents<UPrimitiveComponent>(Parts);
    for (UPrimitiveComponent* Part : Parts)
    {
        // Части, стоящие в мире сами по себе (провод и розетка у вилки — абсолютные
        // координаты), остаются в мире: иначе розетка и провод рисовались «у лица» игрока.
        if (Part->IsUsingAbsoluteLocation())
        {
            continue;
        }
        Part->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
    }
}

void ACarryableItem::DetachFromHolder()
{
    SetFirstPersonRendering(false);
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
    // Убран в инвентарь (CurrentHolder опустел вместе с появлением StowedBy) — не «выпал».
    if (!CurrentHolder && StowedBy)
    {
        ApplyStowedState();
        return;
    }
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
    // Предмет поставлен на карту руками (не сборщиком) — подсказки из String Table по умолчанию.
    if (PromptPickUp.IsEmpty())
    {
        PromptPickUp = UDarcGameplayLibrary::UIText(TEXT("Carry_PickUp"));
    }
    if (PromptDrop.IsEmpty())
    {
        PromptDrop = UDarcGameplayLibrary::UIText(TEXT("Carry_Drop"));
    }
    VisualSpec.bLocalUV = true; // предмет двигается — текстура не должна «плыть» по нему
    VisualSpec.ApplyTo(Mesh); // модель — у каждой машины сама
    // Декали дороги/пола на переносимом предмете тоже «плывут».
    TArray<UPrimitiveComponent*> Parts;
    GetComponents<UPrimitiveComponent>(Parts);
    for (UPrimitiveComponent* Part : Parts)
    {
        Part->SetReceivesDecals(false);
    }
    UDarcAssetSettings::EnsurePhysicsCollision(Mesh, VisualSpec);

    // Мелкий предмет не сталкивается с персонажем: на него не встать, он не «везёт» игрока
    // и не отлетает от шагов (было: жёсткий диск крутил игрока как скейтборд и улетал).
    // Взгляд/взаимодействие (Visibility) и столкновения с миром остаются.
    Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Mesh->CanCharacterStepUpOn = ECB_No;
    Mesh->SetAngularDamping(2.f);
    Mesh->SetLinearDamping(0.3f);
    // Тонкие предметы (карта — полсантиметра) при падении «проскакивали» пол за один кадр
    // физики — непрерывная проверка столкновений это исключает.
    Mesh->SetUseCCD(true);
    if (bPhysicsWhenFree && !CurrentHolder && Mesh->GetStaticMesh())
    {
        Mesh->SetSimulatePhysics(true);
    }

    if (HasAuthority())
    {
        SafeTransform = GetActorTransform();
        GetWorldTimerManager().SetTimer(FallCheckTimer, this, &ACarryableItem::CheckFellThrough, 1.f, true, 1.f);
    }
}
