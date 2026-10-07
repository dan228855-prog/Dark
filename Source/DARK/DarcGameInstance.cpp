// DarcGameInstance.cpp
#include "DarcGameInstance.h"
#include "DarcGameplayLibrary.h"
#include "DarcGameUserSettings.h"
#include "MoviePlayer.h"
#include "Engine/Texture2D.h"
#include "Misc/Paths.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Fonts/CompositeFont.h"

namespace
{
	// Моноширинный шрифт движка (есть кириллица), лежит в Engine/Content/Slate и попадает в сборку.
	FSlateFontInfo MonoFont(float Size)
	{
		static const TSharedPtr<const FCompositeFont> Mono = MakeShared<FStandaloneCompositeFont>(TEXT("DarcMono"),
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansMono.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		return FSlateFontInfo(Mono, Size);
	}

	// Стилизованная надпись поверх картинки.
	TSharedRef<SWidget> MakeOverlayText(const FDarcLoadingOverlay& Overlay)
	{
		const FText Text = UDarcGameplayLibrary::UIText(Overlay.Text);
		TSharedPtr<SWidget> Widget;
		switch (Overlay.Style)
		{
		case EDarcOverlayStyle::Terminal:
		{
			// Экранный текст: голубой, с красно-бирюзовым «двоением» и мигающим курсором.
			const FSlateFontInfo Font = MonoFont(Overlay.Size);
			const float Shift = FMath::Max(1.f, Overlay.Size * 0.08f);
			auto WithCursor = [Text]()
			{
				const bool bCursor = FMath::Fmod(FPlatformTime::Seconds(), 1.0) < 0.55;
				return FText::FromString(Text.ToString() + (bCursor ? TEXT(" █") : TEXT("  ")));
			};
			auto Layer = [&](const FLinearColor& Color, float Dx) -> TSharedRef<SWidget>
			{
				return SNew(SBox).Padding(FMargin(FMath::Max(0.f, Dx), 0.f, FMath::Max(0.f, -Dx), 0.f))
					[
						SNew(STextBlock).Text_Lambda(WithCursor).Font(Font).ColorAndOpacity(Color)
						.AutoWrapText(true).LineHeightPercentage(1.35f)
					];
			};
			Widget = SNew(SOverlay)
				+ SOverlay::Slot()[Layer(FLinearColor(1.f, 0.15f, 0.2f, 0.28f), Shift)]
				+ SOverlay::Slot()[Layer(FLinearColor(0.1f, 0.9f, 1.f, 0.22f), -Shift)]
				+ SOverlay::Slot()[Layer(FLinearColor(0.62f, 0.74f, 1.f, 0.9f), 0.f)];
			break;
		}
		case EDarcOverlayStyle::Poster:
		{
			FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", Overlay.Size);
			Font.LetterSpacing = 120;
			Widget = SNew(STextBlock).Text(Text).Font(Font)
				.ColorAndOpacity(FLinearColor(0.72f, 0.76f, 0.82f, 0.78f))
				.Justification(ETextJustify::Center).AutoWrapText(true).LineHeightPercentage(1.2f);
			break;
		}
		case EDarcOverlayStyle::Title:
		default:
		{
			FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", Overlay.Size);
			Font.LetterSpacing = 160;
			Widget = SNew(STextBlock).Text(Text).Font(Font).ColorAndOpacity(FLinearColor(0.85f, 0.88f, 0.92f, 0.95f));
			break;
		}
		}
		return SNew(SBox).WidthOverride(Overlay.Width)
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(Overlay.Angle))))
			[
				Widget.ToSharedRef()
			];
	}

	// Загрузочный экран: картинка на весь экран (обрезается по краям, без полос) вместе с надписями
	// (одним блоком — надписи остаются на своих местах при любом разрешении) + «Загрузка…» внизу справа.
	class SDarcLoadingScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDarcLoadingScreen) {}
			SLATE_ARGUMENT(UTexture2D*, Image)
			SLATE_ARGUMENT(const TArray<FDarcLoadingOverlay>*, Overlays)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			UTexture2D* Texture = InArgs._Image;
			const FVector2D ImageSize = Texture ? FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) : FVector2D(1920.f, 1080.f);
			if (Texture)
			{
				ImageBrush.SetResourceObject(Texture);
				ImageBrush.ImageSize = ImageSize;
				ImageBrush.DrawAs = ESlateBrushDrawType::Image;
			}

			TSharedRef<SCanvas> Canvas = SNew(SCanvas);
			if (Texture && InArgs._Overlays)
			{
				for (const FDarcLoadingOverlay& Overlay : *InArgs._Overlays)
				{
					Canvas->AddSlot()
						.Position(FVector2D(Overlay.X, Overlay.Y))
						.Size(FVector2D(Overlay.Width, ImageSize.Y - Overlay.Y))
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Top)
						[
							MakeOverlayText(Overlay)
						];
				}
			}

			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor::Black)
				.Padding(0.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SScaleBox)
						.Stretch(EStretch::ScaleToFill)
						.Visibility(Texture ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
						[
							SNew(SBox).WidthOverride(ImageSize.X).HeightOverride(ImageSize.Y)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()[SNew(SImage).Image(&ImageBrush)]
								+ SOverlay::Slot()[Canvas]
							]
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(48.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f, 0.f)
						[
							SNew(STextBlock)
							.Text(UDarcGameplayLibrary::UIText(TEXT("Loading_Text")))
							.Font(MonoFont(20))
							.ColorAndOpacity(FLinearColor(0.82f, 0.85f, 0.9f))
							.ShadowOffset(FVector2D(1.f, 1.f))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SThrobber)
						]
					]
				]
			];
		}

	private:
		FSlateBrush ImageBrush;
	};
}

