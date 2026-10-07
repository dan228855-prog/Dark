// DarcPlayerController.h
// Контроллер игрока D.A.R.C. Наследник шаблонного (ходьба/обзор — его Enhanced Input
// из Blueprint шаблона, раскладка копируется автоматически). Сверху — свои клавиши
// без ассетов ввода: E — взаимодействие, Tab — список задач, для «духа» ЛКМ/ПКМ/F.
// Окно терминала (ввод кода) — Slate, без UMG-ассетов.
#pragma once

#include "CoreMinimal.h"
#include "DARKPlayerController.h"
#include "Styling/SlateBrush.h"
#include "DarcPlayerController.generated.h"

class ADarcTerminal;
class SWidget;
class SEditableTextBox;
class STextBlock;

UCLASS(config = "Game")
class DARK_API ADarcPlayerController : public ADARKPlayerController
{
	GENERATED_BODY()

public:
	ADarcPlayerController();

	/** Терминал занят этим игроком — открыть окно ввода (вызывает терминал на локальной машине). */
	void OpenTerminalUI(ADarcTerminal* Terminal);
	void CloseTerminalUI(bool bNotifyServer);

	/** Результат ввода на открытом терминале. */
	void ShowTerminalResult(bool bAccepted, const FString& Echo);

	/** Обновить содержимое окна (состояние терминала сменилось). */
	void RefreshTerminalUI();

	bool IsTaskListVisible() const { return bShowTasks; }

	/** Зажат V (говорю). */
	bool IsTalking() const { return bTalking; }

	/** Сервер: новое «безопасное» место (например, игрока высадили из машины в другом месте карты). */
	void SetSafeSpawnLocation(const FVector& Location) { SafeSpawnLocation = Location; }

	/** Локальный контроллер этой машины (или nullptr на выделенном сервере). */
	static ADarcPlayerController* GetLocal(const UObject* WorldContextObject);

	/** Клавиши из настроек игрока: прямые привязки и раскладка ходьбы/прыжка шаблона. */
	void ApplyKeyBindings();

	/** Голос: открытый микрофон или «нажми и говори» (из настроек). */
	void ApplyVoiceMode();

	/** База: открыть окно выбора выезда у этого игрока (терминал в лобби). */
	UFUNCTION(Client, Reliable)
	void Client_OpenMissionSelect();

	/** База: выбрать выезд (сервер проверяет, открыт ли уровень). */
	UFUNCTION(Server, Reliable)
	void Server_SelectMission(int32 Level);

	/** Esc: меню паузы. Закрыть — вернуть управление игре. */
	void SetPauseMenuOpen(bool bOpen);

	// --- Консоль разработчика (клавиша ` / Ё) для проверок, в том числе в собранной игре. ---
	// В Shipping-сборке команды ничего не делают. Реализация — DarcDevConsole.cpp. Список — DarcHelp.
	UFUNCTION(Exec) void DarcHelp();
	UFUNCTION(Exec) void DarcGod();
	UFUNCTION(Exec) void DarcHeal();
	UFUNCTION(Exec) void DarcKill();
	UFUNCTION(Exec) void DarcBattery(int32 Percent);
	UFUNCTION(Exec) void DarcFear(float Value);
	UFUNCTION(Exec) void DarcShadow();
	UFUNCTION(Exec) void DarcTasks();
	UFUNCTION(Exec) void DarcTask(const FString& TaskId);
	UFUNCTION(Exec) void DarcMains(int32 On);
	UFUNCTION(Exec) void DarcFact(const FString& Fact);
	UFUNCTION(Exec) void DarcFacts();
	UFUNCTION(Exec) void DarcRooms();
	UFUNCTION(Exec) void DarcGoto(const FString& Room);
	UFUNCTION(Exec) void DarcSkipIntro();
	UFUNCTION(Exec) void DarcEndMission(int32 Success);
	UFUNCTION(Exec) void DarcVoiceLogs();

	/** Сервер: выполнить команду консоли разработчика (то, что меняет игру, решает сервер). */
	UFUNCTION(Server, Reliable)
	void Server_Dev(const FString& Command, const FString& Arg);

