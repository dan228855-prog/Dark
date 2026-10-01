// DarcPlayerController.h
// Контроллер игрока D.A.R.C. Наследник шаблонного (ходьба/обзор — его Enhanced Input
// из Blueprint шаблона, раскладка копируется автоматически). Сверху — свои клавиши
// без ассетов ввода: E — взаимодействие, Tab — список задач, для «духа» ЛКМ/ПКМ/F.
// Окно терминала (ввод кода) — Slate, без UMG-ассетов.
#pragma once

#include "CoreMinimal.h"
#include "DARKPlayerController.h"
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

	/** Локальный контроллер этой машины (или nullptr на выделенном сервере). */
	static ADarcPlayerController* GetLocal(const UObject* WorldContextObject);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void CopyTemplateInputMappings();
	void EnsureInteractionComponent(APawn* InPawn);

	/** Сервер: добавить персонажу шаблона нужный ему сетевой компонент, если его нет. */
	template <class TComponent>
	void EnsureComponent(APawn* InPawn, const TCHAR* Name);

	void HandleInteract();
	void HandleToggleTasks();
	void HandlePrimaryPressed();   // ЛКМ: захват (живой) / мигнуть светом (дух)
	void HandlePrimaryReleased();  // ЛКМ отпущена: отпустить предмет
	void HandleSecondary();        // ПКМ: бросить (живой) / толкнуть предмет (дух)
	void HandleSpiritBreaker();

	void SubmitTerminalText(const FText& Text);
	FText BuildTerminalScreenText() const;

	TWeakObjectPtr<ADarcTerminal> OpenTerminal;
	TSharedPtr<SWidget> TerminalWidget;
	TSharedPtr<SEditableTextBox> TerminalInput;
	TSharedPtr<STextBlock> TerminalScreen;
	TSharedPtr<STextBlock> TerminalStatus;

	bool bShowTasks = true;
};
