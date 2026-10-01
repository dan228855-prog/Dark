// DarcGameMode.cpp
#include "DarcGameMode.h"
#include "DarcGameState.h"
#include "DarcPlayerState.h"

ADarcGameMode::ADarcGameMode()
{
    GameStateClass = ADarcGameState::StaticClass();
    PlayerStateClass = ADarcPlayerState::StaticClass();
}

void ADarcGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    // Здесь со временем: уведомить GameState о новом игроке, синхронизировать
    // текущее состояние задания для присоединившегося (в т.ч. для join-in-progress).
}
