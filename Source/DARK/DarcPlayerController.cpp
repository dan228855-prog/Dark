// DarcPlayerController.cpp
#include "DarcPlayerController.h"
#include "DarcTerminal.h"
#include "DarcSpiritCharacter.h"
#include "DarcGameplayLibrary.h"
#include "InteractionComponent.h"
#include "DarcGrabComponent.h"
#include "DarcFootstepComponent.h"
#include "DarcFlashlightComponent.h"
#include "DarcPlayerOutfitComponent.h"
#include "DarcVitalsComponent.h"
#include "DarcInventoryComponent.h"
#include "DarcShadowFigure.h"
#include "DarcNoiseSubsystem.h"
#include "DarcGameState.h"
#include "DarcTool.h"
#include "DarcAssetSettings.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/VoiceConfig.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Components/InputComponent.h"
#include "Components/PrimitiveComponent.h"
#include "TimerManager.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Engine/Texture2D.h"
#include "DarcGameUserSettings.h"
#include "DarcSettingsMenu.h"
#include "DarcBase.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"

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
	if (IsLocalController())
	{
		GetWorldTimerManager().SetTimer(VoiceTimer, this, &ADarcPlayerController::UpdateVoiceTalkers, 0.5f, true);
		GetWorldTimerManager().SetTimer(ShadowTimer, this, &ADarcPlayerController::MaybeSpawnShadow, 20.f, true, 90.f);
		ApplyVoiceMode();
	}
}

void ADarcPlayerController::MaybeSpawnShadow()
{
	// Только живой на ногах (не «дух», не в машине) и только одна тень за раз.
	APawn* MyPawn = GetPawn();
	if (!MyPawn || Cast<ADarcSpiritCharacter>(MyPawn) || MyPawn->GetAttachParentActor() || Shadow.IsValid())
	{
		return;
	}
	// Чем дальше уровень — тем чаще; испуганному мерещится чаще; шум рядом её притягивает.
	const int32 Level = ADarcGameState::GetMissionLevel(this);
	const UDarcVitalsComponent* Vitals = MyPawn->FindComponentByClass<UDarcVitalsComponent>();
	float Chance = ShadowChance * (1.f + 0.35f * (Level - 1)) + (Vitals ? Vitals->GetFear() * 0.1f : 0.f);
	FVector NoiseLocation;
	float Loudness = 0.f;
	if (const UDarcNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UDarcNoiseSubsystem>())
	{
		Noise->FindRecent(MyPawn->GetActorLocation(), 2500.f, 25.f, NoiseLocation, Loudness);
	}
	Chance += Loudness * 0.3f;
	if (FMath::FRand() < Chance)
	{
		SpawnShadowNow();
	}
}

void ADarcPlayerController::SpawnShadowNow()
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn || Shadow.IsValid())
	{
		return;
	}
	FVector NoiseLocation;
	float Loudness = 0.f;
	const UDarcNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UDarcNoiseSubsystem>();
	const bool bNoise = Noise && Noise->FindRecent(MyPawn->GetActorLocation(), 2500.f, 25.f, NoiseLocation, Loudness);
	Shadow = ADarcShadowFigure::TrySpawnNear(this, bNoise ? &NoiseLocation : nullptr);
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
	// Родитель добавит копии раскладок — в них клавиши игрока (ApplyKeyBindings).
	if (PlayerMappingContexts.Num() == 0)
	{
		for (TArray<UInputMappingContext*>* Contexts : { &DefaultMappingContexts, &MobileExcludedMappingContexts })
		{
			for (UInputMappingContext*& Context : *Contexts)
			{
				if (Context)
				{
					TemplateMappingContexts.Add(Context);
					Context = DuplicateObject<UInputMappingContext>(Context, this);
					PlayerMappingContexts.Add(Context);
				}
			}
		}
	}
	Super::SetupInputComponent();

	ApplyKeyBindings();
}

