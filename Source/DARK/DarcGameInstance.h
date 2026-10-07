// DarcGameInstance.h
// Экземпляр игры: показывает загрузочный экран (MoviePlayer) на время загрузки любой карты —
// случайная картинка из списка LoadingScreens + «Загрузка…». Поверх картинки — стилизованные
// надписи (плакат, экран монитора, сообщение в небе) в координатах самой картинки, текст — из ST_UI.
// Список — в Config/DefaultGame.ini, секция [/Script/DARK.DarcGameInstance]:
//   +LoadingScreens=(Image=/Game/DARC/UI/LoadingScreens/T_Loading_01.T_Loading_01,Overlays=((Text=Loading_Poster,X=1022,Y=300,Width=100,Size=12,Style=Poster)))
// Пустой список — чёрный экран с надписью. В PIE движок загрузочный экран не показывает,
// проверять — в Standalone Game или в собранной игре.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "DarcGameInstance.generated.h"

class UTexture2D;

UENUM()
enum class EDarcOverlayStyle : uint8
{
	Terminal, // моноширинный текст экрана/системы: голубой, с цветным «двоением» и курсором
	Poster,   // крупный жирный текст по центру (плакат, табличка)
	Title,    // заголовок (название на экране)
};

/** Надпись поверх картинки. Координаты и размер шрифта — в пикселях исходной картинки. */
USTRUCT()
struct FDarcLoadingOverlay
{
	GENERATED_BODY()

	/** Ключ строки в ST_UI. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading")
	FName Text;

	/** Левый верхний угол блока текста. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading")
	float X = 0.f;

	UPROPERTY(Config, EditAnywhere, Category = "Loading")
	float Y = 0.f;

	/** Ширина блока (перенос строк). */
	UPROPERTY(Config, EditAnywhere, Category = "Loading")
	float Width = 300.f;

	UPROPERTY(Config, EditAnywhere, Category = "Loading")
	float Size = 14.f;

	UPROPERTY(Config, EditAnywhere, Category = "Loading")
	EDarcOverlayStyle Style = EDarcOverlayStyle::Terminal;

	/** Поворот, градусы (надпись на стене под углом). */
	UPROPERTY(Config, EditAnywhere, Category = "Loading")
	float Angle = 0.f;
};

USTRUCT()
struct FDarcLoadingScreen
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	FSoftObjectPath Image;

	UPROPERTY(Config, EditAnywhere, Category = "Loading")
	TArray<FDarcLoadingOverlay> Overlays;
};

UCLASS(Config = Game)
class DARK_API UDarcGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	/** Загрузочные экраны. Каждая загрузка — случайный из списка. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading")
	TArray<FDarcLoadingScreen> LoadingScreens;

	/** Минимум секунд на экране, чтобы картинка не мелькала на быстрой загрузке. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading")
	float MinLoadingScreenTime = 2.f;

private:
	void BeginLoadingScreen(const FString& MapName);
	void OnMapLoaded(UWorld* LoadedWorld);

	/** Текущая картинка: держим до следующей загрузки (иначе сборщик мусора выгрузит её с экрана). */
	TStrongObjectPtr<UTexture2D> CurrentImage;
};
