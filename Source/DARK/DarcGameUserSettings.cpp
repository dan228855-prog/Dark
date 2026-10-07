// DarcGameUserSettings.cpp
#include "DarcGameUserSettings.h"
#include "DarcPlayerController.h"
#include "DarcGameplayLibrary.h"
#include "AudioDevice.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Scalability.h"
#if DARC_WITH_NVIDIA
#include "DLSSLibrary.h"
#include "StreamlineLibraryDLSSG.h"
#include "StreamlineLibraryReflex.h"
#endif

namespace
{
	// Ассеты создаёт Tools/darc_setup_audio.py. Нет ассета — категория просто не регулируется.
	const TCHAR* VolumeMixPath = TEXT("/Game/DARC/Audio/SM_DarcVolumes.SM_DarcVolumes");
	const TCHAR* VolumeClassPaths[] = {
		TEXT("/Game/DARC/Audio/SC_Music.SC_Music"),
		TEXT("/Game/DARC/Audio/SC_SFX.SC_SFX"),
		TEXT("/Game/DARC/Audio/SC_Voice.SC_Voice"),
		TEXT("/Game/DARC/Audio/SC_Radio.SC_Radio"),
	};

	// Таблицы строк: русский — источник, английский — перевод тех же ключей.
	const TCHAR* StringTableRu = TEXT("Localization/ST_UI.csv");
	const TCHAR* StringTableEn = TEXT("Localization/ST_UI_EN.csv");
}

UDarcGameUserSettings* UDarcGameUserSettings::Get()
{
	return GEngine ? Cast<UDarcGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UDarcGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	Super::ApplySettings(bCheckForCommandLineOverrides); // применяет разрешение/качество и сохраняет ini
	ApplyDarcSettings();
}

void UDarcGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	const UDarcGameUserSettings* Defaults = GetDefault<UDarcGameUserSettings>();
	MouseSensitivity = Defaults->MouseSensitivity;
	bInvertMouseY = Defaults->bInvertMouseY;
	FieldOfView = Defaults->FieldOfView;
	MasterVolume = Defaults->MasterVolume;
	MusicVolume = Defaults->MusicVolume;
	EffectsVolume = Defaults->EffectsVolume;
	VoiceVolume = Defaults->VoiceVolume;
	RadioVolume = Defaults->RadioVolume;
	Brightness = Defaults->Brightness;
	bOpenMic = Defaults->bOpenMic;
	MicGain = Defaults->MicGain;
	Language = Defaults->Language;
	Upscaler = Defaults->Upscaler;
	UpscalerQuality = Defaults->UpscalerQuality;
	FrameGeneration = Defaults->FrameGeneration;
	ReflexMode = Defaults->ReflexMode;
	AntiAliasingMethod = Defaults->AntiAliasingMethod;
	KeyBindings.Reset();
}

const TArray<FDarcKeyAction>& UDarcGameUserSettings::GetKeyActions()
{
	static const TArray<FDarcKeyAction> Actions = {
		{ TEXT("MoveForward"), EKeys::W, TEXT("Key_MoveForward"), true },
		{ TEXT("MoveBack"), EKeys::S, TEXT("Key_MoveBack"), true },
		{ TEXT("MoveLeft"), EKeys::A, TEXT("Key_MoveLeft"), true },
		{ TEXT("MoveRight"), EKeys::D, TEXT("Key_MoveRight"), true },
		{ TEXT("Jump"), EKeys::SpaceBar, TEXT("Key_Jump"), true },
		{ TEXT("Sprint"), EKeys::LeftShift, TEXT("Key_Sprint"), false },
		{ TEXT("Crouch"), EKeys::LeftControl, TEXT("Key_Crouch"), false },
		{ TEXT("Interact"), EKeys::E, TEXT("Key_Interact"), false },
		{ TEXT("Grab"), EKeys::LeftMouseButton, TEXT("Key_Grab"), false },
		{ TEXT("Push"), EKeys::RightMouseButton, TEXT("Key_Push"), false },
		{ TEXT("Flashlight"), EKeys::F, TEXT("Key_Flashlight"), false },
		{ TEXT("Drop"), EKeys::G, TEXT("Key_Drop"), false },
		{ TEXT("Tasks"), EKeys::Tab, TEXT("Key_Tasks"), false },
		{ TEXT("Talk"), EKeys::V, TEXT("Key_Talk"), false },
		{ TEXT("Slot1"), EKeys::One, TEXT("Key_Slot1"), false },
		{ TEXT("Slot2"), EKeys::Two, TEXT("Key_Slot2"), false },
		{ TEXT("Slot3"), EKeys::Three, TEXT("Key_Slot3"), false },
		{ TEXT("Slot4"), EKeys::Four, TEXT("Key_Slot4"), false },
	};
	return Actions;
}

FKey UDarcGameUserSettings::GetKey(FName ActionId) const
{
	if (const FKey* Custom = KeyBindings.Find(ActionId); Custom && Custom->IsValid())
	{
		return *Custom;
	}
	for (const FDarcKeyAction& Action : GetKeyActions())
	{
		if (Action.Id == ActionId)
		{
			return Action.DefaultKey;
		}
	}
	return EKeys::Invalid;
}

