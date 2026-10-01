// DarcPlayerController.cpp
#include "DarcPlayerController.h"
#include "DarcTerminal.h"
#include "DarcSpiritCharacter.h"
#include "DarcGameplayLibrary.h"
#include "InteractionComponent.h"
#include "DarcGrabComponent.h"
#include "DarcFootstepComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Components/InputComponent.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

ADarcPlayerController::ADarcPlayerController()
{
}

ADarcPlayerController* ADarcPlayerController::GetLocal(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? Cast<ADarcPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

void ADarcPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void ADarcPlayerController::CopyTemplateInputMappings()
{
	// Раскладка ходьбы/обзора настроена в Blueprint контроллера шаблона. Чтобы не дублировать
	// ассеты ввода, копируем её оттуда через отражение (свойства защищённые).
	if (DefaultMappingContexts.Num() > 0)
	{
		return;
	}
	UClass* TemplateClass = LoadClass<ADARKPlayerController>(nullptr,
		TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
	const UObject* TemplateCDO = TemplateClass ? TemplateClass->GetDefaultObject() : nullptr;
	if (!TemplateCDO)
	{
		UE_LOG(LogTemp, Warning, TEXT("DARC: template player controller not found, movement mappings are empty"));
		return;
	}

	for (const TCHAR* Name : { TEXT("DefaultMappingContexts"), TEXT("MobileExcludedMappingContexts") })
	{
		if (FProperty* Prop = ADARKPlayerController::StaticClass()->FindPropertyByName(Name))
		{
			Prop->CopyCompleteValue(Prop->ContainerPtrToValuePtr<void>(this), Prop->ContainerPtrToValuePtr<void>(TemplateCDO));
		}
	}
}

void ADarcPlayerController::SetupInputComponent()
{
	CopyTemplateInputMappings(); // до Super: родитель добавляет контексты из этих массивов
	Super::SetupInputComponent();

	// Свои клавиши — прямой привязкой, без ассетов Input Action.
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ADarcPlayerController::HandleInteract);
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ADarcPlayerController::HandleToggleTasks);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADarcPlayerController::HandlePrimaryPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ADarcPlayerController::HandlePrimaryReleased);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ADarcPlayerController::HandleSecondary);
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ADarcPlayerController::HandleSpiritBreaker);
}

void ADarcPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	EnsureInteractionComponent(InPawn);
}

template <class TComponent>
void ADarcPlayerController::EnsureComponent(APawn* InPawn, const TCHAR* Name)
{
	// Реплицируемый компонент, созданный на сервере, появится и у клиентов — через него
	// клиент шлёт Server RPC, а у остальных игроков он, например, играет шаги.
	if (!InPawn || InPawn->FindComponentByClass<TComponent>())
	{
		return;
	}
	TComponent* Component = NewObject<TComponent>(InPawn, Name);
	Component->SetIsReplicated(true);
	Component->RegisterComponent();
	InPawn->AddInstanceComponent(Component);
}

void ADarcPlayerController::EnsureInteractionComponent(APawn* InPawn)
{
	// Персонажу из шаблона добавляем свои компоненты. «Духу» они не нужны.
	if (!InPawn || Cast<ADarcSpiritCharacter>(InPawn))
	{
		return;
	}
	EnsureComponent<UInteractionComponent>(InPawn, TEXT("Interaction"));
	EnsureComponent<UDarcGrabComponent>(InPawn, TEXT("Grab"));
	EnsureComponent<UDarcFootstepComponent>(InPawn, TEXT("Footsteps"));
}

void ADarcPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseTerminalUI(false);
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Клавиши
// ---------------------------------------------------------------------------

void ADarcPlayerController::HandleInteract()
{
	if (UInteractionComponent* Interaction = GetPawn() ? GetPawn()->FindComponentByClass<UInteractionComponent>() : nullptr)
	{
		Interaction->TryInteract();
	}
}

void ADarcPlayerController::HandleToggleTasks()
{
	bShowTasks = !bShowTasks;
}

void ADarcPlayerController::HandlePrimaryPressed()
{
	if (ADarcSpiritCharacter* Spirit = Cast<ADarcSpiritCharacter>(GetPawn()))
	{
		Spirit->TryFlicker();
	}
	else if (UDarcGrabComponent* Grab = GetPawn() ? GetPawn()->FindComponentByClass<UDarcGrabComponent>() : nullptr)
	{
		Grab->StartGrab();
	}
}

void ADarcPlayerController::HandlePrimaryReleased()
{
	if (UDarcGrabComponent* Grab = GetPawn() ? GetPawn()->FindComponentByClass<UDarcGrabComponent>() : nullptr)
	{
		Grab->StopGrab();
	}
}

