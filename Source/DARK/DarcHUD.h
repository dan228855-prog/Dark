// DarcHUD.h
// Простой HUD на Canvas — без UMG-ассетов, чтобы срез работал сразу после сборки:
// точка прицела, подсказка взаимодействия, субтитры, список задач (Tab), итог выезда,
// подсказка управления для «духа». Все тексты — из String Table ST_UI.
// Позже можно заменить на UMG, данные те же (TaskManager, GameState::OnSubtitle).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DarcHUD.generated.h"

UCLASS()
class DARK_API ADarcHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	UFUNCTION()
	void HandleSubtitle(const FText& Speaker, const FText& Line, float Duration);

	void BindSubtitlesIfNeeded();
	void DrawTextLine(const FText& Text, float X, float Y, const FLinearColor& Color, float Scale = 1.f, bool bCentered = false);

	struct FSubtitle
	{
		FText Speaker;
		FText Line;
		float EndTime = 0.f;
	};
	TArray<FSubtitle> Subtitles;
	bool bSubtitlesBound = false;
};
