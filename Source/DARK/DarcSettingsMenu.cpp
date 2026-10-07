// DarcSettingsMenu.cpp
#include "DarcSettingsMenu.h"
#include "DarcGameUserSettings.h"
#include "DarcGameplayLibrary.h"
#include "DarcPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameModeBase.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FText T(const TCHAR* Key) { return UDarcGameplayLibrary::UIText(Key); }

	const FLinearColor TextColor(0.82f, 0.85f, 0.9f);
	const FLinearColor DimColor(0.45f, 0.5f, 0.58f);
	const FLinearColor AccentColor(0.45f, 0.65f, 1.f);

	FSlateFontInfo Font(int32 Size, const TCHAR* Style = TEXT("Regular"))
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}

	// Затемнение всего экрана + панель по центру.
	TSharedRef<SWidget> MakeFrame(float Width, const TSharedRef<SWidget>& Content)
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.9f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(Width)
				.MaxDesiredHeight(900.f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.012f, 0.014f, 0.018f, 0.98f))
					.Padding(36.f, 28.f)
					[
						Content
					]
				]
			];
	}

	TSharedRef<SWidget> MakeButton(const FText& Text, TFunction<void()> OnClick, float MinWidth = 0.f)
	{
		return SNew(SBox).MinDesiredWidth(MinWidth)
			[
				SNew(SButton)
				.ContentPadding(FMargin(18.f, 8.f))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
				[
					SNew(STextBlock).Text(Text).Font(Font(15, TEXT("Bold"))).ColorAndOpacity(TextColor)
				]
			];
	}

	// Пункты качества графики (уровни движка 0..4) по порядку строк окна.
	enum EQualityRow : int32 { QShadows, QViewDistance, QTextures, QEffects, QPostProcess, QFoliage, QShading, QGlobalIllumination, QReflections, QAntiAliasing, QCount };

	class SDarcSettingsMenu : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDarcSettingsMenu) {}
			SLATE_EVENT(FSimpleDelegate, OnClose)
			SLATE_EVENT(FSimpleDelegate, OnLanguageChanged)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			OnClose = InArgs._OnClose;
			OnLanguageChanged = InArgs._OnLanguageChanged;
			LoadFrom(UDarcGameUserSettings::Get());

			// Разрешения монитора; текущее — обязательно в списке.
			UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
			Resolutions.AddUnique(Resolution);
			Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });

			TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);

			AddHeader(Rows, T(TEXT("Settings_Interface")));
			AddChoice(Rows, T(TEXT("Settings_Language")),
				[this]() { return T(Language == TEXT("en") ? TEXT("Settings_LangEn") : TEXT("Settings_LangRu")); },
				[this](int32) { Language = Language == TEXT("en") ? TEXT("ru") : TEXT("en"); });

			AddHeader(Rows, T(TEXT("Settings_Graphics")));
			AddChoice(Rows, T(TEXT("Settings_WindowMode")),
				[this]() { return WindowModeText(); },
				[this](int32 Dir) { WindowMode = (WindowMode + Dir + 3) % 3; });
			AddChoice(Rows, T(TEXT("Settings_Resolution")),
				[this]() { return FText::FromString(FString::Printf(TEXT("%d × %d"), Resolution.X, Resolution.Y)); },
				[this](int32 Dir) { Step(Resolutions, Resolution, Dir); });
			AddChoice(Rows, T(TEXT("Settings_Quality")),
				[this]() { const int32 P = GetPreset(); return P < 0 ? T(TEXT("Settings_QCustom")) : LevelText(P); },
				[this](int32 Dir) { const int32 P = GetPreset(); SetPreset(FMath::Clamp((P < 0 ? 3 : P) + Dir, 0, 4)); });
			AddCheck(Rows, T(TEXT("Settings_VSync")), &bVSync);
			AddChoice(Rows, T(TEXT("Settings_FpsLimit")),
				[this]() { return FpsLimit <= 0 ? T(TEXT("Settings_FpsUnlimited")) : FText::AsNumber(FMath::RoundToInt(FpsLimit)); },
				[this](int32 Dir) { Step(FpsOptions, FpsLimit, Dir); });
			AddChoice(Rows, T(TEXT("Settings_Upscaler")),
				[this]() { return UpscalerText(Upscaler); },
				[this](int32 Dir)
				{
					Step(Upscalers, Upscaler, Dir);
					// Генерация кадров у DLSS и FSR своя — при смене технологии выбор сбрасывается, если он недоступен.
					if (!UDarcGameUserSettings::GetFrameGenerationOptions(Upscaler).Contains(FrameGeneration))
					{
						FrameGeneration = 0;
					}
				});
			AddChoice(Rows, T(TEXT("Settings_UpscalerMode")),
				[this]() { return UpscalerModeText(); },
				[this](int32 Dir) { UpscalerQuality = FMath::Clamp(UpscalerQuality + Dir, 0, 4); },
				[this]() { return Upscaler != 0; });
			AddChoice(Rows, T(TEXT("Settings_FrameGen")),
				[this]() { return UDarcGameUserSettings::GetFrameGenerationName(FrameGeneration); },
				[this](int32 Dir) { const TArray<int32> Options = UDarcGameUserSettings::GetFrameGenerationOptions(Upscaler); Step(Options, FrameGeneration, Dir); },
				[this]() { return UDarcGameUserSettings::GetFrameGenerationOptions(Upscaler).Num() > 1; });
			if (UDarcGameUserSettings::IsReflexAvailable())
			{
				AddChoice(Rows, FText::FromString(TEXT("NVIDIA Reflex")),
					[this]() { return T(ReflexMode == 3 ? TEXT("Settings_ReflexBoost") : ReflexMode ? TEXT("Settings_On") : TEXT("Settings_Off")); },
					[this](int32 Dir) { Step(ReflexModes, ReflexMode, Dir); });
			}
			AddSlider(Rows, T(TEXT("Settings_ResolutionScale")), &ResolutionScale, MinResolutionScale, 100.f,
				[this]() { return FText::AsPercent(ResolutionScale / 100.f); },
				[this]() { return Upscaler == 0; });
			AddChoice(Rows, T(TEXT("Settings_AAMethod")),
				[this]() { return AAMethodText(); },
				[this](int32 Dir) { Step(AAMethods, AAMethod, Dir); },
				[this]() { return Upscaler == 0; });
			AddSlider(Rows, T(TEXT("Settings_Brightness")), &Brightness,
				UDarcGameUserSettings::MinBrightness, UDarcGameUserSettings::MaxBrightness,
				[this]() { return FText::AsPercent((Brightness - UDarcGameUserSettings::MinBrightness) / (UDarcGameUserSettings::MaxBrightness - UDarcGameUserSettings::MinBrightness)); });

			AddHeader(Rows, T(TEXT("Settings_GraphicsAdvanced")));
			static const TCHAR* QualityKeys[QCount] = {
				TEXT("Settings_QShadows"), TEXT("Settings_QViewDistance"), TEXT("Settings_QTextures"), TEXT("Settings_QEffects"),
				TEXT("Settings_QPostProcess"), TEXT("Settings_QFoliage"), TEXT("Settings_QShading"), TEXT("Settings_QGI"),
				TEXT("Settings_QReflections"), TEXT("Settings_QAntiAliasing") };
			for (int32 Row = 0; Row < QCount; ++Row)
			{
				AddChoice(Rows, T(QualityKeys[Row]),
					[this, Row]() { return LevelText(Levels[Row]); },
					[this, Row](int32 Dir) { Levels[Row] = FMath::Clamp(Levels[Row] + Dir, 0, 4); });
			}

			AddHeader(Rows, T(TEXT("Settings_Controls")));
			AddSlider(Rows, T(TEXT("Settings_Sensitivity")), &Sensitivity,
				UDarcGameUserSettings::MinSensitivity, UDarcGameUserSettings::MaxSensitivity,
				[this]() { FNumberFormattingOptions O; O.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1); return FText::AsNumber(Sensitivity, &O); });
			AddCheck(Rows, T(TEXT("Settings_InvertY")), &bInvertY);
			AddSlider(Rows, T(TEXT("Settings_Fov")), &Fov,
				UDarcGameUserSettings::MinFov, UDarcGameUserSettings::MaxFov,
				[this]() { return FText::AsNumber(FMath::RoundToInt(Fov)); });

			AddHeader(Rows, T(TEXT("Settings_Keys")));
			const TArray<FDarcKeyAction>& Actions = UDarcGameUserSettings::GetKeyActions();
			for (int32 Index = 0; Index < Actions.Num(); ++Index)
			{
				AddKeyRow(Rows, Index);
			}

			AddHeader(Rows, T(TEXT("Settings_Audio")));
			AddSlider(Rows, T(TEXT("Settings_Volume")), &Volume, 0.f, 1.f, [this]() { return FText::AsPercent(Volume); });
			static const TCHAR* VolumeKeys[] = { TEXT("Settings_VolMusic"), TEXT("Settings_VolEffects"), TEXT("Settings_VolVoice"), TEXT("Settings_VolRadio") };
			for (int32 Index = 0; Index < 4; ++Index)
			{
				AddSlider(Rows, T(VolumeKeys[Index]), &Volumes[Index], 0.f, 1.f, [this, Index]() { return FText::AsPercent(Volumes[Index]); });
			}

			AddHeader(Rows, T(TEXT("Settings_VoiceChat")));
			AddChoice(Rows, T(TEXT("Settings_VoiceMode")),
				[this]() { return T(bOpenMic ? TEXT("Settings_VoiceOpen") : TEXT("Settings_VoicePTT")); },
				[this](int32) { bOpenMic = !bOpenMic; });
			AddSlider(Rows, T(TEXT("Settings_MicGain")), &MicGain,
				UDarcGameUserSettings::MinMicGain, UDarcGameUserSettings::MaxMicGain,
				[this]() { return FText::AsPercent(MicGain); });

			ChildSlot
			[
				MakeFrame(900.f,
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
					[
						SNew(STextBlock).Text(T(TEXT("Settings_Title"))).Font(Font(30, TEXT("Bold"))).ColorAndOpacity(TextColor)
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SScrollBox) + SScrollBox::Slot().Padding(0.f, 0.f, 16.f, 0.f)[Rows]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							MakeButton(T(TEXT("Settings_Defaults")), [this]() { ResetToDefaults(); })
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)
						[
							MakeButton(T(TEXT("Settings_Back")), [this]() { Close(); })
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							MakeButton(T(TEXT("Settings_Apply")), [this]() { Apply(); })
						]
					])
			];
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		// Назначение клавиши: ловим нажатие раньше кнопок окна.
		virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override
		{
			if (CapturingAction != INDEX_NONE)
			{
				if (KeyEvent.GetKey() != EKeys::Escape)
				{
					AssignKey(KeyEvent.GetKey());
				}
				CapturingAction = INDEX_NONE;
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		virtual FReply OnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override
		{
			if (CapturingAction != INDEX_NONE)
			{
				AssignKey(MouseEvent.GetEffectingButton());
				CapturingAction = INDEX_NONE;
				bSwallowMouseUp = true;
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override
		{
			bSwallowMouseUp = false;
			return FReply::Handled();
		}

		virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override
		{
			if (KeyEvent.GetKey() == EKeys::Escape)
			{
				Close();
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

	private:
		FSimpleDelegate OnClose;
		FSimpleDelegate OnLanguageChanged;

		// Значения в окне (до «Применить»).
		FString Language = TEXT("ru");
		int32 WindowMode = 0; // 0 полный экран, 1 окно без рамки, 2 окно
		FIntPoint Resolution = FIntPoint(1920, 1080);
		int32 Levels[QCount] = { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };
		bool bVSync = false;
		float FpsLimit = 0.f;
		float ResolutionScale = 100.f;
		int32 AAMethod = -1;
		float Brightness = 2.2f;
		float Sensitivity = 1.f;
		bool bInvertY = false;
		float Fov = 90.f;
		float Volume = 1.f;
		float Volumes[4] = { 1.f, 1.f, 1.f, 1.f };
		bool bOpenMic = false;
		float MicGain = 1.f;
		TMap<FName, FKey> Keys;

		int32 CapturingAction = INDEX_NONE;
		bool bSwallowMouseUp = false;

		TArray<FIntPoint> Resolutions;
		TArray<float> FpsOptions = { 30.f, 60.f, 120.f, 144.f, 165.f, 240.f, 0.f };
		TArray<int32> AAMethods = { -1, 0, 1, 2, 4 };
		int32 Upscaler = 0;
		int32 UpscalerQuality = 1;
		int32 FrameGeneration = 0;
		int32 ReflexMode = 1;
		TArray<int32> Upscalers;
		TArray<int32> ReflexModes = { 0, 1, 3 };
		static constexpr float MinResolutionScale = 50.f;

		static int32 ModeToIndex(EWindowMode::Type Mode)
		{
			return Mode == EWindowMode::Fullscreen ? 0 : Mode == EWindowMode::WindowedFullscreen ? 1 : 2;
		}

		static EWindowMode::Type IndexToMode(int32 Index)
		{
			return Index == 0 ? EWindowMode::Fullscreen : Index == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;
		}

		template <typename TValue>
		static void Step(const TArray<TValue>& Options, TValue& Value, int32 Dir)
		{
			if (Options.Num() == 0)
			{
				return;
			}
			const int32 Index = Options.IndexOfByKey(Value);
			Value = Options[Index == INDEX_NONE ? 0 : (Index + Dir + Options.Num()) % Options.Num()];
		}

		int32 GetPreset() const
		{
			for (int32 Row = 1; Row < QCount; ++Row)
			{
				if (Levels[Row] != Levels[0])
				{
					return -1;
				}
			}
			return Levels[0];
		}

		void SetPreset(int32 Level)
		{
			for (int32& L : Levels)
			{
				L = Level;
			}
		}

		FText WindowModeText() const
		{
			static const TCHAR* ModeKeys[] = { TEXT("Settings_Fullscreen"), TEXT("Settings_Borderless"), TEXT("Settings_Windowed") };
			return T(ModeKeys[WindowMode]);
		}

		static FText LevelText(int32 Level)
		{
			static const TCHAR* LevelKeys[] = { TEXT("Settings_QLow"), TEXT("Settings_QMedium"), TEXT("Settings_QHigh"), TEXT("Settings_QEpic"), TEXT("Settings_QCinematic") };
			return T(LevelKeys[FMath::Clamp(Level, 0, 4)]);
		}

		static FText UpscalerText(int32 Value)
		{
			switch (Value)
			{
			case 1: return FText::FromString(TEXT("NVIDIA DLSS"));
			case 2: return FText::FromString(TEXT("AMD FSR"));
			default: return FText::FromString(TEXT("TSR"));
			}
		}

		FText UpscalerModeText() const
		{
			if (UpscalerQuality == 0)
			{
				return FText::FromString(Upscaler == 1 ? TEXT("DLAA") : TEXT("Native AA"));
			}
			static const TCHAR* ModeKeys[] = { nullptr, TEXT("Settings_UpQuality"), TEXT("Settings_UpBalanced"), TEXT("Settings_UpPerformance"), TEXT("Settings_UpUltraPerformance") };
			return T(ModeKeys[FMath::Clamp(UpscalerQuality, 1, 4)]);
		}

		FText AAMethodText() const
		{
			switch (AAMethod)
			{
			case 0: return T(TEXT("Settings_AAOff"));
			case 1: return FText::FromString(TEXT("FXAA"));
			case 2: return FText::FromString(TEXT("TAA"));
			case 4: return FText::FromString(TEXT("TSR"));
			default: return T(TEXT("Settings_AADefault"));
			}
		}

		FKey KeyFor(const FDarcKeyAction& Action) const
		{
			const FKey* Custom = Keys.Find(Action.Id);
			return Custom ? *Custom : Action.DefaultKey;
		}

		void AssignKey(const FKey& NewKey)
		{
			const TArray<FDarcKeyAction>& Actions = UDarcGameUserSettings::GetKeyActions();
			if (!Actions.IsValidIndex(CapturingAction) || !NewKey.IsValid() || NewKey.IsAxis1D() || NewKey.IsAxis2D())
			{
				return;
			}
			const FDarcKeyAction& Target = Actions[CapturingAction];
			const FKey OldKey = KeyFor(Target);
			// Клавиша уже занята другим действием — меняем их местами.
			for (const FDarcKeyAction& Other : Actions)
			{
				if (Other.Id != Target.Id && KeyFor(Other) == NewKey)
				{
					Keys.Add(Other.Id, OldKey);
				}
			}
			Keys.Add(Target.Id, NewKey);
		}

		void LoadFrom(const UDarcGameUserSettings* S)
		{
			if (!S)
			{
				return;
			}
			Language = S->GetEffectiveLanguage();
			WindowMode = ModeToIndex(S->GetFullscreenMode());
			Resolution = S->GetScreenResolution();
			Levels[QShadows] = S->GetShadowQuality();
			Levels[QViewDistance] = S->GetViewDistanceQuality();
			Levels[QTextures] = S->GetTextureQuality();
			Levels[QEffects] = S->GetVisualEffectQuality();
			Levels[QPostProcess] = S->GetPostProcessingQuality();
			Levels[QFoliage] = S->GetFoliageQuality();
			Levels[QShading] = S->GetShadingQuality();
			Levels[QGlobalIllumination] = S->GetGlobalIlluminationQuality();
			Levels[QReflections] = S->GetReflectionQuality();
			Levels[QAntiAliasing] = S->GetAntiAliasingQuality();
			for (int32& L : Levels)
			{
				L = FMath::Clamp(L, 0, 4);
			}
			bVSync = S->IsVSyncEnabled();
			FpsLimit = S->GetFrameRateLimit();
			if (!FpsOptions.Contains(FpsLimit))
			{
				FpsOptions.Insert(FpsLimit, 0);
			}
			float ScaleNormalized = 1.f, ScaleValue = 100.f, MinScale = 0.f, MaxScale = 100.f;
			S->GetResolutionScaleInformationEx(ScaleNormalized, ScaleValue, MinScale, MaxScale);
			// 0 — «как задано проектом» (обычно 100 %), не путать с минимальным масштабом.
			ResolutionScale = ScaleValue > 0.f ? FMath::Clamp(ScaleValue, MinResolutionScale, 100.f) : 100.f;
			AAMethod = AAMethods.Contains(S->AntiAliasingMethod) ? S->AntiAliasingMethod : -1;
			Upscalers = { 0 };
			if (UDarcGameUserSettings::IsDLSSAvailable())
			{
				Upscalers.Add(1);
			}
			Upscalers.Add(2);
			Upscaler = Upscalers.Contains(S->Upscaler) ? S->Upscaler : 0;
			UpscalerQuality = FMath::Clamp(S->UpscalerQuality, 0, 4);
			FrameGeneration = UDarcGameUserSettings::GetFrameGenerationOptions(Upscaler).Contains(S->FrameGeneration) ? S->FrameGeneration : 0;
			ReflexMode = ReflexModes.Contains(S->ReflexMode) ? S->ReflexMode : 1;
			Brightness = S->Brightness;
			Sensitivity = S->MouseSensitivity;
			bInvertY = S->bInvertMouseY;
			Fov = S->FieldOfView;
			Volume = S->MasterVolume;
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Volumes[Index] = S->GetVolume(static_cast<EDarcVolume>(Index));
			}
			bOpenMic = S->bOpenMic;
			MicGain = S->MicGain;
			Keys = S->KeyBindings;
		}

		void ResetToDefaults()
		{
			const UDarcGameUserSettings* D = GetDefault<UDarcGameUserSettings>();
			WindowMode = 0;
			Resolution = D->GetDesktopResolution(); // разрешение рабочего стола
			Resolutions.AddUnique(Resolution);
			SetPreset(3);
			bVSync = false;
			FpsLimit = 0.f;
			ResolutionScale = 100.f;
			AAMethod = -1;
			Upscaler = D->Upscaler;
			UpscalerQuality = D->UpscalerQuality;
			FrameGeneration = D->FrameGeneration;
			ReflexMode = D->ReflexMode;
			Brightness = D->Brightness;
			Sensitivity = D->MouseSensitivity;
			bInvertY = D->bInvertMouseY;
			Fov = D->FieldOfView;
			Volume = D->MasterVolume;
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Volumes[Index] = D->GetVolume(static_cast<EDarcVolume>(Index));
			}
			bOpenMic = D->bOpenMic;
			MicGain = D->MicGain;
			Keys.Reset();
		}

		void Apply()
		{
			UDarcGameUserSettings* S = UDarcGameUserSettings::Get();
			if (!S)
			{
				return;
			}
			const bool bLanguageChanged = S->GetEffectiveLanguage() != Language;
			S->Language = Language;
			S->SetFullscreenMode(IndexToMode(WindowMode));
			S->SetScreenResolution(Resolution);
			S->SetShadowQuality(Levels[QShadows]);
			S->SetViewDistanceQuality(Levels[QViewDistance]);
			S->SetTextureQuality(Levels[QTextures]);
			S->SetVisualEffectQuality(Levels[QEffects]);
			S->SetPostProcessingQuality(Levels[QPostProcess]);
			S->SetFoliageQuality(Levels[QFoliage]);
			S->SetShadingQuality(Levels[QShading]);
			S->SetGlobalIlluminationQuality(Levels[QGlobalIllumination]);
			S->SetReflectionQuality(Levels[QReflections]);
			S->SetAntiAliasingQuality(Levels[QAntiAliasing]);
			S->SetVSyncEnabled(bVSync);
			S->SetFrameRateLimit(FpsLimit);
			S->SetResolutionScaleValueEx(ResolutionScale);
			S->AntiAliasingMethod = AAMethod;
			S->Upscaler = Upscaler;
			S->UpscalerQuality = UpscalerQuality;
			S->FrameGeneration = FrameGeneration;
			S->ReflexMode = ReflexMode;
			S->Brightness = Brightness;
			S->MouseSensitivity = Sensitivity;
			S->bInvertMouseY = bInvertY;
			S->FieldOfView = Fov;
			S->MasterVolume = Volume;
			for (int32 Index = 0; Index < 4; ++Index)
			{
				S->SetVolume(static_cast<EDarcVolume>(Index), Volumes[Index]);
			}
			S->bOpenMic = bOpenMic;
			S->MicGain = MicGain;
			// Храним только отличающиеся от умолчания клавиши.
			S->KeyBindings.Reset();
			for (const FDarcKeyAction& Action : UDarcGameUserSettings::GetKeyActions())
			{
				if (KeyFor(Action) != Action.DefaultKey)
				{
					S->KeyBindings.Add(Action.Id, KeyFor(Action));
				}
			}
			S->ApplySettings(false); // применяет и сохраняет в GameUserSettings.ini

			if (bLanguageChanged)
			{
				OnLanguageChanged.ExecuteIfBound(); // окно пересоберётся на новом языке
			}
		}

		void Close()
		{
			OnClose.ExecuteIfBound();
		}

		// --- строки окна ---

		static TSharedRef<SWidget> Label(const FText& Text)
		{
			return SNew(SBox).WidthOverride(360.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Text).Font(Font(16)).ColorAndOpacity(TextColor)
				];
		}

		void AddHeader(const TSharedRef<SVerticalBox>& Rows, const FText& Text)
		{
			Rows->AddSlot().AutoHeight().Padding(0.f, 20.f, 0.f, 8.f)
				[
					SNew(STextBlock).Text(Text).Font(Font(18, TEXT("Bold"))).ColorAndOpacity(AccentColor)
				];
		}

		void AddChoice(const TSharedRef<SVerticalBox>& Rows, const FText& Name, TFunction<FText()> GetText, TFunction<void(int32)> OnStep,
			TFunction<bool()> IsEnabled = nullptr)
		{
			auto Arrow = [OnStep](const TCHAR* Glyph, int32 Dir) -> TSharedRef<SWidget>
			{
				return SNew(SButton)
					.ButtonStyle(FCoreStyle::Get(), "NoBorder")
					.ContentPadding(FMargin(10.f, 2.f))
					.OnClicked_Lambda([OnStep, Dir]() { OnStep(Dir); return FReply::Handled(); })
					[
						SNew(STextBlock).Text(FText::FromString(Glyph)).Font(Font(18, TEXT("Bold"))).ColorAndOpacity(AccentColor)
					];
			};
			Rows->AddSlot().AutoHeight().Padding(0.f, 4.f)
				[
					SNew(SHorizontalBox)
					.IsEnabled_Lambda([IsEnabled]() { return !IsEnabled || IsEnabled(); })
					+ SHorizontalBox::Slot().AutoWidth()[Label(Name)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Arrow(TEXT("<"), -1)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Center)
					[
						SNew(STextBlock).Text_Lambda([GetText]() { return GetText(); }).Font(Font(16)).ColorAndOpacity(TextColor)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Arrow(TEXT(">"), 1)]
				];
		}

		void AddCheck(const TSharedRef<SVerticalBox>& Rows, const FText& Name, bool* Value)
		{
			Rows->AddSlot().AutoHeight().Padding(0.f, 4.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Label(Name)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([Value]() { return *Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([Value](ECheckBoxState State) { *Value = State == ECheckBoxState::Checked; })
					]
				];
		}

		void AddSlider(const TSharedRef<SVerticalBox>& Rows, const FText& Name, float* Value, float Min, float Max, TFunction<FText()> GetText,
			TFunction<bool()> IsEnabled = nullptr)
		{
			Rows->AddSlot().AutoHeight().Padding(0.f, 4.f)
				[
					SNew(SHorizontalBox)
					.IsEnabled_Lambda([IsEnabled]() { return !IsEnabled || IsEnabled(); })
					+ SHorizontalBox::Slot().AutoWidth()[Label(Name)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 0.f)
					[
						SNew(SSlider)
						.MinValue(Min)
						.MaxValue(Max)
						.Value_Lambda([Value]() { return *Value; })
						.OnValueChanged_Lambda([Value](float NewValue) { *Value = NewValue; })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(70.f).HAlign(HAlign_Right)
						[
							SNew(STextBlock).Text_Lambda([GetText]() { return GetText(); }).Font(Font(16)).ColorAndOpacity(DimColor)
						]
					]
				];
		}

		void AddKeyRow(const TSharedRef<SVerticalBox>& Rows, int32 Index)
		{
			const FDarcKeyAction& Action = UDarcGameUserSettings::GetKeyActions()[Index];
			Rows->AddSlot().AutoHeight().Padding(0.f, 3.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Label(T(Action.LabelKey))]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Center)
					[
						SNew(SBox).MinDesiredWidth(220.f)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FMargin(12.f, 4.f))
							.OnClicked_Lambda([this, Index]() { CapturingAction = Index; return FReply::Handled(); })
							[
								SNew(STextBlock)
								.Font(Font(15, TEXT("Bold")))
								.ColorAndOpacity_Lambda([this, Index]() { return FSlateColor(CapturingAction == Index ? AccentColor : TextColor); })
								.Text_Lambda([this, Index]()
								{
									const FDarcKeyAction& A = UDarcGameUserSettings::GetKeyActions()[Index];
									return CapturingAction == Index ? T(TEXT("Settings_PressKey")) : KeyFor(A).GetDisplayName();
								})
							]
						]
					]
				];
		}
	};

	// --- меню паузы ---

	class SDarcPauseMenu : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDarcPauseMenu) {}
			SLATE_EVENT(FSimpleDelegate, OnResume)
			SLATE_EVENT(FSimpleDelegate, OnSettings)
			SLATE_EVENT(FSimpleDelegate, OnMainMenu)
			SLATE_EVENT(FSimpleDelegate, OnQuit)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			OnResume = InArgs._OnResume;
			auto Button = [](const FText& Text, FSimpleDelegate Action)
			{
				return MakeButton(Text, [Action]() { Action.ExecuteIfBound(); }, 320.f);
			};
			ChildSlot
			[
				MakeFrame(420.f,
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 22.f)
					[
						SNew(STextBlock).Text(T(TEXT("Pause_Title"))).Font(Font(30, TEXT("Bold"))).ColorAndOpacity(TextColor)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 5.f)[Button(T(TEXT("Pause_Resume")), InArgs._OnResume)]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 5.f)[Button(T(TEXT("Pause_Settings")), InArgs._OnSettings)]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 5.f)[Button(T(TEXT("Pause_MainMenu")), InArgs._OnMainMenu)]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 5.f)[Button(T(TEXT("Pause_Quit")), InArgs._OnQuit)])
			];
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override
		{
			if (KeyEvent.GetKey() == EKeys::Escape)
			{
				OnResume.ExecuteIfBound();
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

	private:
		FSimpleDelegate OnResume;
	};

	struct FOpenWidget
	{
		TWeakPtr<SWidget> Widget;
		TWeakObjectPtr<UGameViewportClient> Viewport;

		bool IsOpen() const { return Widget.IsValid(); }

		void Show(UGameViewportClient* InViewport, const TSharedRef<SWidget>& InWidget, int32 ZOrder)
		{
			InViewport->AddViewportWidgetContent(InWidget, ZOrder);
			Widget = InWidget;
			Viewport = InViewport;
			FSlateApplication::Get().SetKeyboardFocus(InWidget, EFocusCause::SetDirectly);
		}

		void Hide()
		{
			if (UGameViewportClient* VP = Viewport.Get())
			{
				if (TSharedPtr<SWidget> Pinned = Widget.Pin())
				{
					VP->RemoveViewportWidgetContent(Pinned.ToSharedRef());
				}
			}
			Widget.Reset();
			Viewport.Reset();
		}

		void Focus() const
		{
			if (TSharedPtr<SWidget> Pinned = Widget.Pin())
			{
				FSlateApplication::Get().SetKeyboardFocus(Pinned, EFocusCause::SetDirectly);
			}
		}
	};

	FOpenWidget SettingsMenu;
	FOpenWidget PauseMenu;
}