FString UDarcGameUserSettings::GetEffectiveLanguage() const
{
	if (Language == TEXT("ru") || Language == TEXT("en"))
	{
		return Language;
	}
	// По умолчанию — язык системы: русский для ru/uk/be/kk, иначе английский.
	const FString System = FInternationalization::Get().GetDefaultLanguage()->GetTwoLetterISOLanguageName();
	return (System == TEXT("ru") || System == TEXT("uk") || System == TEXT("be") || System == TEXT("kk")) ? TEXT("ru") : TEXT("en");
}

float UDarcGameUserSettings::GetVolume(EDarcVolume Category) const
{
	switch (Category)
	{
	case EDarcVolume::Music: return MusicVolume;
	case EDarcVolume::Effects: return EffectsVolume;
	case EDarcVolume::Voice: return VoiceVolume;
	case EDarcVolume::Radio: return RadioVolume;
	}
	return 1.f;
}

void UDarcGameUserSettings::SetVolume(EDarcVolume Category, float Value)
{
	Value = FMath::Clamp(Value, 0.f, 1.f);
	switch (Category)
	{
	case EDarcVolume::Music: MusicVolume = Value; break;
	case EDarcVolume::Effects: EffectsVolume = Value; break;
	case EDarcVolume::Voice: VoiceVolume = Value; break;
	case EDarcVolume::Radio: RadioVolume = Value; break;
	}
}

void UDarcGameUserSettings::ApplyDarcSettings()
{
	if (!GEngine)
	{
		return;
	}
	if (FAudioDeviceHandle Audio = GEngine->GetMainAudioDevice())
	{
		Audio->SetTransientPrimaryVolume(FMath::Clamp(MasterVolume, 0.f, 1.f));
	}
	GEngine->DisplayGamma = FMath::Clamp(Brightness, MinBrightness, MaxBrightness);
	if (IConsoleVariable* Gain = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.MicInputGain")))
	{
		Gain->Set(FMath::Clamp(MicGain, MinMicGain, MaxMicGain), ECVF_SetByGameSetting);
	}
	if (AntiAliasingMethod >= 0)
	{
		if (IConsoleVariable* Method = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
		{
			Method->Set(AntiAliasingMethod, ECVF_SetByGameSetting);
		}
	}
	ApplyUpscaling();
	ApplyLanguage();

	// Уже идущая игра (настройки меняют из паузы): громкость, поле зрения, клавиши, голос — сразу.
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || !World->IsGameWorld())
		{
			continue;
		}
		ApplyVolumes(World);
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC || !PC->IsLocalController())
			{
				continue;
			}
			ApplyFieldOfView(PC->GetPawn());
			if (ADarcPlayerController* DarcPC = Cast<ADarcPlayerController>(PC))
			{
				DarcPC->ApplyKeyBindings();
				DarcPC->ApplyVoiceMode();
			}
		}
	}
}

void UDarcGameUserSettings::ApplyVolumes(UWorld* World) const
{
	USoundMix* Mix = LoadObject<USoundMix>(nullptr, VolumeMixPath);
	if (!World || !Mix)
	{
		return;
	}
	UGameplayStatics::PushSoundMixModifier(World, Mix);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(VolumeClassPaths); ++Index)
	{
		if (USoundClass* Class = LoadObject<USoundClass>(nullptr, VolumeClassPaths[Index]))
		{
			UGameplayStatics::SetSoundMixClassOverride(World, Mix, Class, GetVolume(static_cast<EDarcVolume>(Index)), 1.f, 0.f, true);
		}
	}
}

void UDarcGameUserSettings::ApplyFieldOfView(APawn* Pawn) const
{
	if (!Pawn)
	{
		return;
	}
	if (UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>())
	{
		Camera->SetFieldOfView(FMath::Clamp(FieldOfView, MinFov, MaxFov));
	}
}

void UDarcGameUserSettings::ApplyLanguage() const
{
	// Собираем ST_UI заново: русские строки, поверх — английские (если выбран английский).
	// Тексты, уже взятые из таблицы, обновятся сами после RefreshResources.
	const FString Lang = GetEffectiveLanguage();
	static FString AppliedLang;
	if (AppliedLang == Lang)
	{
		return;
	}
	AppliedLang = Lang;

	FStringTableRef Table = FStringTable::NewStringTable();
	Table->SetNamespace(TEXT("ST_UI"));
	Table->ImportStringsFromCSVFile(FPaths::ProjectContentDir() / StringTableRu);
	if (Lang == TEXT("en"))
	{
		FStringTableRef English = FStringTable::NewStringTable();
		if (English->ImportStringsFromCSVFile(FPaths::ProjectContentDir() / StringTableEn))
		{
			English->EnumerateSourceStrings([&Table](const FString& Key, const FString& Text)
			{
				if (!Text.IsEmpty())
				{
#if WITH_EDITORONLY_DATA
					Table->SetSourceString(Key, Text, FString());
#else
					Table->SetSourceString(Key, Text);
#endif
				}
				return true;
			});
		}
	}
	FStringTableRegistry::Get().UnregisterStringTable(TEXT("ST_UI"));
	FStringTableRegistry::Get().RegisterStringTable(TEXT("ST_UI"), Table);
	FTextLocalizationManager::Get().RefreshResources();
}

