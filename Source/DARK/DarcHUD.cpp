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
#include "DarcFlashlightComponent.h"
#include "DarcVitalsComponent.h"
#include "DarcInventoryComponent.h"
#include "DarcTool.h"
#include "Engine/Texture2D.h"
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

void ADarcHUD::DrawHudTexture(FName Name, float X, float Y, float Width, float Height, float U0, float U1, const FLinearColor& Tint)
{
	// Картинки HUD — из RawAssets/Textures/UI/DARC_HUD (импорт в /Game/DARC/Imported/Textures/UI/DARC_HUD).
	TObjectPtr<UTexture2D>* Cached = HudTextures.Find(Name);
	if (!Cached)
	{
		// Имя с путём («Guard_Portrait/T_Guard_Front») — из UI-пака D.A.R.C., иначе — из DARC_HUD.
		const FString Key = Name.ToString();
		FString Folder = TEXT("DARC_HUD"), Asset = Key;
		if (Key.Split(TEXT("/"), &Folder, &Asset))
		{
			Folder = TEXT("DARC_UI_Pack/") + Folder;
		}
		const FString Path = FString::Printf(TEXT("/Game/DARC/Imported/Textures/UI/%s/%s.%s"), *Folder, *Asset, *Asset);
		Cached = &HudTextures.Add(Name, LoadObject<UTexture2D>(nullptr, *Path));
	}
	if (*Cached && Width > 0.5f && Height > 0.5f)
	{
		DrawTexture(*Cached, X, Y, Width, Height, U0, 0.f, U1 - U0, 1.f, Tint, BLEND_Translucent);
	}
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

	const float S = H / 1080.f; // элементы HUD нарисованы под 1080p

	// --- Урон: вспышка по краям экрана, при малом здоровье — постоянная тень ---
	const UDarcVitalsComponent* Vitals = Pawn ? Pawn->FindComponentByClass<UDarcVitalsComponent>() : nullptr;
	if (Vitals)
	{
		const float Flash = FMath::Clamp(1.f - Vitals->GetSecondsSinceDamage() / 1.2f, 0.f, 1.f);
		const float LowHealth = FMath::Clamp((0.5f - Vitals->GetHealth01()) * 1.2f, 0.f, 0.6f);
		const float Alpha = FMath::Max(Flash * 0.85f, LowHealth);
		if (Alpha > 0.01f)
		{
			DrawHudTexture(TEXT("T_11_damage_vignette"), 0.f, 0.f, W, H, 0.f, 1.f, FLinearColor(1.f, 1.f, 1.f, Alpha));
		}
		// Страх: края экрана темнеют и медленно «дышат».
		const float Fear = Vitals->GetFear();
		const float FearAlpha = FMath::Clamp((Fear - 0.4f) * 1.3f, 0.f, 0.6f) * (0.85f + 0.15f * FMath::Sin(Now * 2.2f));
		if (FearAlpha > 0.01f)
		{
			DrawHudTexture(TEXT("T_11_damage_vignette"), 0.f, 0.f, W, H, 0.f, 1.f, FLinearColor(0.f, 0.f, 0.f, FearAlpha));
		}
	}

	// --- Прицел ---
	const float Cross = 22.f * S;
	DrawHudTexture(TEXT("T_01_crosshair"), W * 0.5f - Cross * 0.5f, H * 0.5f - Cross * 0.5f, Cross, Cross, 0.f, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.7f));

	// --- Здоровье, выносливость, статусы (слева внизу) ---
	if (Vitals)
	{
		const float X = 32.f * S;
		// Полоса = рамка + заливка по доле (кадр заливки обрезается справа).
		auto Bar = [this](FName Frame, FName Fill, float BX, float BY, float BW, float BH, float FillU0, float FillU1, float Fraction, float Alpha)
		{
			DrawHudTexture(Fill, BX + BW * FillU0, BY, BW * (FillU1 - FillU0) * Fraction, BH, FillU0, FillU0 + (FillU1 - FillU0) * Fraction, FLinearColor(1.f, 1.f, 1.f, Alpha));
			DrawHudTexture(Frame, BX, BY, BW, BH, 0.f, 1.f, FLinearColor(1.f, 1.f, 1.f, Alpha));
		};
		const float HealthW = 360.f * S, HealthH = HealthW * 150.f / 1200.f;
		const float StaminaW = 300.f * S, StaminaH = StaminaW * 120.f / 1200.f;
		const float HealthY = H - 40.f * S - StaminaH - 8.f * S - HealthH;
		Bar(TEXT("T_02_health_frame"), TEXT("T_02_health_fill"), X, HealthY, HealthW, HealthH, 30.f / 1200.f, 1170.f / 1200.f, Vitals->GetHealth01(), 0.9f);
		// Выносливость полная — полоса приглушена, чтобы не мозолила глаза.
		const float Stamina = Vitals->GetStamina01();
		Bar(TEXT("T_03_stamina_frame"), TEXT("T_03_stamina_fill"), X, H - 40.f * S - StaminaH, StaminaW, StaminaH, 25.f / 1200.f, 1175.f / 1200.f, Stamina, Stamina > 0.99f ? 0.35f : 0.9f);

		// Батарейка фонаря — справа от полосы здоровья: видна, пока фонарь горит или батарейка не полная.
		if (const UDarcFlashlightComponent* Flashlight = Pawn->FindComponentByClass<UDarcFlashlightComponent>())
		{
			const float Charge = Flashlight->GetCharge();
			if (Flashlight->IsOn() || Charge < 0.995f)
			{
				const bool bDead = Charge <= 0.f;
				const bool bLow = Charge < 0.25f;
				const float Blink = bLow ? 0.55f + 0.45f * FMath::Abs(FMath::Sin(Now * 3.f)) : 0.85f;
				const FText Label = bDead ? UDarcGameplayLibrary::UIText(TEXT("HUD_BatteryDead"))
					: FText::Format(UDarcGameplayLibrary::UIText(TEXT("HUD_Battery")), FText::AsNumber(FMath::CeilToInt(Charge * 100.f)));
				DrawTextLine(Label, X + HealthW + 14.f * S, HealthY + HealthH * 0.2f,
					bLow ? FLinearColor(1.f, 0.45f, 0.3f, Blink) : FLinearColor(1.f, 0.92f, 0.7f, Blink), 0.75f);
			}
		}

		static const TMap<FName, FName> StatusIcons = {
			{ TEXT("Fear"), TEXT("T_04_status_1_fear") }, { TEXT("Bleeding"), TEXT("T_04_status_2_bleeding") },
			{ TEXT("Poison"), TEXT("T_04_status_3_poison") }, { TEXT("Infection"), TEXT("T_04_status_4_infection") } };
		float IconX = X;
		const float Icon = 40.f * S;
		// Страх — свой у каждого игрока (не из серверного списка статусов).
		if (Vitals->GetFear() > 0.35f)
		{
			DrawHudTexture(TEXT("T_04_status_1_fear"), IconX, HealthY - Icon - 6.f * S, Icon, Icon, 0.f, 1.f,
				FLinearColor(1.f, 1.f, 1.f, FMath::Clamp((Vitals->GetFear() - 0.35f) * 4.f, 0.3f, 0.95f)));
			IconX += Icon + 6.f * S;
		}
		for (const FName Status : Vitals->GetStatuses())
		{
			if (const FName* Texture = StatusIcons.Find(Status))
			{
				DrawHudTexture(*Texture, IconX, HealthY - Icon - 6.f * S, Icon, Icon, 0.f, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.95f));
				IconX += Icon + 6.f * S;
			}
		}
	}

	// --- Инвентарь: 4 слота внизу по центру (1–4), активный — в руке ---
	if (const UDarcInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UDarcInventoryComponent>() : nullptr)
	{
		const float Slot = 72.f * S;
		const float Gap = 8.f * S;
		const float Total = UDarcInventoryComponent::NumSlots * Slot + (UDarcInventoryComponent::NumSlots - 1) * Gap;
		const float Y = H - 28.f * S - Slot;
		for (int32 i = 0; i < UDarcInventoryComponent::NumSlots; ++i)
		{
			const float X = (W - Total) * 0.5f + i * (Slot + Gap);
			const bool bActive = i == Inventory->GetActiveSlot();
			DrawHudTexture(bActive ? TEXT("T_05_inventory_selected") : TEXT("T_05_inventory_empty"), X, Y, Slot, Slot, 0.f, 1.f,
				FLinearColor(1.f, 1.f, 1.f, bActive ? 0.95f : 0.6f));
			DrawTextLine(FText::AsNumber(i + 1), X + 8.f * S, Y + 4.f * S, FLinearColor(1.f, 1.f, 1.f, 0.6f), 0.7f);
			if (const ACarryableItem* Item = Inventory->GetSlotItem(i))
			{
				DrawTextLine(Item->ItemName, X + Slot * 0.5f, Y + Slot * 0.42f, FLinearColor(1.f, 0.92f, 0.7f, bActive ? 1.f : 0.7f), 0.62f, true);
			}
		}
		// Подсказка клавиш — вместе со списком задач (Tab прячет и её).
		const ADarcPlayerController* DarcPC = Cast<ADarcPlayerController>(PlayerOwner);
		if (DarcPC && DarcPC->IsTaskListVisible() && !DarcPC->IsTalking())
		{
			DrawTextLine(UDarcGameplayLibrary::UIText(TEXT("HUD_InventoryHint")), W * 0.5f, Y - 24.f * S,
				FLinearColor(0.8f, 0.8f, 0.8f, 0.45f), 0.7f, true);
		}
		// Говорю (V): с рацией — в эфир, без — вслух рядом.
		if (DarcPC && DarcPC->IsTalking())
		{
			const bool bRadio = ADarcTool::HasTool(Pawn, EDarcToolType::Radio);
			DrawTextLine(UDarcGameplayLibrary::UIText(bRadio ? TEXT("Voice_Radio") : TEXT("Voice_Talking")),
				W * 0.5f, Y - 26.f * S, FLinearColor(0.6f, 1.f, 0.65f, 0.95f), 0.8f, true);
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
		// По ходу прохождения: выполненное (галочка) и то, что можно делать сейчас; будущие задачи
		// не показываем, пока не откроются. Внезапная задача («починить кабель») — прямо перед той,
		// которую она блокирует, а не в конце списка.
		const TArray<FDarcTaskState> All = Tasks->GetVisibleTasks();
		auto Shown = [&All](const FDarcTaskState& T)
		{
			if (T.Status == EDarcTaskStatus::Locked)
			{
				return false;
			}
			// Подзадача — только пока её родитель открыт (выполнен родитель — путь уже выбран).
			if (!T.Definition.ParentTaskId.IsNone())
			{
				const FDarcTaskState* Parent = All.FindByPredicate([&T](const FDarcTaskState& P) { return P.GetId() == T.Definition.ParentTaskId; });
				return !Parent || Parent->Status == EDarcTaskStatus::Active;
			}
			return true;
		};
		TArray<const FDarcTaskState*> Ordered;
		for (const FDarcTaskState& T : All)
		{
			if (T.bEmergent)
			{
				continue;
			}
			for (const FDarcTaskState& E : All)
			{
				if (E.bEmergent && T.Definition.Prerequisites.Contains(E.GetId()) && !Ordered.Contains(&E))
				{
					Ordered.Add(&E);
				}
			}
			Ordered.Add(&T);
		}
		// Внезапные, которые ничего не блокируют, — перед первой незакрытой задачей.
		int32 FirstOpen = Ordered.IndexOfByPredicate([](const FDarcTaskState* T) { return !T->IsClosed() && T->Status != EDarcTaskStatus::Locked; });
		for (const FDarcTaskState& E : All)
		{
			if (E.bEmergent && !Ordered.Contains(&E))
			{
				Ordered.Insert(&E, FirstOpen == INDEX_NONE ? Ordered.Num() : FirstOpen);
			}
		}
		for (const FDarcTaskState* TaskPtr : Ordered)
		{
			const FDarcTaskState& Task = *TaskPtr;
			if (!Shown(Task))
			{
				continue;
			}
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
	float SubY = H * 0.74f;
	const FText GuardName = UDarcGameplayLibrary::UIText(TEXT("Speaker_Guard"));
	for (const FSubtitle& Sub : Subtitles)
	{
		if (Sub.Speaker.IsEmpty())
		{
			DrawTextLine(Sub.Line, W * 0.5f, SubY, FLinearColor(1.f, 0.95f, 0.8f), 1.1f, true);
			SubY += 30.f;
			continue;
		}
		// Реплика персонажа — плашка как в диалоговом окне D.A.R.C.: имя, ниже текст; у охранника портрет.
		const bool bGuard = Sub.Speaker.EqualTo(GuardName);
		const float PanelW = 780.f * S;
		const float PanelH = (bGuard ? 92.f : 62.f) * S;
		const float X0 = (W - PanelW) * 0.5f;
		DrawRect(FLinearColor(0.015f, 0.025f, 0.045f, 0.82f), X0, SubY, PanelW, PanelH);
		DrawRect(FLinearColor(0.35f, 0.55f, 0.85f, 0.6f), X0, SubY, PanelW, 2.f * S); // светлая кромка сверху
		float TX = X0 + 16.f * S;
		if (bGuard)
		{
			const float P = PanelH - 12.f * S;
			DrawHudTexture(TEXT("Guard_Portrait/T_Guard_Front"), X0 + 6.f * S, SubY + 6.f * S, P, P, 0.f, 1.f, FLinearColor::White);
			TX += P + 4.f * S;
		}
		DrawTextLine(Sub.Speaker, TX, SubY + 8.f * S, FLinearColor(0.55f, 0.75f, 1.f), 0.85f);
		DrawTextLine(Sub.Line, TX, SubY + 34.f * S, FLinearColor(0.92f, 0.94f, 0.98f), 1.0f);
		SubY += PanelH + 8.f * S;
	}
}