	/** Ответ консоли: в консоль и на экран у этого игрока. */
	UFUNCTION(Client, Reliable)
	void Client_DevPrint(const FString& Text);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	/** Вызывается и на сервере, и у клиента — здесь применяем локальные настройки игрока (поле зрения). */
	virtual void SetPawn(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void CopyTemplateInputMappings();
	void EnsureInteractionComponent(APawn* InPawn);

	/** Сервер: добавить персонажу шаблона нужный ему сетевой компонент, если его нет. */
	template <class TComponent>
	void EnsureComponent(APawn* InPawn, const TCHAR* Name);

	void HandleInteract();
	void HandleToggleTasks();
	void HandleDrop();             // G: положить то, что в руках
	void HandleSprintPressed();    // Shift: бег (тратит выносливость)
	void HandleSprintReleased();
	void HandlePrimaryPressed();   // ЛКМ: захват (живой) / мигнуть светом (дух)
	void HandlePrimaryReleased();  // ЛКМ отпущена: отпустить предмет
	void HandleSecondary();        // ПКМ: бросить (живой) / толкнуть предмет (дух)
	void HandleSpiritBreaker();    // F: рубильник (дух) / фонарик (живой)
	void HandleSlot1();            // 1–4: слот инвентаря
	void HandleSlot2();
	void HandleSlot3();
	void HandleSlot4();
	void SelectInventorySlot(int32 Index);
	void HandleCrouchPressed();    // Ctrl / C: пригнуться (вентиляция, низкие проходы)
	void HandleCrouchReleased();
	void HandleTalkPressed();      // V: говорить (push-to-talk)
	void HandleTalkReleased();
	void HandlePause();            // Esc: меню паузы (не переназначается)

	/** Раскладки шаблона как есть (по ним ищем клавиши по умолчанию) и их копии с клавишами игрока. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UInputMappingContext>> TemplateMappingContexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UInputMappingContext>> PlayerMappingContexts;

	/** Своя машина, раз в 0.5 с: голос других игроков — из их персонажа (рядом) или по рации. */
	void UpdateVoiceTalkers();
	FTimerHandle VoiceTimer;

	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerState>, TObjectPtr<class UVOIPTalker>> Talkers;

	UPROPERTY(Transient)
	TObjectPtr<class USoundAttenuation> VoiceAttenuation;

	bool bTalking = false;

	/** Своя машина: изредка «тень» в стороне от взгляда (см. ADarcShadowFigure). */
	void MaybeSpawnShadow();
	FTimerHandle ShadowTimer;
	TWeakObjectPtr<class ADarcShadowFigure> Shadow;

	/** Шанс появления тени за одну проверку (раз в 20 с), после первых 90 с на ногах. */
	UPROPERTY(EditAnywhere, Category = "Shadow")
	float ShadowChance = 0.12f;

	/** Своя машина: поставить тень сейчас (с учётом недавнего шума рядом). */
	void SpawnShadowNow();

	void SubmitTerminalText(const FText& Text);
	FText BuildTerminalScreenText() const;

	/** Место, куда вернуть игрока, если он провалится сквозь карту (дыра в коллизии) —
	    запоминается при каждом OnPossess, обновляется таймером раз в секунду. */
	void CheckFallSafety();
	FVector SafeSpawnLocation = FVector::ZeroVector;
	FTimerHandle FallSafetyTimer;

	TWeakObjectPtr<ADarcTerminal> OpenTerminal;
	TSharedPtr<SWidget> TerminalWidget;
	TSharedPtr<SEditableTextBox> TerminalInput;
	TSharedPtr<STextBlock> TerminalScreen;
	TSharedPtr<STextBlock> TerminalStatus;

	/** Оформление окна терминала из UI-пака D.A.R.C.: экран с развёрткой и сканлайны поверх. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UTexture2D>> TerminalTextures;
	FSlateBrush TerminalScreenBrush;
	FSlateBrush TerminalScanBrush;

	bool bShowTasks = true;
};
