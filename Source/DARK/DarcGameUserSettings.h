// DarcGameUserSettings.h
// Настройки игрока поверх стандартных движка (разрешение, режим окна, качество, VSync, лимит FPS):
// мышь, поле зрения, громкость по категориям, яркость, голосовая связь, клавиши, язык.
// Хранятся в Saved/Config/<платформа>/GameUserSettings.ini, класс подключён в DefaultEngine.ini
// (GameUserSettingsClassName). Окно — SDarcSettingsMenu.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "DarcGameUserSettings.generated.h"

class APawn;
class UWorld;

/** Категории громкости. Классы звука создаёт Tools/darc_setup_audio.py (/Game/DARC/Audio). */
UENUM()
enum class EDarcVolume : uint8
{
	Music,
	Effects,
	Voice, // голоса напарников (голосовой чат)
	Radio, // радио в машине, переговоры по рации
};

/** Действие, которое игрок может переназначить. */
struct FDarcKeyAction
{
	FName Id;
	FKey DefaultKey;
	const TCHAR* LabelKey; // ключ в ST_UI
	bool bEnhancedInput;   // клавиша из раскладки шаблона (ходьба, прыжок), а не прямая привязка
};

UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class DARK_API UDarcGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	static UDarcGameUserSettings* Get();

	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;
	virtual void SetToDefaults() override;

	/** Применить наше (громкость, яркость, язык, поле зрения, клавиши и голос у своих контроллеров). */
	void ApplyDarcSettings();

	/** Громкость по категориям — микс заново на каждой карте (его вызывает UDarcGameInstance). */
	void ApplyVolumes(UWorld* World) const;

	/** Поле зрения камеры персонажа (вызывается, когда контроллер получает персонажа). */
	void ApplyFieldOfView(APawn* Pawn) const;

	/** DLSS / FSR / TSR, генерация кадров, Reflex. */
	void ApplyUpscaling();

	/** Таблица строк ST_UI на выбранном языке (пустой ключ в переводе — берётся русский). */
	void ApplyLanguage() const;

	/** Действия для переназначения (порядок — как в окне настроек). */
	static const TArray<FDarcKeyAction>& GetKeyActions();

	/** Текущая клавиша действия (своя или по умолчанию). */
	FKey GetKey(FName ActionId) const;

	/** «ru»/«en»: выбранный или по языку системы. */
	FString GetEffectiveLanguage() const;

	float GetVolume(EDarcVolume Category) const;
	void SetVolume(EDarcVolume Category, float Value);

	/** Множитель поворота камеры мышью/стиком. */
	UPROPERTY(Config)
	float MouseSensitivity = 1.f;

	UPROPERTY(Config)
	bool bInvertMouseY = false;

	/** Поле зрения по горизонтали, градусы. */
	UPROPERTY(Config)
	float FieldOfView = 90.f;

	/** Общая громкость 0..1. */
	UPROPERTY(Config)
	float MasterVolume = 1.f;

	UPROPERTY(Config)
	float MusicVolume = 1.f;

	UPROPERTY(Config)
	float EffectsVolume = 1.f;

	UPROPERTY(Config)
	float VoiceVolume = 1.f;

	UPROPERTY(Config)
	float RadioVolume = 1.f;

	/** Яркость — гамма экрана (движок по умолчанию 2.2). */
	UPROPERTY(Config)
	float Brightness = 2.2f;

	/** Сглаживание (r.AntiAliasingMethod): -1 — как в проекте, 0 нет, 1 FXAA, 2 TAA, 4 TSR. */
	UPROPERTY(Config)
	int32 AntiAliasingMethod = -1;

	/** Масштабирование: 0 — TSR (движок), 1 — NVIDIA DLSS, 2 — AMD FSR. */
	UPROPERTY(Config)
	int32 Upscaler = 0;

	/** Режим масштабирования: 0 — родное разрешение (DLAA / Native AA), 1 качество, 2 баланс,
	    3 производительность, 4 макс. производительность. */
	UPROPERTY(Config)
	int32 UpscalerQuality = 1;

	/** Генерация кадров: 0 — выкл., 1 — FSR, остальное — режим DLSS (EStreamlineDLSSGMode). */
	UPROPERTY(Config)
	int32 FrameGeneration = 0;

	/** NVIDIA Reflex: 0 — выкл., 1 — вкл., 3 — вкл. + ускорение. */
	UPROPERTY(Config)
	int32 ReflexMode = 1;

	/** Что доступно на этой видеокарте (для окна настроек). */
	static bool IsDLSSAvailable();
	static bool IsReflexAvailable();
	/** Варианты генерации кадров для выбранного масштабирования (первый — 0, выкл.). */
	static TArray<int32> GetFrameGenerationOptions(int32 ForUpscaler);
	static FText GetFrameGenerationName(int32 Mode);

	/** Открытый микрофон (голос идёт всегда) вместо «нажми и говори». */
	UPROPERTY(Config)
	bool bOpenMic = false;

	/** Усиление микрофона (voice.MicInputGain). */
	UPROPERTY(Config)
	float MicGain = 1.f;

	/** Язык интерфейса: "ru", "en" или пусто — по языку системы. */
	UPROPERTY(Config)
	FString Language;

	/** Переназначенные клавиши: только отличающиеся от умолчания. */
	UPROPERTY(Config)
	TMap<FName, FKey> KeyBindings;

	static constexpr float MinSensitivity = 0.2f;
	static constexpr float MaxSensitivity = 3.f;
	static constexpr float MinFov = 70.f;
	static constexpr float MaxFov = 110.f;
	static constexpr float MinBrightness = 1.6f;
	static constexpr float MaxBrightness = 3.f;
	static constexpr float MinMicGain = 0.25f;
	static constexpr float MaxMicGain = 3.f;
};
