// DarcMenuGameMode.h
// Режим главного меню (карта /Game/LevelPrototyping/Map/Menu): без персонажа, курсор виден,
// ввод только в интерфейс. Сам виджет меню (MainMenu) создаёт Level Blueprint карты.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "DarcMenuGameMode.generated.h"

UCLASS()
class DARK_API ADarcMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADarcMenuPlayerController();

protected:
	virtual void BeginPlay() override;
};

UCLASS()
class DARK_API ADarcMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADarcMenuGameMode();
};
