// DarcSettingsMenu.h
// Окна интерфейса на Slate (поверх всего экрана):
//  - настройки: язык, графика (основное и подробно), управление и клавиши, звук, голосовая связь.
//    Изменения копятся в окне и вступают в силу по «Применить»; «Назад»/Esc — закрыть без сохранения.
//    Значения — UDarcGameUserSettings.
//  - меню паузы (Esc в игре): продолжить, настройки, в главное меню, выйти из игры.
// Открываются из Blueprint (кнопка «Настройки» в MainMenu) и из ADarcPlayerController (Esc).
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DarcSettingsMenu.generated.h"

class APlayerController;

UCLASS()
class DARK_API UDarcSettingsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Показать окно настроек поверх экрана (второй раз не откроется). */
	UFUNCTION(BlueprintCallable, Category = "DARC|Settings", meta = (WorldContext = "WorldContextObject"))
	static void OpenSettingsMenu(const UObject* WorldContextObject);

	/** То же, с действием по закрытию окна (меню паузы возвращает себе фокус). */
	static void OpenSettingsMenuWithCallback(const UObject* WorldContextObject, FSimpleDelegate OnClosed);

	/** Открыто ли окно настроек (например, чтобы пауза не закрылась по Esc вместе с ним). */
	UFUNCTION(BlueprintPure, Category = "DARC|Settings")
	static bool IsSettingsMenuOpen();

	/** Меню паузы для этого игрока (вызывает ADarcPlayerController::SetPauseMenuOpen). */
	static void OpenPauseMenu(APlayerController* Owner);

	static bool IsPauseMenuOpen();
};