void ADarcPlayerController::ApplyKeyBindings()
{
	const UDarcGameUserSettings* Settings = UDarcGameUserSettings::Get();
	auto Key = [Settings](FName Id) { return Settings ? Settings->GetKey(Id) : FKey(); };

	// Свои клавиши — прямой привязкой, без ассетов Input Action. Клавиши — из настроек игрока.
	if (InputComponent)
	{
		InputComponent->KeyBindings.Reset();
		InputComponent->BindKey(Key(TEXT("Interact")), IE_Pressed, this, &ADarcPlayerController::HandleInteract);
		InputComponent->BindKey(Key(TEXT("Tasks")), IE_Pressed, this, &ADarcPlayerController::HandleToggleTasks);
		InputComponent->BindKey(Key(TEXT("Grab")), IE_Pressed, this, &ADarcPlayerController::HandlePrimaryPressed);
		InputComponent->BindKey(Key(TEXT("Grab")), IE_Released, this, &ADarcPlayerController::HandlePrimaryReleased);
		InputComponent->BindKey(Key(TEXT("Push")), IE_Pressed, this, &ADarcPlayerController::HandleSecondary);
		InputComponent->BindKey(Key(TEXT("Flashlight")), IE_Pressed, this, &ADarcPlayerController::HandleSpiritBreaker);
		InputComponent->BindKey(Key(TEXT("Drop")), IE_Pressed, this, &ADarcPlayerController::HandleDrop);
		InputComponent->BindKey(Key(TEXT("Sprint")), IE_Pressed, this, &ADarcPlayerController::HandleSprintPressed);
		InputComponent->BindKey(Key(TEXT("Sprint")), IE_Released, this, &ADarcPlayerController::HandleSprintReleased);
		InputComponent->BindKey(Key(TEXT("Slot1")), IE_Pressed, this, &ADarcPlayerController::HandleSlot1);
		InputComponent->BindKey(Key(TEXT("Slot2")), IE_Pressed, this, &ADarcPlayerController::HandleSlot2);
		InputComponent->BindKey(Key(TEXT("Slot3")), IE_Pressed, this, &ADarcPlayerController::HandleSlot3);
		InputComponent->BindKey(Key(TEXT("Slot4")), IE_Pressed, this, &ADarcPlayerController::HandleSlot4);
		InputComponent->BindKey(Key(TEXT("Crouch")), IE_Pressed, this, &ADarcPlayerController::HandleCrouchPressed);
		InputComponent->BindKey(Key(TEXT("Crouch")), IE_Released, this, &ADarcPlayerController::HandleCrouchReleased);
		InputComponent->BindKey(Key(TEXT("Talk")), IE_Pressed, this, &ADarcPlayerController::HandleTalkPressed);
		InputComponent->BindKey(Key(TEXT("Talk")), IE_Released, this, &ADarcPlayerController::HandleTalkReleased);
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ADarcPlayerController::HandlePause);
	}

	// Ходьба и прыжок — раскладка шаблона: в копии меняем клавиши по умолчанию на клавиши игрока.
	for (int32 Index = 0; Index < TemplateMappingContexts.Num() && Index < PlayerMappingContexts.Num(); ++Index)
	{
		const UInputMappingContext* Template = TemplateMappingContexts[Index];
		UInputMappingContext* Copy = PlayerMappingContexts[Index];
		if (!Template || !Copy || Template->GetMappings().Num() != Copy->GetMappings().Num())
		{
			continue;
		}
		for (int32 M = 0; M < Copy->GetMappings().Num(); ++M)
		{
			const FKey DefaultKey = Template->GetMappings()[M].Key;
			FKey NewKey = DefaultKey;
			for (const FDarcKeyAction& Action : UDarcGameUserSettings::GetKeyActions())
			{
				if (Action.bEnhancedInput && Action.DefaultKey == DefaultKey)
				{
					NewKey = Key(Action.Id);
					break;
				}
			}
			Copy->GetMapping(M).Key = NewKey;
		}
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->RequestRebuildControlMappings();
	}
}

