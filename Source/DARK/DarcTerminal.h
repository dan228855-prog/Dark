// DarcTerminal.h
// Компьютер/терминал: ввод кода доступа и команды после разблокировки.
//
// Сеть:
// - Терминалом одновременно пользуется один игрок (CurrentUser, реплицируется).
//   Остальные видят, что терминал занят, и экран обновляется у всех.
// - Окно ввода (UMG) открывает Blueprint у того клиента, кто сейчас пользователь.
// - Набранный текст клиент отправляет через UInteractionComponent::SubmitTerminalInput
//   (Server RPC на своей пешке). Сервер проверяет: тот ли это пользователь, рядом ли он,
//   есть ли питание — и только потом сверяет код. Код клиенту не доверяем.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcPowerSubsystem.h"
#include "DarcTerminal.generated.h"

class APlayerState;
class UDarcPowerConsumerComponent;
class AInteractableDoor;

UENUM(BlueprintType)
enum class EDarcTerminalState : uint8
{
	NoPower  UMETA(DisplayName = "Нет питания"),
	Locked   UMETA(DisplayName = "Требует код"),
	Unlocked UMETA(DisplayName = "Открыт")
};

UCLASS()
class DARK_API ADarcTerminal : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcTerminal();

	/** Питание терминала. Если CircuitId у компонента пуст — терминал считается всегда запитанным. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terminal")
	TObjectPtr<UDarcPowerConsumerComponent> Power;

	/** Ключ кода выезда (UDarcGameplayLibrary::GetMissionCode). Тот же ключ показывает карта доступа. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminal")
	FName CodeKey = TEXT("CatalogCode");

	/** Фиксированный код вместо кода выезда (для ручных тестов). Пусто — используется CodeKey. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminal")
	FString FixedCode;

	/** Терминал открыт изначально (без кода). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminal")
	bool bStartsUnlocked = false;

	/** Задача при правильном коде. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminal|Tasks")
	FName TaskIdOnUnlock;

	/** Необязательно: дверь, которую отпирает правильный код (кодовая панель у двери). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminal")
	TObjectPtr<AInteractableDoor> DoorToUnlock;

	/** Команды после разблокировки → задачи (например "copy_extra" → TakeExtraFile). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminal|Tasks")
	TMap<FString, FName> CommandTasks;

	/** Дальше этой дистанции пользователь «отходит», и терминал освобождается, см. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminal")
	float UseRange = 250.f;

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Terminal")
	EDarcTerminalState State = EDarcTerminalState::Locked;

	UPROPERTY(ReplicatedUsing = OnRep_User, BlueprintReadOnly, Category = "Terminal")
	TObjectPtr<APlayerState> CurrentUser;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminal|Text")
	FText PromptUse;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminal|Text")
	FText PromptBusy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terminal|Text")
	FText PromptNoPower;

	/** Это я сейчас пользуюсь терминалом? (для UI на клиенте) */
	UFUNCTION(BlueprintPure, Category = "Terminal")
	bool IsLocalPlayerUser() const;

	/** Сервер: обработать ввод от пользователя. Вызывается из UInteractionComponent после проверок. */
	void ServerHandleInput(APlayerState* FromPlayer, const FString& Input);

	/** Сервер: освободить терминал. */
	void ServerReleaseUser(APlayerState* FromPlayer);

	/** Сервер: можно ли этому игроку сейчас вводить (пользователь, рядом, есть питание). */
	bool IsValidUser(const APlayerState* Player) const;

	/** Сервер: разблокировать без кода (альтернативный путь, событие, сценарий). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Terminal")
	void ForceUnlock(AActor* ByActor);

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
	void OnRep_State();

	UFUNCTION()
	void OnRep_User();

	/** Экран терминала у всех клиентов: что показывать при этом состоянии. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Terminal")
	void OnTerminalStateChanged(EDarcTerminalState NewState);

	/** Сменился пользователь. bIsLocalUser — открыть/закрыть окно ввода на этой машине. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Terminal")
	void OnUserChanged(APlayerState* NewUser, bool bIsLocalUser);

	/** Результат ввода — всем (экран терминала видят все), с текстом ввода для эха на экране. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_InputResult(bool bAccepted, const FString& EchoInput);

	UFUNCTION(BlueprintImplementableEvent, Category = "Terminal")
	void OnInputResult(bool bAccepted, const FString& EchoInput);

	/** Сервер: команда после разблокировки, не найденная в CommandTasks (сюжетные страницы и т.п.). */
	UFUNCTION(BlueprintNativeEvent, Category = "Terminal")
	bool HandleCustomCommand(APlayerState* FromPlayer, const FString& Command);

	FString GetExpectedCode() const;
	void SetState(EDarcTerminalState NewState);
	void CheckUserStillNear();

	FTimerHandle UserCheckTimer;
	bool bUnlocked = false;
};
