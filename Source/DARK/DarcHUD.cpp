// DarcHUD.cpp
#include "DarcHUD.h"
#include "CarryableItem.h"
#include "DarcGameState.h"
#include "DarcGameplayLibrary.h"
#include "DarcPlayerController.h"
#include "DarcSpiritCharacter.h"
#include "Interactable.h"
#include "InteractionComponent.h"
#include "DarcGrabComponent.h"
#include "DarcHintComponent.h"
#include "DarcFlashlightComponent.h"
#include "GameFramework/PlayerController.h"
#include "TaskManagerComponent.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

void ADarcHUD::BindSubtitlesIfNeeded()
{
	// У клиента GameState приходит не сразу — подписываемся, как только он появился.
	if (bSubtitlesBound)
	{
		return;
	}
	if (ADarcGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADarcGameState>() : nullptr)
	{
		GS->OnSubtitle.AddDynamic(this, &ADarcHUD::HandleSubtitle);
		bSubtitlesBound = true;
	}
}

void ADarcHUD::HandleSubtitle(const FText& Speaker, const FText& Line, float Duration)
{
	FSubtitle& Sub = Subtitles.AddDefaulted_GetRef();
	Sub.Speaker = Speaker;
	Sub.Line = Line;
	Sub.EndTime = GetWorld()->GetTimeSeconds() + Duration;
	if (Subtitles.Num() > 3)
	{
		Subtitles.RemoveAt(0);
	}
}

void ADarcHUD::DrawTextLine(const FText& Text, float X, float Y, const FLinearColor& Color, float Scale, bool bCentered)
{
	if (Text.IsEmpty() || !Canvas)
	{
		return;
	}
	FCanvasTextItem Item(FVector2D(X, Y), Text, GEngine->GetMediumFont(), Color);
	Item.Scale = FVector2D(Scale);
	Item.bCentreX = bCentered;
	Item.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Item);
}

void ADarcHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	BindSubtitlesIfNeeded();

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float Now = GetWorld()->GetTimeSeconds();
	APawn* Pawn = GetOwningPawn();

	// --- Прицел ---
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.6f), W * 0.5f - 2.f, H * 0.5f - 2.f, 4.f, 4.f);

	// --- Название под прицелом (ВРЕМЕННО, см. DarcHintComponent.h) ---
	if (PlayerOwner && Pawn)
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		PlayerOwner->GetPlayerViewPoint(ViewLocation, ViewRotation);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcHintTrace), false, Pawn);
		if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * 350.f, ECC_Visibility, Params))
		{
			const UDarcHintComponent* Hint = Hit.GetActor() ? Hit.GetActor()->FindComponentByClass<UDarcHintComponent>() : nullptr;
			const FText Label = Hint ? Hint->GetDisplayText() : FText::GetEmpty();
			if (!Label.IsEmpty())
			{
				DrawTextLine(Label, W * 0.5f, H * 0.5f - 40.f, FLinearColor(1.f, 0.92f, 0.7f, 0.95f), 1.f, true);
			}
		}
	}

	// --- Подсказка взаимодействия ---
	if (const UInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UInteractionComponent>() : nullptr)
	{
		if (AActor* Focused = Interaction->FocusedActor)
		{
			const FText Prompt = IInteractable::Execute_GetInteractionPrompt(Focused);
			if (!Prompt.IsEmpty())
			{
				DrawTextLine(FText::Format(UDarcGameplayLibrary::UIText(TEXT("HUD_PromptFormat")), Prompt),
					W * 0.5f, H * 0.5f + 28.f, FLinearColor::White, 1.1f, true);
			}
		}
	}

	// --- Захват (ЛКМ) ---
	if (const UDarcGrabComponent* Grab = Pawn ? Pawn->FindComponentByClass<UDarcGrabComponent>() : nullptr)
	{
		if (Grab->IsHolding())
		{
			DrawTextLine(UDarcGameplayLibrary::UIText(TEXT("HUD_Holding")), W * 0.5f, H * 0.5f + 52.f, FLinearColor(0.85f, 0.9f, 1.f, 0.85f), 0.9f, true);
		}
		else if (Grab->FocusedGrabbable)
		{
			DrawTextLine(UDarcGameplayLibrary::UIText(TEXT("HUD_Grab")), W * 0.5f, H * 0.5f + 52.f, FLinearColor(0.85f, 0.9f, 1.f, 0.85f), 0.9f, true);
		}
	}

	// --- В руках предмет: как положить ---
	if (Pawn && ACarryableItem::FindItemHeldBy(Pawn))
	{
		DrawTextLine(UDarcGameplayLibrary::UIText(TEXT("HUD_DropHint")), W - 160.f, H - 64.f, FLinearColor(0.8f, 0.8f, 0.8f, 0.6f), 0.85f, false);
	}

	// --- Фонарик: подсказка, пока выключен ---
	if (const UDarcFlashlightComponent* Flashlight = Pawn ? Pawn->FindComponentByClass<UDarcFlashlightComponent>() : nullptr)
	{
		if (!Flashlight->IsOn())
		{
			DrawTextLine(UDarcGameplayLibrary::UIText(TEXT("HUD_FlashlightHint")), W - 160.f, H - 40.f, FLinearColor(0.8f, 0.8f, 0.8f, 0.6f), 0.85f, false);
		}
	}

	// --- «Дух» ---
	if (const ADarcSpiritCharacter* Spirit = Cast<ADarcSpiritCharacter>(Pawn))
	{
		DrawTextLine(UDarcGameplayLibrary::UIText(TEXT("HUD_SpiritHint")), W * 0.5f, H - 60.f, FLinearColor(0.7f, 0.8f, 1.f, 0.8f), 0.9f, true);
		const AGameStateBase* GS = GetWorld()->GetGameState();
		const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : Now;
		if (Spirit->CooldownEndTime > ServerNow)
		{
			DrawTextLine(UDarcGameplayLibrary::UIText(TEXT("HUD_SpiritCooldown")), W * 0.5f, H - 36.f, FLinearColor(0.6f, 0.6f, 0.7f, 0.7f), 0.8f, true);
		}
	}

	// --- Задачи (Tab) ---
	const ADarcPlayerController* PC = Cast<ADarcPlayerController>(GetOwningPlayerController());
	if (const UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this); Tasks && (!PC || PC->IsTaskListVisible()))
	{
		float Y = 40.f;
		for (const FDarcTaskState& Task : Tasks->GetVisibleTasks())
		{
			const bool bChild = !Task.Definition.ParentTaskId.IsNone();
			FLinearColor Color = FLinearColor(0.85f, 0.85f, 0.85f);
			FName Mark = TEXT("HUD_TaskActive");
			switch (Task.Status)
			{
			case EDarcTaskStatus::Completed: Color = FLinearColor(0.45f, 0.8f, 0.45f); Mark = TEXT("HUD_TaskDone"); break;
			case EDarcTaskStatus::Failed:    Color = FLinearColor(0.85f, 0.35f, 0.3f); Mark = TEXT("HUD_TaskFailed"); break;
			case EDarcTaskStatus::Locked:    Color = FLinearColor(0.5f, 0.5f, 0.5f); Mark = TEXT("HUD_TaskLocked"); break;
			default: break;
			}
			const FText Line = FText::Format(UDarcGameplayLibrary::UIText(Mark), Task.Definition.Title);
			DrawTextLine(Line, bChild ? 64.f : 40.f, Y, Color, bChild ? 0.85f : 0.95f);
			Y += bChild ? 22.f : 26.f;
		}
	}

	// --- Итог выезда ---
	if (const ADarcGameState* GS = GetWorld()->GetGameState<ADarcGameState>())
	{
		if (GS->MissionPhase == EMissionPhase::Completed)
		{
			DrawTextLine(UDarcGameplayLibrary::UIText(TEXT("Mission_Completed")), W * 0.5f, H * 0.2f, FLinearColor(0.6f, 0.9f, 0.6f), 1.6f, true);
		}
		else if (GS->MissionPhase == EMissionPhase::Failed)
		{
			DrawTextLine(UDarcGameplayLibrary::UIText(TEXT("Mission_Failed")), W * 0.5f, H * 0.2f, FLinearColor(0.9f, 0.4f, 0.35f), 1.6f, true);
		}
	}

	// --- Субтитры ---
	Subtitles.RemoveAll([Now](const FSubtitle& S) { return S.EndTime < Now; });
	float SubY = H * 0.78f;
	for (const FSubtitle& Sub : Subtitles)
	{
		const FText Line = Sub.Speaker.IsEmpty() ? Sub.Line : FText::Format(UDarcGameplayLibrary::UIText(TEXT("HUD_SubtitleFormat")), Sub.Speaker, Sub.Line);
		DrawTextLine(Line, W * 0.5f, SubY, FLinearColor(1.f, 0.95f, 0.8f), 1.1f, true);
		SubY += 30.f;
	}
}