void ADarcPlayerController::ApplyVoiceMode()
{
	const UDarcGameUserSettings* Settings = UDarcGameUserSettings::Get();
	if (!IsLocalController() || !Settings)
	{
		return;
	}
	if (Settings->bOpenMic)
	{
		bTalking = true;
		StartTalking();
	}
	else if (bTalking)
	{
		bTalking = false;
		StopTalking();
	}
}

void ADarcPlayerController::HandlePause()
{
	if (TerminalWidget.IsValid())
	{
		CloseTerminalUI(true); // Esc в терминале — просто закрыть терминал
		return;
	}
	SetPauseMenuOpen(true);
}

void ADarcPlayerController::SetPauseMenuOpen(bool bOpen)
{
	if (bOpen)
	{
		if (UDarcSettingsLibrary::IsPauseMenuOpen())
		{
			return;
		}
		UDarcSettingsLibrary::OpenPauseMenu(this);
		SetShowMouseCursor(true);
		SetInputMode(FInputModeUIOnly());
		// В одиночной игре время останавливается; в кооперативе мир идёт дальше.
		if (GetNetMode() == NM_Standalone)
		{
			SetPause(true);
		}
		return;
	}
	SetPause(false);
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
}

void ADarcPlayerController::HandleSlot1() { SelectInventorySlot(0); }
void ADarcPlayerController::HandleSlot2() { SelectInventorySlot(1); }
void ADarcPlayerController::HandleSlot3() { SelectInventorySlot(2); }
void ADarcPlayerController::HandleSlot4() { SelectInventorySlot(3); }

void ADarcPlayerController::SelectInventorySlot(int32 Index)
{
	if (UDarcInventoryComponent* Inventory = GetPawn() ? GetPawn()->FindComponentByClass<UDarcInventoryComponent>() : nullptr)
	{
		Inventory->SelectSlot(Index);
	}
}

void ADarcPlayerController::HandleCrouchPressed()
{
	// Пока Ctrl зажат — пригнулся. Отпустил в низком проходе — встанет сам, когда будет место.
	if (ACharacter* MyCharacter = Cast<ACharacter>(GetPawn()); MyCharacter && !Cast<ADarcSpiritCharacter>(MyCharacter))
	{
		MyCharacter->GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
		MyCharacter->Crouch();
	}
}

void ADarcPlayerController::HandleCrouchReleased()
{
	if (ACharacter* MyCharacter = Cast<ACharacter>(GetPawn()))
	{
		MyCharacter->UnCrouch();
	}
}

void ADarcPlayerController::HandleTalkPressed()
{
	if (const UDarcGameUserSettings* Settings = UDarcGameUserSettings::Get(); Settings && Settings->bOpenMic)
	{
		return; // открытый микрофон — голос идёт и так
	}
	bTalking = true;
	StartTalking();
	// С рацией — щелчок тангенты у себя (напарник с рацией слышит голос по радио, без расстояния).
	if (GetPawn() && ADarcTool::HasTool(GetPawn(), EDarcToolType::Radio))
	{
		UDarcAssetSettings::PlaySound(this, TEXT("RadioClick"), GetPawn()->GetActorLocation(), 0.5f);
	}
}

void ADarcPlayerController::HandleTalkReleased()
{
	if (const UDarcGameUserSettings* Settings = UDarcGameUserSettings::Get(); Settings && Settings->bOpenMic)
	{
		return;
	}
	bTalking = false;
	StopTalking();
}

