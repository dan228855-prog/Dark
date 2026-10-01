// DarcDataTransferStation.h
// Передача данных с сервера на носитель (сцена 7 среза).
// Состояния: нет питания → «EXTERNAL STORAGE NOT READY» (нет интерфейсного модуля) →
// «TRANSFER READY» → передача → пауза «CHECKING DATA...» → «TRANSFER COMPLETE».
// Пропало питание во время передачи — передача встаёт и продолжится с того же места,
// когда питание вернут (ошибка → новая проблема, а не провал).
// Тексты статусов на экране — в Blueprint из String Table по состоянию.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcPowerSubsystem.h"
#include "DarcAssetSettings.h"
#include "DarcDataTransferStation.generated.h"

class ADarcItemSlot;
class ACarryableItem;
class UDarcPowerConsumerComponent;

UENUM(BlueprintType)
enum class EDarcTransferState : uint8
{
	NoPower         UMETA(DisplayName = "Нет питания"),
	StorageNotReady UMETA(DisplayName = "Носитель не готов (нет модуля)"),
	Ready           UMETA(DisplayName = "Готов к передаче"),
	Transferring    UMETA(DisplayName = "Передача"),
	Checking        UMETA(DisplayName = "Проверка данных"),
	Complete        UMETA(DisplayName = "Передача завершена")
};

class UStaticMeshComponent;

UCLASS()
class DARK_API ADarcDataTransferStation : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcDataTransferStation();

	/** Какая модель у объекта (реплицируется при появлении, применяется у каждого игрока). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
	FDarcVisualSpec VisualSpec;

	/** Видимая модель (по умолчанию серая коробка, см. DarcAssetSettings). Нужна и для трейса взгляда. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Питание сервера (контур задаётся в компоненте, например "Server"). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Transfer")
	TObjectPtr<UDarcPowerConsumerComponent> Power;

	/** Слот интерфейсного модуля. Без модуля — «EXTERNAL STORAGE NOT READY». */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transfer")
	TObjectPtr<ADarcItemSlot> InterfaceSlot;

	/** Слот носителя. Вынуть носитель можно только после завершения передачи. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transfer")
	TObjectPtr<ADarcItemSlot> DriveSlot;

	/** Чистое время передачи без паузы, сек. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transfer", meta = (ClampMin = "1"))
	float TransferSeconds = 40.f;

	/** На каком проценте передача «задумывается». */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transfer", meta = (ClampMin = "0", ClampMax = "100"))
	float PauseAtPercent = 94.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transfer", meta = (ClampMin = "0"))
	float PauseSeconds = 6.f;

	/** Задача по завершении передачи (CopyArchive). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transfer|Tasks")
	FName TaskIdOnComplete;

	/**
	 * Постоянная аномалия среза: во время передачи моргает свет соседнего контура
	 * и (в Blueprint) шумит динамик. Пусто — без аномалии.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transfer|Anomaly")
	FName AnomalyFlickerCircuit = TEXT("Corridor");

	/** Через сколько секунд после старта передачи моргнуть. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transfer|Anomaly", meta = (ClampMin = "0"))
	float AnomalyDelay = 12.f;

	UPROPERTY(ReplicatedUsing = OnRep_Transfer, BlueprintReadOnly, Category = "Transfer")
	EDarcTransferState State = EDarcTransferState::NoPower;

	/** 0–100. Реплицируется ступенями по 1%, экран может сглаживать. */
	UPROPERTY(ReplicatedUsing = OnRep_Transfer, BlueprintReadOnly, Category = "Transfer")
	uint8 ProgressPercent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transfer|Text")
	FText PromptStart;

	// --- IInteractable ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void HandlePowerChanged(bool bPowered, EDarcPowerSource Source);

	UFUNCTION()
	void HandleSlotChanged(ACarryableItem* Item);

	UFUNCTION()
	void OnRep_Transfer();

	/** Экран сервера: показать состояние и процент (у всех клиентов). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Transfer")
	void OnTransferStateChanged(EDarcTransferState NewState, int32 Percent);

	/** Сервер: передача началась — место для сценарных событий в Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Transfer")
	void OnTransferStartedServer(AActor* StartedBy);

	/** Сервер: передача завершена (например, решить про «лишний файл»). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Transfer")
	void OnTransferCompletedServer();

	void UpdateIdleState();
	void SetState(EDarcTransferState NewState);
	void TickTransfer();
	void FinishPause();
	void FireAnomaly();

	FTimerHandle TransferTimer;
	FTimerHandle PauseTimer;
	FTimerHandle AnomalyTimer;

	/** Точный прогресс на сервере (ProgressPercent — округлённый для сети). */
	float Progress = 0.f;
	bool bPauseDone = false;
	bool bAnomalyDone = false;
	bool bWasStarted = false;

	static constexpr float TickInterval = 0.25f;

	/** Последнее показанное состояние — чтобы звук играл на переход, а не на каждый процент. */
	EDarcTransferState LastShownState = EDarcTransferState::NoPower;
};
