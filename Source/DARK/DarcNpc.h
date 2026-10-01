// DarcNpc.h
// Простой NPC без ИИ-поведения (в срезе сложных NPC нет): охранник КПП, куратор базы.
// Умеет: сказать приветствие, когда игрок подошёл; выдать предмет в руки (карта доступа);
// принять предмет из рук (носитель с архивом); после каждого — короткая серия реплик.
// Реплики — субтитры через ADarcGameState::Say, тексты из String Table, паузы между ними
// задаются в данных («Хорошо.» … пауза … «Тогда возвращайтесь.»).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcAssetSettings.h"
#include "DarcNpc.generated.h"

class ACarryableItem;
class USphereComponent;

USTRUCT(BlueprintType)
struct FDarcNpcLine
{
	GENERATED_BODY()

	/** Реплика — строка из String Table. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FText Text;

	/** Пауза перед репликой, сек. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC", meta = (ClampMin = "0"))
	float DelayBefore = 0.f;

	/** Сколько держать субтитр, сек. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC", meta = (ClampMin = "0.5"))
	float Duration = 3.f;
};

class UStaticMeshComponent;

UCLASS()
class DARK_API ADarcNpc : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcNpc();

	/** Какая модель у объекта (реплицируется при появлении, применяется у каждого игрока). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
	FDarcVisualSpec VisualSpec;

	/** Видимая модель (по умолчанию серая коробка, см. DarcAssetSettings). Нужна и для трейса взгляда. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Имя говорящего в субтитрах (String Table). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FText SpeakerName;

	/** Радиус слышимости реплик, см. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC", meta = (ClampMin = "100"))
	float HearingRadius = 1500.f;

	/** Зона, войдя в которую игрок слышит приветствие (один раз за выезд). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC")
	TObjectPtr<USphereComponent> GreetZone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Lines")
	TArray<FDarcNpcLine> GreetingLines;

	// --- Выдать предмет ---

	/** Предмет, который NPC отдаёт в руки при первом обращении (карта доступа). Ставится в уровень рядом. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Give")
	TObjectPtr<ACarryableItem> ItemToGive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Give")
	FName TaskIdOnGive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Lines")
	TArray<FDarcNpcLine> GiveLines;

	// --- Принять предмет ---

	/** Какой предмет NPC принимает из рук (класс носителя). Пусто — ничего не принимает. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Receive")
	TSubclassOf<ACarryableItem> AcceptedClass;

	/** Реплика, когда игрок подошёл с нужным предметом, но ещё не отдал (например, «Закончили?»). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Lines")
	TArray<FDarcNpcLine> BeforeReceiveLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Receive")
	FName TaskIdOnReceive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Lines")
	TArray<FDarcNpcLine> ReceiveLines;

	/** Ответ, если обратились без дела (после выдачи, без нужного предмета). Пусто — молчит. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Lines")
	TArray<FDarcNpcLine> IdleLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Text")
	FText PromptTalk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Text")
	FText PromptHandOver;

	/** Сервер: проиграть серию реплик (можно звать из Blueprint для своих сцен — куратор и факты кампании). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NPC")
	void SayLines(const TArray<FDarcNpcLine>& Lines);

	UFUNCTION(BlueprintPure, Category = "NPC")
	bool IsSpeaking() const { return bSpeaking; }

	// --- IInteractable ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void HandleGreetOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Сервер: NPC принял предмет — здесь Blueprint решает про особые реплики (лишний файл и т.п.). */
	UFUNCTION(BlueprintImplementableEvent, Category = "NPC")
	void OnItemReceived(ACarryableItem* Item, AActor* FromActor);

	/** Сервер: NPC выдал предмет. */
	UFUNCTION(BlueprintImplementableEvent, Category = "NPC")
	void OnItemGiven(ACarryableItem* Item, AActor* ToActor);

	/** Анимация «говорит» у всех клиентов. */
	UFUNCTION(BlueprintImplementableEvent, Category = "NPC")
	void OnSpeakingChanged(bool bNowSpeaking);

	UFUNCTION()
	void OnRep_Speaking();

	void PlayNextLine();
	void CompleteTaskIfSet(FName TaskId, AActor* ByActor);

	UPROPERTY(ReplicatedUsing = OnRep_Speaking)
	bool bSpeaking = false;

	UPROPERTY(Replicated)
	bool bGaveItem = false;

	bool bGreeted = false;
	bool bAskedBeforeReceive = false;
	TArray<FDarcNpcLine> LineQueue;
	FTimerHandle LineTimer;
};
