// DarcCableJunction.h
// Кабельная линия контура (распаечная коробка на стене). Пока цела — ничего не делает.
// Повреждена (например, дверь взломали ломом, а кабель шёл за ней) — контур обесточен,
// хоть от щита, хоть от генератора, из коробки сыплются искры.
//   Ключ   — починить (несколько нажатий, по винту клеммы).
//   Тестер — состояние линии: повреждена/цела, есть ли напряжение до обрыва.
//   Голой рукой (или чем-то не тем) по повреждённой линии под напряжением — удар током.
// Это и есть «ошибка → новая проблема»: быстрый путь через дверь стоит починки сервера.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcAssetSettings.h"
#include "DarcCableJunction.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class DARK_API ADarcCableJunction : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcCableJunction();

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
	FDarcVisualSpec VisualSpec;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Искры — мигающий свет у коробки (у каждой машины, пока линия повреждена). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UPointLightComponent> SparkLight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	FName CircuitId;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Damaged, BlueprintReadOnly, Category = "Power")
	bool bDamaged = false;

	/** Сколько нажатий ключом нужно на починку. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	int32 RepairSteps = 3;

	/** Урон от удара током, ед. здоровья. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	float ShockDamage = 20.f;

	/** Задача «починить линию», появляется при повреждении (пусто — без задачи). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Tasks")
	FName TaskIdOnDamage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Tasks")
	FText TaskTitle;

	/** Какую задачу блокирует обрыв, пока не починят (например, ConnectInterface). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power|Tasks")
	FName BlocksTaskId;

	/** Сервер: линия повреждена (взлом двери, событие). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Power")
	void ServerDamage(AActor* ByActor);

	/** Сервер: удар током игрока — урон, предмет из руки падает, искры и сообщение рядом. */
	static void ShockPlayer(AActor* Victim, AActor* Source, float Damage, const TCHAR* MessageKey);

	/** Сервер: строка «что показал тестер» для контура (общая для щитков и линий). */
	static FText MakeTesterReport(const UObject* WorldContextObject, FName Circuit, bool bLineDamaged);

	// --- IInteractable ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Damaged();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SparkBurst();

	void Tell(const FText& Line) const;

	int32 RepairLeft = 0;
	float NextCrackle = 0.f;
};