void ADarcPlayerController::HandleSecondary()
{
	if (ADarcSpiritCharacter* Spirit = Cast<ADarcSpiritCharacter>(GetPawn()))
	{
		Spirit->TryPush();
	}
	else if (UDarcGrabComponent* Grab = GetPawn() ? GetPawn()->FindComponentByClass<UDarcGrabComponent>() : nullptr)
	{
		Grab->Throw();
	}
}

void ADarcPlayerController::HandleSpiritBreaker()
{
	if (ADarcSpiritCharacter* Spirit = Cast<ADarcSpiritCharacter>(GetPawn()))
	{
		Spirit->TryToggleBreaker();
	}
}

// ---------------------------------------------------------------------------
// Окно терминала (Slate)
// ---------------------------------------------------------------------------

FText ADarcPlayerController::BuildTerminalScreenText() const
{
	const ADarcTerminal* Terminal = OpenTerminal.Get();
	if (!Terminal)
	{
		return FText::GetEmpty();
	}
	if (Terminal->State == EDarcTerminalState::Unlocked)
	{
		// Экран открытого терминала — строки из настроек терминала (тексты String Table).
		TArray<FText> Lines;
		Lines.Add(Terminal->UnlockedTitle);
		Lines.Append(Terminal->GetScreenLines());
		return FText::Join(FText::FromString(TEXT("\n")), Lines);
	}
	return Terminal->LockedTitle;
}

void ADarcPlayerController::OpenTerminalUI(ADarcTerminal* Terminal)
{
	if (!IsLocalController() || !Terminal || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	CloseTerminalUI(false);
	OpenTerminal = Terminal;

	TerminalWidget =
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(720.f)
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.92f))
				.Padding(24.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
					[
						SAssignNew(TerminalScreen, STextBlock)
						.Text(BuildTerminalScreenText())
						.ColorAndOpacity(FLinearColor(0.55f, 1.f, 0.6f))
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
					[
						SAssignNew(TerminalStatus, STextBlock)
						.ColorAndOpacity(FLinearColor(1.f, 0.8f, 0.4f))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(TerminalInput, SEditableTextBox)
						.HintText(UDarcGameplayLibrary::UIText(TEXT("Terminal_InputHint")))
						.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type Commit)
						{
							if (Commit == ETextCommit::OnEnter)
							{
								SubmitTerminalText(Text);
							}
							else if (Commit == ETextCommit::OnCleared)
							{
								CloseTerminalUI(true); // Esc — выйти из терминала
							}
						})
					]
				]
			]
		];

	GEngine->GameViewport->AddViewportWidgetContent(TerminalWidget.ToSharedRef(), 10);

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(TerminalInput);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
}

void ADarcPlayerController::CloseTerminalUI(bool bNotifyServer)
{
	if (TerminalWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(TerminalWidget.ToSharedRef());
	}
	const bool bWasOpen = TerminalWidget.IsValid();
	TerminalWidget.Reset();
	TerminalInput.Reset();
	TerminalScreen.Reset();
	TerminalStatus.Reset();

	if (bWasOpen && IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}

	if (bNotifyServer && OpenTerminal.IsValid())
	{
		if (UInteractionComponent* Interaction = GetPawn() ? GetPawn()->FindComponentByClass<UInteractionComponent>() : nullptr)
		{
			Interaction->LeaveTerminal(OpenTerminal.Get());
		}
	}
	OpenTerminal.Reset();
}

void ADarcPlayerController::SubmitTerminalText(const FText& Text)
{
	if (OpenTerminal.IsValid() && !Text.IsEmpty())
	{
		if (UInteractionComponent* Interaction = GetPawn() ? GetPawn()->FindComponentByClass<UInteractionComponent>() : nullptr)
		{
			Interaction->SubmitTerminalInput(OpenTerminal.Get(), Text.ToString());
		}
	}
	if (TerminalInput.IsValid())
	{
		TerminalInput->SetText(FText::GetEmpty());
		FSlateApplication::Get().SetKeyboardFocus(TerminalInput);
	}
}

void ADarcPlayerController::ShowTerminalResult(bool bAccepted, const FString& Echo)
{
	if (TerminalStatus.IsValid())
	{
		TerminalStatus->SetText(UDarcGameplayLibrary::UIText(bAccepted ? TEXT("Screen_AccessGranted") : TEXT("Screen_AccessDenied")));
	}
	RefreshTerminalUI();
}

void ADarcPlayerController::RefreshTerminalUI()
{
	if (TerminalScreen.IsValid())
	{
		TerminalScreen->SetText(BuildTerminalScreenText());
	}
}
