// DarcGameMode.h
// Минимальный Game Mode для вертикального среза. Логика набора команды здесь
// со временем обрастет: старт задания, условия завершения выезда и т.п.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DarcGameMode.generated.h"

UCLASS()
class DARK_API ADarcGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ADarcGameMode();

    virtual void PostLogin(APlayerController* NewPlayer) override;
};
