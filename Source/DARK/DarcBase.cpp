// DarcBase.cpp
#include "DarcBase.h"
#include "DarcAssetSettings.h"
#include "DarcGameState.h"
#include "DarcGameplayLibrary.h"
#include "DarcPlayerController.h"
#include "DarcSpiritCharacter.h"
#include "DarcWorldMemorySubsystem.h"
#include "InteractableDoor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FText T(const TCHAR* Key) { return UDarcGameplayLibrary::UIText(Key); }

	/** Факт «уровень пройден» — по нему открываются повторы и меняется база (Arc1.Level7Done — свет под дверью). */
	FName LevelDoneFact(int32 Level) { return FName(*FString::Printf(TEXT("Arc1.Level%dDone"), Level)); }

	ADarcGameState* GetDarcGameState(const UObject* Context)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		return World ? World->GetGameState<ADarcGameState>() : nullptr;
	}

	bool IsAliveBody(const APawn* Pawn)
	{
		return Pawn && !Cast<ADarcSpiritCharacter>(Pawn);
	}
}

// ---------------------------------------------------------------------------
// Куратор
// ---------------------------------------------------------------------------

ADarcBaseCurator::ADarcBaseCurator()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Lobby = CreateDefaultSubobject<UBoxComponent>(TEXT("Lobby"));
	Lobby->SetBoxExtent(FVector(400.f, 400.f, 160.f));
	Lobby->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = Lobby;
}

ADarcBaseCurator* ADarcBaseCurator::Find(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ADarcBaseCurator> It(World);
	return It ? *It : nullptr; // нет куратора (выезд, а не база) — разыменовывать нельзя
}

void ADarcBaseCurator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADarcBaseCurator, bTaskGiven);
	DOREPLIFETIME(ADarcBaseCurator, StoryLevel);
	DOREPLIFETIME(ADarcBaseCurator, SelectedLevel);
}

const FDarcBaseMission* ADarcBaseCurator::FindMission(int32 Level) const
{
	return Missions.FindByPredicate([Level](const FDarcBaseMission& M) { return M.Level == Level; });
}

bool ADarcBaseCurator::IsLevelAvailable(int32 Level) const
{
	const FDarcBaseMission* Mission = FindMission(Level);
	return Mission && !Mission->Map.IsEmpty() && Level <= StoryLevel;
}

FString ADarcBaseCurator::GetNextMap() const
{
	const FDarcBaseMission* Mission = FindMission(SelectedLevel);
	return Mission ? Mission->Map : FString();
}

void ADarcBaseCurator::BeginPlay()
{
	Super::BeginPlay();
	RefreshScreens();
	if (!HasAuthority())
	{
		return;
	}

	UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this);
	const FDarcMissionSummary Summary = Memory ? Memory->GetLastMissionSummary() : FDarcMissionSummary();
	const bool bReturning = Memory && Memory->ConsumeMissionDebrief();

	// Возвращение: факты за выезд (открывают комнаты базы и повтор уровня) и реплики приёма данных.
	if (bReturning)
	{
		if (Summary.bSucceeded)
		{
			Memory->AddCampaignFact(LevelDoneFact(Summary.LevelIndex));
			if (const FDarcBaseMission* Done = FindMission(Summary.LevelIndex))
			{
				for (const FName& Fact : Done->Facts)
				{
					Memory->AddCampaignFact(Fact);
				}
				Script.Append(Done->Return);
			}
			// Последствие — один раз: факт остаётся в кампании навсегда, а реплика звучит только
			// при первом возвращении после него (отметка «<Факт>.Told»).
			for (const TPair<FName, FName>& FactLine : ReturnFactLines)
			{
				const FName Told(*(FactLine.Key.ToString() + TEXT(".Told")));
				if (Memory->HasCampaignFact(FactLine.Key) && !Memory->HasCampaignFact(Told))
				{
					Script.Add(FactLine.Value);
					Memory->AddCampaignFact(Told);
				}
			}
		}
		else
		{
			Script.Append(FailedReturn);
		}
	}

	// Следующий по сюжету — первый непройденный.
	StoryLevel = 1;
	while (Memory && Memory->HasCampaignFact(LevelDoneFact(StoryLevel)) && FindMission(StoryLevel + 1))
	{
		++StoryLevel;
	}
	SelectedLevel = StoryLevel;
	const FDarcBaseMission* Next = FindMission(StoryLevel);
	if (Next && !Next->Map.IsEmpty())
	{
		Script.Append(Next->Briefing);
	}
	else
	{
		// Следующий уровень ещё не сделан — можно повторить пройденные.
		while (SelectedLevel > 1 && !IsLevelAvailable(SelectedLevel))
		{
			--SelectedLevel;
		}
		Script.Add(TEXT("Curator_NoNewMissions"));
	}

	// Двери, которые открываются фактами, — перепроверить после выдачи фактов.
	for (TActorIterator<AInteractableDoor> It(GetWorld()); It; ++It)
	{
		It->RefreshUnlockFact();
	}

	Lobby->OnComponentBeginOverlap.AddDynamic(this, &ADarcBaseCurator::OnLobbyEnter);
	RefreshScreens();
}

