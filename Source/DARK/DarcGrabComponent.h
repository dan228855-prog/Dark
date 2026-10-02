// DarcGrabComponent.h
// Физический захват предметов (основа «физики как геймплея» из мастер-документа).
// Зажал ЛКМ на физическом предмете — он тянется к точке перед камерой; отпустил — упал;
// ПКМ во время захвата — бросок.
//
// Как это устроено, чтобы кооп работал «сам»:
// - Сила, с которой игрок тянет, ОГРАНИЧЕНА (MaxForce). Лёгкое летает за взглядом, тяжёлое
//   один игрок не сдвинет — а двое тянут каждый своей силой, и силы складываются.
//   Отдельной логики «нужно N игроков» нет — это просто физика.
// - Соло: если жив один игрок, сила умножается на SoloForceMultiplier, иначе тяжёлое
//   было бы непроходимо (правило проекта: у любой кооп-механики есть соло-вариант).
//
// Сеть: решает и толкает только сервер (Server RPC с проверкой дистанции и цели),
// предмет реплицирует позицию сам (ReplicateMovement). Клиент только просит и рисует подсказку.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarcGrabComponent.generated.h"

class UPrimitiveComponent;

UCLASS(ClassGroup = (DARC), meta = (BlueprintSpawnableComponent))
class DARK_API UDarcGrabComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarcGrabComponent();

	/** Дальность захвата, см. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float GrabRange = 240.f;

	/** Максимальная сила одного игрока, кг·см/с². ~40 кг поднять можно, ~80 кг — только вдвоём. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float MaxForce = 40000.f;

	/** Во сколько раз сильнее игрок, если он остался один. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float SoloForceMultiplier = 2.f;

	/** Жёсткость и демпфирование «пружины» захвата (на единицу массы). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float Stiffness = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float Damping = 14.f;

	/** Если предмет застрял дальше этого от точки захвата — захват срывается, см. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float BreakDistance = 450.f;

	/**
	 * «Привязь»: дальше этого (по горизонтали, от игрока до ближайшего края предмета) игрок
	 * не отойдёт, пока держит предмет, — ~3 шага. Тяжёлое не «тянется резиной» за убегающим
	 * игроком: игрок упирается и тянет, предмет идёт с той скоростью, на какую хватает сил.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float TetherLength = 220.f;

	/** Скорость броска (изменение скорости), см/с — тяжёлое бросается слабее (ограничено силой). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab")
	float ThrowSpeed = 900.f;

	/** Что сейчас держу (реплицируется — для подсказок и анимации). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Grab")
	TObjectPtr<AActor> GrabbedActor;

	/** Клиент: физический предмет под прицелом, который можно схватить (для HUD). */
	UPROPERTY(BlueprintReadOnly, Category = "Grab")
	TObjectPtr<AActor> FocusedGrabbable;

	// --- Ввод (локальный клиент) ---
	UFUNCTION(BlueprintCallable, Category = "Grab")
	void StartGrab();

	UFUNCTION(BlueprintCallable, Category = "Grab")
	void StopGrab();

	UFUNCTION(BlueprintCallable, Category = "Grab")
	void Throw();

	UFUNCTION(BlueprintPure, Category = "Grab")
	bool IsHolding() const { return GrabbedActor != nullptr; }

	/** Можно ли схватить этот компонент (физика включена, предмет сетевой). */
	static bool IsGrabbable(const UPrimitiveComponent* Component);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Server, Reliable)
	void Server_Grab(AActor* Target, FVector_NetQuantize HitLocation);

	UFUNCTION(Server, Reliable)
	void Server_Release();

	UFUNCTION(Server, Reliable)
	void Server_Throw();

	bool GetEyes(FVector& OutLocation, FVector& OutForward) const;
	UPrimitiveComponent* TraceForGrabbable(FVector* OutHit = nullptr) const;
	void ServerTickHold(float DeltaTime);
	/** Не дать игроку отойти дальше TetherLength (локальный игрок и сервер — одинаково). */
	void ApplyTether(float DeltaTime);
	void ReleaseInternal();
	float GetEffectiveMaxForce() const;

	/** Сервер: что держим и за какую точку (в локальных координатах тела). */
	TWeakObjectPtr<UPrimitiveComponent> HeldComponent;
	FVector LocalGrabPoint = FVector::ZeroVector;
	float HoldDistance = 150.f;
	float SavedAngularDamping = 0.f;
};