void UDarcSettingsLibrary::OpenSettingsMenu(const UObject* WorldContextObject)
{
	OpenSettingsMenuWithCallback(WorldContextObject, FSimpleDelegate());
}

void UDarcSettingsLibrary::OpenSettingsMenuWithCallback(const UObject* WorldContextObject, FSimpleDelegate OnClosed)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport || SettingsMenu.IsOpen())
	{
		return;
	}
	TWeakObjectPtr<const UObject> Context = WorldContextObject;
	SettingsMenu.Show(Viewport, SNew(SDarcSettingsMenu)
		.OnClose_Lambda([OnClosed]()
		{
			SettingsMenu.Hide();
			OnClosed.ExecuteIfBound();
		})
		.OnLanguageChanged_Lambda([Context, OnClosed]()
		{
			// Пересобрать окно на новом языке (тексты окна взяты из таблицы при создании).
			SettingsMenu.Hide();
			if (Context.IsValid())
			{
				OpenSettingsMenuWithCallback(Context.Get(), OnClosed);
			}
		}), 1100);
}

bool UDarcSettingsLibrary::IsSettingsMenuOpen()
{
	return SettingsMenu.IsOpen();
}

void UDarcSettingsLibrary::OpenPauseMenu(APlayerController* Owner)
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport || PauseMenu.IsOpen())
	{
		return;
	}
	TWeakObjectPtr<APlayerController> WeakOwner = Owner;
	auto Resume = [WeakOwner]()
	{
		if (SettingsMenu.IsOpen())
		{
			return;
		}
		PauseMenu.Hide();
		if (ADarcPlayerController* PC = Cast<ADarcPlayerController>(WeakOwner.Get()))
		{
			PC->SetPauseMenuOpen(false);
		}
	};
	PauseMenu.Show(Viewport, SNew(SDarcPauseMenu)
		.OnResume_Lambda(Resume)
		.OnSettings_Lambda([WeakOwner]()
		{
			OpenSettingsMenuWithCallback(WeakOwner.Get(), FSimpleDelegate::CreateLambda([]() { PauseMenu.Focus(); }));
		})
		.OnMainMenu_Lambda([WeakOwner]()
		{
			PauseMenu.Hide();
			if (APlayerController* PC = WeakOwner.Get())
			{
				PC->SetPause(false);
				// Главное меню — карта по умолчанию (Project Settings → Maps & Modes).
				UGameplayStatics::OpenLevel(PC, FName(*UGameMapsSettings::GetGameDefaultMap()));
			}
		})
		.OnQuit_Lambda([WeakOwner]()
		{
			UKismetSystemLibrary::QuitGame(WeakOwner.Get(), WeakOwner.Get(), EQuitPreference::Quit, false);
		}), 1000);
}

bool UDarcSettingsLibrary::IsPauseMenuOpen()
{
	return PauseMenu.IsOpen();
}