void ADarcBaseCurator::OnLobbyEnter(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (bStarted || !IsAliveBody(Pawn) || !Pawn->IsPlayerControlled())
	{
		return;
	}
	bStarted = true;
	ScriptIndex = 0;
	GetWorldTimerManager().SetTimer(LineTimer, this, &ADarcBaseCurator::SayNextLine, 1.2f, false); // пауза: «зашли — заговорил»
}

void ADarcBaseCurator::SayLine(FName Key, float Duration)
{
	if (ADarcGameState* GS = GetDarcGameState(this))
	{
		GS->Say(T(TEXT("Curator_Name")), UDarcGameplayLibrary::UIText(Key), Duration, GetActorLocation(), SpeakerRadius);
	}
	// Голос — слот звука с именем реплики (пока не записан — тишина).
	UDarcAssetSettings::PlaySound(this, Key, GetActorLocation() + FVector(0.f, 0.f, 120.f));
}

void ADarcBaseCurator::SayNextLine()
{
	if (!Script.IsValidIndex(ScriptIndex))
	{
		// Разговор окончен: задание выдано, машина готова.
		bTaskGiven = true;
		if (ADarcGameState* GS = GetDarcGameState(this))
		{
			GS->Say(FText::GetEmpty(), T(TEXT("Base_GoToCar")), 6.f, GetActorLocation(), 0.f);
		}
		return;
	}
	const FName Key = Script[ScriptIndex++];
	// Пустой ключ в списке — пауза (куратор молчит).
	const FString Line = Key.IsNone() ? FString() : UDarcGameplayLibrary::UIText(Key).ToString();
	const float Duration = Key.IsNone() ? 2.f : FMath::Clamp(1.4f + Line.Len() * 0.055f, 2.f, 9.f);
	if (!Key.IsNone())
	{
		SayLine(Key, Duration);
	}
	GetWorldTimerManager().SetTimer(LineTimer, this, &ADarcBaseCurator::SayNextLine, Duration + 0.4f, false);
}

void ADarcBaseCurator::SelectLevel(int32 Level)
{
	if (!HasAuthority() || Level == SelectedLevel || !IsLevelAvailable(Level))
	{
		return;
	}
	SelectedLevel = Level;
	RefreshScreens();
	SayLine(Level == StoryLevel ? TEXT("Curator_SelectStory") : TEXT("Curator_SelectReplay"), 4.f);
}

void ADarcBaseCurator::OnRep_Selected()
{
	RefreshScreens();
}

