// CarryableItem.h
// Переносимый объект (Item / Carry System, P0). Игрок подбирает предмет,
// он "прикрепляется" к руке/сокету, сервер авторитетен по владению.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcAssetSettings.h"
#include "CarryableItem.generated.h"

class UStaticMeshComponent;

UCLASS()
class DARK_API ACarryableItem : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    ACarryableItem();

    /** Какая модель у объекта (реплицируется при появлении, применяется у каждого игрока). */
    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
    FDarcVisualSpec VisualSpec;

    // Тело предмета (корень). Модель — из DarcAssetSettings или задаётся в Blueprint.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carry")
    TObjectPtr<UStaticMeshComponent> Mesh;

    // Кто сейчас держит предмет. nullptr, если предмет лежит на земле.
    // Реплицируется, чтобы все клиенты видели один и тот же предмет "в руке" одного игрока.
    UPROPERTY(ReplicatedUsing = OnRep_Holder, BlueprintReadOnly, Category = "Carry")
    AActor* CurrentHolder = nullptr;

    // Предмет убран в инвентарь игрока (слоты 1–4): не в руке, скрыт, без коллизии, едет с игроком.
    // CurrentHolder при этом пуст — механики «что в руках» убранный предмет не видят.
    UPROPERTY(ReplicatedUsing = OnRep_Stowed, BlueprintReadOnly, Category = "Carry")
    AActor* StowedBy = nullptr;

    // Можно ли убрать предмет в инвентарь (вилка на проводе — нельзя).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
    bool bCanStow = true;

    // Название в слоте инвентаря (ключ String Table из разметки Name=...).
    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Carry|Text")
    FText ItemName;

    // Сервер: убрать предмет из руки держателя в инвентарь / достать обратно в руку / выбросить из инвентаря.
    void ServerStow(AActor* ToActor);
    void ServerUnstowTo(AActor* ToActor);
    void ServerDropFromStow(AActor* ByActor);

    // Имя сокета на скелете игрока, куда крепится предмет при переноске.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
    FName CarrySocketName = TEXT("hand_r_socket");

    // Свободно лежащий предмет — с физикой (падает, его можно толкнуть).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
    bool bPhysicsWhenFree = true;

    // Если сокета нет — где держать предмет относительно персонажа (вперёд, вправо, вверх), см.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
    FVector HoldOffset = FVector(45.f, 18.f, 30.f);

    // Как предмет виден своему игроку: смещение от камеры (вперёд, вправо, вверх), см, и поворот.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
    FVector FirstPersonHoldOffset = FVector(32.f, 16.f, -14.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
    FRotator FirstPersonHoldRotation = FRotator(0.f, -15.f, 0.f);

    // В руке от первого лица (сокет HandGrip_R рук): смещение и поворот относительно хвата.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
    FVector FirstPersonGripOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
    FRotator FirstPersonGripRotation = FRotator::ZeroRotator;

    // Хват в осях кости ладони: FirstPersonGripOffset/Rotation — положение и поворот относительно
    // кости, а не от взгляда в момент подбора (карту выдают, пока игрок в машине, — вид был другой).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
    bool bGripInBoneSpace = false;

    // ID предмета для WorldMemory. Если пусто — имя актора в уровне.
    // Процедурный генератор и заспавненные в рантайме предметы обязаны задавать его явно.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
    FName MemoryId;

    // Тексты подсказок — из String Table (ST_UI: Carry_PickUp / Carry_Drop), не из C++.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry|Text")
    FText PromptPickUp;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry|Text")
    FText PromptDrop;

    UFUNCTION(BlueprintPure, Category = "Carry")
    FName GetMemoryId() const { return MemoryId.IsNone() ? GetFName() : MemoryId; }

    // Игрок вызывает это же взаимодействие и на подбор, и на выброс (переключение).
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual void OnInteract_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionPrompt_Implementation() const override;

    // Что сейчас держит этот игрок (nullptr — ничего). Для механик «вставить то, что в руках».
    UFUNCTION(BlueprintPure, Category = "Carry", meta = (DefaultToSelf = "Holder"))
    static ACarryableItem* FindItemHeldBy(const AActor* Holder);

    // Сервер: передать предмет новому держателю — игроку или слоту/устройству (nullptr = положить).
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Carry")
    void ServerTransferTo(AActor* NewHolder, AActor* ByActor);

    // Явный сброс предмета (например, если игрок погиб/потерял сознание - см. решение про "дух").
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Carry")
    void ForceDrop();

    // Сервер: вытолкнуть предмет из устройства в мир (физический предмет в точке Location со
    // скоростью Velocity) — например, носитель из разъёма после копирования.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Carry")
    void ServerEjectTo(const FVector& Location, const FVector& Velocity, AActor* ByActor);

protected:
    virtual void BeginPlay() override;
    virtual void OnRep_AttachmentReplication() override;

    /** Рисовать предмет вместе с руками от первого лица (в руке у своего игрока) или в мире. */
    void SetFirstPersonRendering(bool bFirstPerson);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_Holder();

    UFUNCTION()
    void OnRep_Stowed();

    /** Убранный предмет: спрятать, выключить физику/коллизию, прицепить к игроку. */
    void ApplyStowedState();

    // Сюда в Blueprint добавляете физическую attach-логику / выключение физики при подборе и т.п.
    UFUNCTION(BlueprintImplementableEvent, Category = "Carry")
    void OnHolderChanged(AActor* NewHolder);

    void AttachToHolder(AActor* Holder);
    void DetachFromHolder();

    // Предмет был физическим до того, как его взяли, — вернуть физику при отпускании.
    bool bRestorePhysicsOnDetach = false;

    // Где предмет лежал до того, как его подняли, — для записи перемещения в WorldMemory.
    FTransform PickUpTransform;

    // Отпустить предмет на сервере и записать перемещение в память мира.
    // Из руки игрока предмет выпадает перед ним (с физикой).
    void ServerRelease(AActor* ByActor);

    // Поставить предмет перед игроком (на полшага, чуть выше пояса, не в стену).
    void PlaceInFrontOf(const APawn* Pawn);

    // Сервер, раз в секунду: свободный предмет провалился под карту — вернуть на место.
    void CheckFellThrough();

    // Где предмет точно лежал на полу (начало уровня / последнее место, откуда его подняли).
    FTransform SafeTransform;
    FTimerHandle FallCheckTimer;
    static constexpr float FellThroughZ = -300.f;
};