void ADarcPlayerController::UpdateVoiceTalkers()
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS || !PlayerState)
	{
		return;
	}
	if (!VoiceAttenuation)
	{
		// Голос рядом: слышно отчётливо до 3 м, тише к 20 м, сквозь стены — глуше.
		VoiceAttenuation = NewObject<USoundAttenuation>(this);
		FSoundAttenuationSettings& A = VoiceAttenuation->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(300.f, 0.f, 0.f);
		A.FalloffDistance = 1700.f;
		A.bEnableOcclusion = true;
		A.OcclusionLowPassFilterFrequency = 1200.f;
		A.OcclusionVolumeAttenuation = 0.45f;
	}
	const bool bIHaveRadio = GetPawn() && ADarcTool::HasTool(GetPawn(), EDarcToolType::Radio);
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS || PS == PlayerState)
		{
			continue;
		}
		UVOIPTalker* Talker = Talkers.FindRef(PS);
		if (!Talker)
		{
			Talker = UVOIPTalker::CreateTalkerForPlayer(PS);
			if (!Talker)
			{
				continue;
			}
			Talkers.Add(PS, Talker);
		}
		// Рации у обоих — голос по радио (без расстояния, «в ухе»); иначе — из персонажа рядом.
		APawn* Other = PS->GetPawn();
		const bool bRadio = bIHaveRadio && Other && ADarcTool::HasTool(Other, EDarcToolType::Radio);
		Talker->Settings.ComponentToAttachTo = (!bRadio && Other) ? Other->GetRootComponent() : nullptr;
		Talker->Settings.AttenuationSettings = bRadio ? nullptr : VoiceAttenuation.Get();
	}
	for (auto It = Talkers.CreateIterator(); It; ++It)
	{
		if (!IsValid(It.Key()) || !GS->PlayerArray.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

void ADarcPlayerController::HandleSprintPressed()
{
	if (UDarcVitalsComponent* Vitals = GetPawn() ? GetPawn()->FindComponentByClass<UDarcVitalsComponent>() : nullptr)
	{
		Vitals->SetSprintPressed(true);
	}
}

void ADarcPlayerController::HandleSprintReleased()
{
	if (UDarcVitalsComponent* Vitals = GetPawn() ? GetPawn()->FindComponentByClass<UDarcVitalsComponent>() : nullptr)
	{
		Vitals->SetSprintPressed(false);
	}
}

void ADarcPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	if (InPawn && IsLocalController())
	{
		if (const UDarcGameUserSettings* Settings = UDarcGameUserSettings::Get())
		{
			Settings->ApplyFieldOfView(InPawn);
		}
	}
}

void ADarcPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	EnsureInteractionComponent(InPawn);

	// Запоминаем точку появления и следим раз в секунду: если игрок проваливается сквозь
	// карту (дыра в коллизии где-то в уровне) и падает ниже, чем могло бы быть «ещё пол»,
	// возвращаем его назад, а не даём падать бесконечно (было: каждый новый Play начинался
	// там же, где закончилась предыдущая попытка, то есть всё глубже под картой).
	if (InPawn && HasAuthority())
	{
		SafeSpawnLocation = InPawn->GetActorLocation();
		GetWorldTimerManager().SetTimer(FallSafetyTimer, this, &ADarcPlayerController::CheckFallSafety, 1.f, true);
	}
}

void ADarcPlayerController::CheckFallSafety()
{
	APawn* MyPawn = GetPawn();
	// Сидит в машине (привязан к ней) — машина едет по рельефу, это не падение.
	if (!MyPawn || !HasAuthority() || MyPawn->GetAttachParentActor())
	{
		return;
	}
	if (MyPawn->GetActorLocation().Z < SafeSpawnLocation.Z - 1000.f)
	{
		MyPawn->SetActorLocation(SafeSpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
		if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(MyPawn->GetRootComponent()))
		{
			Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		}
	}
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
	EnsureComponent<UDarcFlashlightComponent>(InPawn, TEXT("Flashlight"));
	EnsureComponent<UDarcPlayerOutfitComponent>(InPawn, TEXT("Outfit"));
	EnsureComponent<UDarcVitalsComponent>(InPawn, TEXT("Vitals"));
	EnsureComponent<UDarcInventoryComponent>(InPawn, TEXT("Inventory"));
	// Пригибаться (Ctrl) — для вентиляции и низких проходов; у клиента включается при нажатии.
	if (ACharacter* MyCharacter = Cast<ACharacter>(InPawn))
	{
		MyCharacter->GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	}
}

void ADarcPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseTerminalUI(false);
	GetWorldTimerManager().ClearTimer(FallSafetyTimer);
	GetWorldTimerManager().ClearTimer(VoiceTimer);
	GetWorldTimerManager().ClearTimer(ShadowTimer);
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

void ADarcPlayerController::HandleDrop()
{
	if (UInteractionComponent* Interaction = GetPawn() ? GetPawn()->FindComponentByClass<UInteractionComponent>() : nullptr)
	{
		Interaction->DropHeldItem();
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
	// F: у «духа» — рубильник, у живого — фонарик.
	if (ADarcSpiritCharacter* Spirit = Cast<ADarcSpiritCharacter>(GetPawn()))
	{
		Spirit->TryToggleBreaker();
	}
	else if (UDarcFlashlightComponent* Flashlight = GetPawn() ? GetPawn()->FindComponentByClass<UDarcFlashlightComponent>() : nullptr)
	{
		Flashlight->Toggle();
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

	// Оформление из UI-пака D.A.R.C.: экран ЭЛТ с развёрткой и сканлайны поверх (картинки без текста —
	// весь текст окна из String Table). Нет картинок — остаётся тёмная подложка.
	auto SetupBrush = [this](FSlateBrush& Brush, const TCHAR* Path)
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path);
		if (Texture)
		{
			TerminalTextures.AddUnique(Texture);
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
			Brush.DrawAs = ESlateBrushDrawType::Image;
		}
		return Texture != nullptr;
	};
	const bool bScreen = SetupBrush(TerminalScreenBrush, TEXT("/Game/DARC/Imported/Textures/UI/DARC_UI_Pack/Crops/T_Screen_Scan.T_Screen_Scan"));
	const bool bScan = SetupBrush(TerminalScanBrush, TEXT("/Game/DARC/Imported/Textures/UI/DARC_UI_Pack/Crops/T_Scanlines.T_Scanlines"));
	const FLinearColor TextColor(0.72f, 0.88f, 1.f);

	TerminalWidget =
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(760.f)
			[
				// Корпус: тёмно-серая рамка, внутри — экран.
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0.09f, 0.1f, 0.12f, 0.98f))
				.Padding(10.f)
				[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(bScreen ? &TerminalScreenBrush : nullptr)
					.ColorAndOpacity(bScreen ? FLinearColor(0.55f, 0.6f, 0.7f, 1.f) : FLinearColor(0.f, 0.02f, 0.04f, 0.95f))
				]
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(bScan ? &TerminalScanBrush : nullptr)
					.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, bScan ? 0.12f : 0.f))
					.Visibility(EVisibility::HitTestInvisible)
				]
				+ SOverlay::Slot()
				.Padding(26.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
					[
						SAssignNew(TerminalScreen, STextBlock)
						.Text(BuildTerminalScreenText())
						.ColorAndOpacity(TextColor)
						.ShadowColorAndOpacity(FLinearColor(0.2f, 0.5f, 1.f, 0.35f))
						.ShadowOffset(FVector2D(0.f, 0.f))
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

void ADarcPlayerController::Client_OpenMissionSelect_Implementation()
{
	ADarcMissionTerminal::OpenSelectionUI(this);
}

void ADarcPlayerController::Server_SelectMission_Implementation(int32 Level)
{
	// Проверка на сервере: игрок стоит у терминала, уровень открыт (это проверяет куратор).
	const APawn* MyPawn = GetPawn();
	bool bNearTerminal = false;
	for (TActorIterator<ADarcMissionTerminal> It(GetWorld()); It && MyPawn; ++It)
	{
		bNearTerminal |= FVector::Dist(It->GetActorLocation(), MyPawn->GetActorLocation()) < 400.f;
	}
	if (ADarcBaseCurator* Curator = bNearTerminal ? ADarcBaseCurator::Find(this) : nullptr)
	{
		Curator->SelectLevel(Level);
	}
}