void ADarcBaseCurator::RefreshScreens() const
{
	// Экран лобби: надписи с тегом DARC.MissionScreen (их ставит скрипт сборки), у каждой машины — свои.
	const FText Text = FText::Format(T(TEXT("Base_ScreenFormat")), FText::AsNumber(SelectedLevel),
		T(*FString::Printf(TEXT("Mission_%d_Name"), SelectedLevel)));
	for (TActorIterator<ATextRenderActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("DARC.MissionScreen")))
		{
			It->GetTextRender()->SetText(Text);
		}
	}
}

// ---------------------------------------------------------------------------
// Машина
// ---------------------------------------------------------------------------

ADarcBaseCar::ADarcBaseCar()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetBoxExtent(FVector(260.f, 110.f, 90.f));
	Body->SetCollisionProfileName(TEXT("NoCollision"));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // по ней наводится взгляд для E
	RootComponent = Body;
}

void ADarcBaseCar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADarcBaseCar, Seated);
}

bool ADarcBaseCar::CanInteract_Implementation(AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const ADarcBaseCurator* Curator = ADarcBaseCurator::Find(this);
	return IsAliveBody(Pawn) && Curator && Curator->bTaskGiven && !bDeparting && !Seated.Contains(Pawn->GetPlayerState());
}

FText ADarcBaseCar::GetInteractionPrompt_Implementation() const
{
	return T(TEXT("Base_CarEnter"));
}

void ADarcBaseCar::OnInteract_Implementation(AActor* Interactor)
{
	ACharacter* Character = Cast<ACharacter>(Interactor);
	if (!Character || !CanInteract_Implementation(Interactor))
	{
		return;
	}
	// Сел: прячем персонажа внутри машины, ходить больше нельзя (смотреть по сторонам — можно).
	Seated.AddUnique(Character->GetPlayerState());
	Character->GetCharacterMovement()->DisableMovement();
	Character->SetActorEnableCollision(false);
	Character->SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, 40.f));
	Character->SetActorHiddenInGame(true);
	CheckDeparture();
}

void ADarcBaseCar::CheckDeparture()
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS || bDeparting)
	{
		return;
	}
	int32 Alive = 0;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		Alive += (PS && IsAliveBody(PS->GetPawn())) ? 1 : 0;
	}
	int32 Ready = 0;
	for (const APlayerState* PS : Seated)
	{
		Ready += (PS && GS->PlayerArray.Contains(PS)) ? 1 : 0;
	}
	if (Ready < Alive)
	{
		if (ADarcGameState* DarcGS = GetDarcGameState(this))
		{
			DarcGS->Say(FText::GetEmpty(), FText::Format(T(TEXT("Base_CarWaiting")), Ready, Alive), 4.f, GetActorLocation(), 0.f);
		}
		return;
	}
	bDeparting = true;
	Multicast_FadeOut();
	GetWorldTimerManager().SetTimer(DepartTimer, this, &ADarcBaseCar::Travel, 2.5f, false);
}

void ADarcBaseCar::Multicast_FadeOut_Implementation()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController() && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 2.f, FLinearColor::Black, false, true);
		}
	}
}

void ADarcBaseCar::Travel()
{
	const ADarcBaseCurator* Curator = ADarcBaseCurator::Find(this);
	const FString Map = Curator ? Curator->GetNextMap() : FString();
	if (Map.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("База: у выбранного уровня нет карты (DefaultGame.ini, DarcBaseCurator)"));
		bDeparting = false;
		return;
	}
	// Весь сервер переезжает вместе с клиентами; хост остаётся хостом.
	const bool bListen = GetNetMode() == NM_ListenServer;
	GetWorld()->ServerTravel(Map + (bListen ? TEXT("?listen") : TEXT("")), true);
}

// ---------------------------------------------------------------------------
// Терминал выбора выезда
// ---------------------------------------------------------------------------