bool UDarcGameUserSettings::IsDLSSAvailable()
{
#if DARC_WITH_NVIDIA
	return UDLSSLibrary::IsDLSSSupported();
#else
	return false;
#endif
}

bool UDarcGameUserSettings::IsReflexAvailable()
{
#if DARC_WITH_NVIDIA
	return UStreamlineLibraryReflex::IsReflexSupported();
#else
	return false;
#endif
}

TArray<int32> UDarcGameUserSettings::GetFrameGenerationOptions(int32 ForUpscaler)
{
	TArray<int32> Options = { 0 };
	if (ForUpscaler == 2)
	{
		Options.Add(1); // FSR: генерация кадров работает вместе с масштабированием FSR, на любой видеокарте с DX12
		return Options;
	}
#if DARC_WITH_NVIDIA
	if (UStreamlineLibraryDLSSG::IsDLSSGSupported())
	{
		for (const EStreamlineDLSSGMode Mode : UStreamlineLibraryDLSSG::GetSupportedDLSSGModes())
		{
			if (Mode != EStreamlineDLSSGMode::Off && Mode != EStreamlineDLSSGMode::Auto)
			{
				Options.Add(static_cast<int32>(Mode));
			}
		}
	}
#endif
	return Options;
}

FText UDarcGameUserSettings::GetFrameGenerationName(int32 Mode)
{
	switch (Mode)
	{
	case 0: return UDarcGameplayLibrary::UIText(TEXT("Settings_Off"));
	case 1: return FText::FromString(TEXT("FSR"));
	case 241: return UDarcGameplayLibrary::UIText(TEXT("Settings_FGDynamic"));
	case 17: return FText::FromString(TEXT("DLSS 2X"));
	case 23: return FText::FromString(TEXT("DLSS 3X"));
	case 31: return FText::FromString(TEXT("DLSS 4X"));
	case 37: return FText::FromString(TEXT("DLSS 5X"));
	case 41: return FText::FromString(TEXT("DLSS 6X"));
	default: return FText::AsNumber(Mode);
	}
}

void UDarcGameUserSettings::ApplyUpscaling()
{
	auto SetCVar = [](const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByGameSetting);
		}
	};
	const bool bDLSS = Upscaler == 1 && IsDLSSAvailable();
	const bool bFSR = Upscaler == 2;
	const int32 Quality = FMath::Clamp(UpscalerQuality, 0, 4);

	// FSR: режимы 0..4 совпадают с нашими (0 — Native AA ... 4 — Ultra Performance).
	SetCVar(TEXT("r.FidelityFX.FSR.Enabled"), bFSR ? 1 : 0);
	SetCVar(TEXT("r.FidelityFX.FSR.QualityMode"), Quality);
	SetCVar(TEXT("r.FidelityFX.FI.Enabled"), (bFSR && FrameGeneration == 1) ? 1 : 0);

#if DARC_WITH_NVIDIA
	// Настройки применяются и при запуске движка — функции DLSS доступны только после его инициализации.
	if (!GEngine || !GEngine->IsInitialized())
	{
		return;
	}
	static bool bDLSSWasOn = false;
	if (bDLSS)
	{
		static const UDLSSMode Modes[] = { UDLSSMode::DLAA, UDLSSMode::Quality, UDLSSMode::Balanced, UDLSSMode::Performance, UDLSSMode::UltraPerformance };
		UDLSSLibrary::SetDLSSMode(GEngine ? GEngine->GetWorld() : nullptr, Modes[Quality]);
		bDLSSWasOn = true;
	}
	else if (bDLSSWasOn || UDLSSLibrary::GetDLSSMode() != UDLSSMode::Off)
	{
		UDLSSLibrary::SetDLSSMode(GEngine ? GEngine->GetWorld() : nullptr, UDLSSMode::Off);
		bDLSSWasOn = false;
		// DLSS сам менял масштаб экрана — вернуть наш (масштаб разрешения из окна настроек).
		Scalability::SetQualityLevels(ScalabilityQuality);
	}

	if (UStreamlineLibraryDLSSG::IsDLSSGSupported())
	{
		const bool bDLSSG = !bFSR && FrameGeneration > 1;
		UStreamlineLibraryDLSSG::SetDLSSGMode(bDLSSG ? static_cast<EStreamlineDLSSGMode>(FrameGeneration) : EStreamlineDLSSGMode::Off);
	}
	if (UStreamlineLibraryReflex::IsReflexSupported())
	{
		UStreamlineLibraryReflex::SetReflexMode(static_cast<EStreamlineReflexMode>(ReflexMode == 3 ? 3 : (ReflexMode ? 1 : 0)));
	}
#endif
}