void UDarcGameInstance::Init()
{
	Super::Init();

	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UDarcGameInstance::BeginLoadingScreen);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UDarcGameInstance::OnMapLoaded);

	// Громкость/яркость/язык из настроек игрока — с самого старта (разрешение движок ставит сам).
	if (UDarcGameUserSettings* Settings = UDarcGameUserSettings::Get())
	{
		Settings->ApplyDarcSettings();
	}
}

void UDarcGameInstance::Shutdown()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	CurrentImage.Reset();

	Super::Shutdown();
}

void UDarcGameInstance::OnMapLoaded(UWorld* LoadedWorld)
{
	// Микс громкости по категориям — заново на каждой карте.
	if (const UDarcGameUserSettings* Settings = UDarcGameUserSettings::Get())
	{
		Settings->ApplyVolumes(LoadedWorld);
	}
}

void UDarcGameInstance::BeginLoadingScreen(const FString& MapName)
{
	// Выделенный сервер экрана не имеет; в PIE MoviePlayer не работает.
	if (IsRunningDedicatedServer() || !IsMoviePlayerEnabled())
	{
		return;
	}

	CurrentImage.Reset();
	const FDarcLoadingScreen* Screen = LoadingScreens.Num() > 0 ? &LoadingScreens[FMath::RandRange(0, LoadingScreens.Num() - 1)] : nullptr;
	if (Screen)
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(Screen->Image.TryLoad()))
		{
			CurrentImage.Reset(Texture);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Загрузочный экран: не найдена текстура %s"), *Screen->Image.ToString());
		}
	}

	FLoadingScreenAttributes Attributes;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bMoviesAreSkippable = false;
	Attributes.MinimumLoadingScreenDisplayTime = MinLoadingScreenTime;
	Attributes.WidgetLoadingScreen = SNew(SDarcLoadingScreen)
		.Image(CurrentImage.Get())
		.Overlays(Screen ? &Screen->Overlays : nullptr);
	GetMoviePlayer()->SetupLoadingScreen(Attributes);
}