namespace
{
	class SDarcMissionSelect : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDarcMissionSelect) {}
			SLATE_ARGUMENT(TWeakObjectPtr<ADarcPlayerController>, Owner)
			SLATE_EVENT(FSimpleDelegate, OnClose)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Owner = InArgs._Owner;
			OnClose = InArgs._OnClose;
			const ADarcBaseCurator* Curator = ADarcBaseCurator::Find(Owner.Get());
			const FLinearColor TextColor(0.82f, 0.85f, 0.9f);

			TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
			for (int32 Level = 1; Curator && Curator->FindMission(Level); ++Level)
			{
				const bool bAvailable = Curator->IsLevelAvailable(Level);
				const bool bDone = Level < Curator->StoryLevel;
				const FText Name = bAvailable
					? FText::Format(T(TEXT("Base_MissionLine")), FText::AsNumber(Level), T(*FString::Printf(TEXT("Mission_%d_Name"), Level)),
						T(bDone ? TEXT("Base_MissionDone") : TEXT("Base_MissionNext")))
					: FText::Format(T(TEXT("Base_MissionClassified")), FText::AsNumber(Level));
				List->AddSlot().AutoHeight().Padding(0.f, 3.f)
					[
						SNew(SButton)
						.IsEnabled(bAvailable)
						.ContentPadding(FMargin(16.f, 8.f))
						.OnClicked_Lambda([this, Level]()
						{
							if (ADarcPlayerController* PC = Owner.Get())
							{
								PC->Server_SelectMission(Level);
							}
							OnClose.ExecuteIfBound();
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Text(Name).Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
							.ColorAndOpacity_Lambda([Level, TextColor, this]()
							{
								const ADarcBaseCurator* C = ADarcBaseCurator::Find(Owner.Get());
								return FSlateColor(C && C->SelectedLevel == Level ? FLinearColor(0.45f, 0.65f, 1.f) : TextColor);
							})
						]
					];
			}

			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.85f))
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(620.f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.012f, 0.014f, 0.018f, 0.98f))
						.Padding(32.f, 26.f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
							[
								SNew(STextBlock).Text(T(TEXT("Base_MissionTitle"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 26)).ColorAndOpacity(TextColor)
							]
							+ SVerticalBox::Slot().AutoHeight()[List]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 18.f, 0.f, 0.f)
							[
								SNew(SButton).ContentPadding(FMargin(18.f, 8.f))
								.OnClicked_Lambda([this]() { OnClose.ExecuteIfBound(); return FReply::Handled(); })
								[
									SNew(STextBlock).Text(T(TEXT("Settings_Back"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 15)).ColorAndOpacity(TextColor)
								]
							]
						]
					]
				]
			];
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override
		{
			if (KeyEvent.GetKey() == EKeys::Escape)
			{
				OnClose.ExecuteIfBound();
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

	private:
		TWeakObjectPtr<ADarcPlayerController> Owner;
		FSimpleDelegate OnClose;
	};

	TWeakPtr<SWidget> OpenSelection;
}

ADarcMissionTerminal::ADarcMissionTerminal()
{
	bReplicates = true;
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetBoxExtent(FVector(40.f, 60.f, 60.f));
	Body->SetCollisionProfileName(TEXT("NoCollision"));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = Body;
}

bool ADarcMissionTerminal::CanInteract_Implementation(AActor* Interactor) const
{
	return IsAliveBody(Cast<APawn>(Interactor)) && ADarcBaseCurator::Find(this) != nullptr;
}

FText ADarcMissionTerminal::GetInteractionPrompt_Implementation() const
{
	return T(TEXT("Base_MissionPrompt"));
}

void ADarcMissionTerminal::OnInteract_Implementation(AActor* Interactor)
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	if (ADarcPlayerController* PC = Pawn ? Cast<ADarcPlayerController>(Pawn->GetController()) : nullptr)
	{
		PC->Client_OpenMissionSelect();
	}
}

void ADarcMissionTerminal::OpenSelectionUI(APlayerController* Owner)
{
	ADarcPlayerController* PC = Cast<ADarcPlayerController>(Owner);
	UGameViewportClient* Viewport = PC && PC->GetWorld() ? PC->GetWorld()->GetGameViewport() : nullptr;
	if (!Viewport || OpenSelection.IsValid())
	{
		return;
	}
	TWeakObjectPtr<ADarcPlayerController> WeakPC = PC;
	TWeakObjectPtr<UGameViewportClient> WeakViewport = Viewport;
	TSharedRef<SWidget> Widget = SNew(SDarcMissionSelect)
		.Owner(WeakPC)
		.OnClose_Lambda([WeakPC, WeakViewport]()
		{
			if (TSharedPtr<SWidget> Pinned = OpenSelection.Pin())
			{
				if (UGameViewportClient* VP = WeakViewport.Get())
				{
					VP->RemoveViewportWidgetContent(Pinned.ToSharedRef());
				}
			}
			OpenSelection.Reset();
			if (ADarcPlayerController* Controller = WeakPC.Get())
			{
				Controller->SetShowMouseCursor(false);
				Controller->SetInputMode(FInputModeGameOnly());
			}
		});
	Viewport->AddViewportWidgetContent(Widget, 900);
	OpenSelection = Widget;
	PC->SetShowMouseCursor(true);
	PC->SetInputMode(FInputModeUIOnly());
	FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
}

// ---------------------------------------------------------------------------
// Дверь со светом
// ---------------------------------------------------------------------------

ADarcLightDoor::ADarcLightDoor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	Glow = CreateDefaultSubobject<URectLightComponent>(TEXT("Glow"));
	RootComponent = Glow;
	Glow->SetIntensityUnits(ELightUnits::Lumens);
	Glow->SetIntensity(180.f);
	Glow->SetLightColor(FLinearColor(0.75f, 0.85f, 1.f));
	Glow->SetSourceWidth(90.f);
	Glow->SetSourceHeight(2.f);
	Glow->SetAttenuationRadius(320.f);
	Glow->SetBarnDoorAngle(70.f);
	Glow->SetCastShadows(false);
}

void ADarcLightDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADarcLightDoor, bSteady);
}

void ADarcLightDoor::BeginPlay()
{
	Super::BeginPlay();
	BaseIntensity = Glow->Intensity;
	if (HasAuthority())
	{
		const UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this);
		bSteady = Memory && Memory->HasCampaignFact(SteadyFact);
	}
	OnRep_Steady();
}

void ADarcLightDoor::OnRep_Steady()
{
	GetWorldTimerManager().ClearTimer(FlickerTimer);
	if (bSteady)
	{
		Glow->SetIntensity(BaseIntensity);
		if (!Hum)
		{
			Hum = UDarcAssetSettings::PlayLoopAttached(TEXT("LightDoorHum"), RootComponent);
		}
		return;
	}
	// До 7-го уровня: свет из-под двери лишь изредка вспыхивает.
	Glow->SetIntensity(0.f);
	GetWorldTimerManager().SetTimer(FlickerTimer, this, &ADarcLightDoor::Flicker, FMath::FRandRange(6.f, 16.f), false);
}

void ADarcLightDoor::Flicker()
{
	const bool bOn = Glow->Intensity <= KINDA_SMALL_NUMBER;
	Glow->SetIntensity(bOn ? BaseIntensity * FMath::FRandRange(0.4f, 1.f) : 0.f);
	const float Next = bOn ? FMath::FRandRange(0.06f, 0.5f) : FMath::FRandRange(5.f, 18.f);
	GetWorldTimerManager().SetTimer(FlickerTimer, this, &ADarcLightDoor::Flicker, Next, false);
}

void ADarcLightDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Hum)
	{
		return;
	}
	// Гул за дверью смолкает, когда к ней подходят (у каждой машины по своему игроку).
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const bool bNear = Pawn && FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) < QuietDistance;
	Hum->SetVolumeMultiplier(FMath::FInterpTo(Hum->VolumeMultiplier, bNear ? 0.f : 1.f, DeltaSeconds, 3.f));
}
