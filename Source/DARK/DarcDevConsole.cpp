// DarcDevConsole.cpp
// Консоль разработчика: команды Darc* у ADarcPlayerController (открыть консоль — ` / Ё).
// Работают в редакторе и в собранной Development-игре, в Shipping — молчат.
// Всё, что меняет игру (здоровье, задачи, питание, память кампании), выполняет сервер.
#include "DarcPlayerController.h"
#include "DarcFlashlightComponent.h"
#include "DarcGameMode.h"
#include "DarcGameState.h"
#include "DarcIntroDrive.h"
#include "DarcPowerSubsystem.h"
#include "DarcRoomVolume.h"
#include "DarcShadowFigure.h"
#include "DarcVitalsComponent.h"
#include "DarcVoiceLog.h"
#include "DarcWorldMemorySubsystem.h"
#include "TaskManagerComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

namespace
{
	constexpr bool bDevAllowed = !UE_BUILD_SHIPPING;

	const TCHAR* StatusName(EDarcTaskStatus Status)
	{
		switch (Status)
		{
		case EDarcTaskStatus::Locked:    return TEXT("закрыта");
		case EDarcTaskStatus::Active:    return TEXT("АКТИВНА");
		case EDarcTaskStatus::Completed: return TEXT("выполнена");
		case EDarcTaskStatus::Failed:    return TEXT("провалена");
		case EDarcTaskStatus::Obsolete:  return TEXT("не понадобилась");
		default:                         return TEXT("?");
		}
	}
}

void ADarcPlayerController::Client_DevPrint_Implementation(const FString& Text)
{
	ClientMessage(Text);
	UE_LOG(LogTemp, Display, TEXT("DarcDev: %s"), *Text);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor(255, 220, 120), Text);
	}
}

void ADarcPlayerController::DarcHelp()
{
	if (!bDevAllowed)
	{
		return;
	}
	Client_DevPrint(TEXT(
		"DarcGod — без урона (вкл/выкл) | DarcHeal — здоровье и выносливость | DarcKill — умереть (стать духом)\n"
		"DarcBattery 0..100 — заряд фонаря | DarcFear 0..1 — страх | DarcShadow — тень рядом\n"
		"DarcTasks — список задач | DarcTask <Id> — засчитать задачу\n"
		"DarcMains 0/1 — сеть здания | DarcFact <Имя> — факт кампании | DarcFacts — список фактов\n"
		"DarcRooms — комнаты | DarcGoto <комната> — телепорт | DarcVoiceLogs — голосовые записи\n"
		"DarcSkipIntro — пропустить поездку | DarcEndMission 1/0 — закончить выезд успехом/провалом\n"
		"Плюс движок: stat fps, stat unit, t.MaxFPS 0"));
}

void ADarcPlayerController::DarcGod()                          { Server_Dev(TEXT("God"), FString()); }
void ADarcPlayerController::DarcHeal()                         { Server_Dev(TEXT("Heal"), FString()); }
void ADarcPlayerController::DarcKill()                         { Server_Dev(TEXT("Kill"), FString()); }
void ADarcPlayerController::DarcBattery(int32 Percent)         { Server_Dev(TEXT("Battery"), FString::FromInt(Percent)); }
void ADarcPlayerController::DarcTask(const FString& TaskId)    { Server_Dev(TEXT("Task"), TaskId); }
void ADarcPlayerController::DarcMains(int32 On)                { Server_Dev(TEXT("Mains"), FString::FromInt(On)); }
void ADarcPlayerController::DarcFact(const FString& Fact)      { Server_Dev(TEXT("Fact"), Fact); }
void ADarcPlayerController::DarcFacts()                        { Server_Dev(TEXT("Facts"), FString()); }
void ADarcPlayerController::DarcGoto(const FString& Room)      { Server_Dev(TEXT("Goto"), Room); }
void ADarcPlayerController::DarcSkipIntro()                    { Server_Dev(TEXT("SkipIntro"), FString()); }
void ADarcPlayerController::DarcEndMission(int32 Success)      { Server_Dev(TEXT("EndMission"), FString::FromInt(Success)); }

void ADarcPlayerController::DarcFear(float Value)
{
	// Страх — только у своей машины.
	UDarcVitalsComponent* Vitals = bDevAllowed && GetPawn() ? GetPawn()->FindComponentByClass<UDarcVitalsComponent>() : nullptr;
	if (Vitals)
	{
		Vitals->SetFear(Value);
		Client_DevPrint(FString::Printf(TEXT("Страх: %.2f (база уровня %.2f)"), Vitals->GetFear(), Vitals->GetBaseFear()));
	}
}

void ADarcPlayerController::DarcShadow()
{
	// Тень — только у своей машины.
	if (!bDevAllowed)
	{
		return;
	}
	SpawnShadowNow();
	Client_DevPrint(Shadow.IsValid() ? TEXT("Тень поставлена — обернись") : TEXT("Тень: не нашлось места (нужен пол и прямая видимость в 8–20 м сбоку/сзади)"));
}

void ADarcPlayerController::DarcTasks()
{
	const UTaskManagerComponent* Tasks = bDevAllowed ? UTaskManagerComponent::GetTaskManager(this) : nullptr;
	if (!Tasks)
	{
		Client_DevPrint(TEXT("Задач нет (выезд не начат)"));
		return;
	}
	FString Out;
	for (const FDarcTaskState& Task : Tasks->GetTasks())
	{
		Out += FString::Printf(TEXT("%s%s — %s — %s\n"), Task.bEmergent ? TEXT("  + ") : TEXT(""), *Task.GetId().ToString(),
			StatusName(Task.Status), *Task.Definition.Title.ToString());
	}
	Client_DevPrint(Out.IsEmpty() ? TEXT("Задач нет") : Out);
}

void ADarcPlayerController::DarcRooms()
{
	if (!bDevAllowed)
	{
		return;
	}
	FString Out;
	for (TActorIterator<ADarcRoomVolume> It(GetWorld()); It; ++It)
	{
		Out += It->RoomId.ToString() + TEXT("  ");
	}
	Client_DevPrint(Out.IsEmpty() ? TEXT("Комнат нет") : TEXT("Комнаты: ") + Out);
}

void ADarcPlayerController::DarcVoiceLogs()
{
	if (!bDevAllowed)
	{
		return;
	}
	FString Out;
	for (TActorIterator<ADarcVoiceLog> It(GetWorld()); It; ++It)
	{
		Out += FString::Printf(TEXT("%s (%d реплик) у %s\n"), *It->LogId.ToString(), It->NumLines, *It->GetActorLocation().ToCompactString());
	}
	Client_DevPrint(Out.IsEmpty() ? TEXT("Голосовых записей на уровне нет") : Out);
}

void ADarcPlayerController::Server_Dev_Implementation(const FString& Command, const FString& Arg)
{
	if (!bDevAllowed)
	{
		return;
	}
	APawn* MyPawn = GetPawn();
	UWorld* World = GetWorld();
	auto Is = [&Command](const TCHAR* Name) { return Command.Equals(Name, ESearchCase::IgnoreCase); };

	if (Is(TEXT("God")) || Is(TEXT("Heal")))
	{
		UDarcVitalsComponent* Vitals = MyPawn ? MyPawn->FindComponentByClass<UDarcVitalsComponent>() : nullptr;
		if (!Vitals)
		{
			Client_DevPrint(TEXT("Нет персонажа"));
			return;
		}
		if (Is(TEXT("God")))
		{
			Vitals->bGodMode = !Vitals->bGodMode;
			Client_DevPrint(Vitals->bGodMode ? TEXT("Без урона: ВКЛ") : TEXT("Без урона: выкл"));
		}
		Vitals->DevRestore();
	}
	else if (Is(TEXT("Kill")))
	{
		if (ADarcGameMode* GM = World->GetAuthGameMode<ADarcGameMode>())
		{
			GM->KillPlayer(this, nullptr);
		}
	}
	else if (Is(TEXT("Battery")))
	{
		if (UDarcFlashlightComponent* Flashlight = MyPawn ? MyPawn->FindComponentByClass<UDarcFlashlightComponent>() : nullptr)
		{
			Flashlight->ServerSetCharge(FCString::Atoi(*Arg) / 100.f);
			Client_DevPrint(FString::Printf(TEXT("Фонарь: %d%%"), FMath::RoundToInt(Flashlight->GetCharge() * 100.f)));
		}
	}
	else if (Is(TEXT("Task")))
	{
		UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this);
		const bool bDone = Tasks && Tasks->CompleteTask(FName(*Arg), MyPawn);
		Client_DevPrint(FString::Printf(TEXT("Задача %s: %s"), *Arg, bDone ? TEXT("засчитана") : TEXT("не засчитана (нет такой или она не активна — см. DarcTasks)")));
	}
	else if (Is(TEXT("Mains")))
	{
		if (UDarcPowerSubsystem* Power = UDarcPowerSubsystem::GetPower(this))
		{
			Power->SetMainsOn(FCString::Atoi(*Arg) != 0);
			Client_DevPrint(Power->IsMainsOn() ? TEXT("Сеть здания: есть") : TEXT("Сеть здания: отключена"));
		}
	}
	else if (Is(TEXT("Fact")) || Is(TEXT("Facts")))
	{
		UDarcWorldMemorySubsystem* Memory = UDarcWorldMemorySubsystem::GetWorldMemory(this);
		if (!Memory)
		{
			return;
		}
		if (Is(TEXT("Fact")) && !Arg.IsEmpty())
		{
			Memory->AddCampaignFact(FName(*Arg));
		}
		FString Out;
		for (const FName& Fact : Memory->GetCampaignFacts())
		{
			Out += Fact.ToString() + TEXT("  ");
		}
		Client_DevPrint(FString::Printf(TEXT("Уровень %d. Факты кампании: %s"), Memory->GetCurrentLevelIndex(), Out.IsEmpty() ? TEXT("нет") : *Out));
	}
	else if (Is(TEXT("Goto")))
	{
		if (!MyPawn || MyPawn->GetAttachParentActor())
		{
			Client_DevPrint(TEXT("Сначала выйди из машины (DarcSkipIntro)"));
			return;
		}
		for (TActorIterator<ADarcRoomVolume> It(World); It; ++It)
		{
			if (It->RoomId.ToString().Contains(Arg))
			{
				MyPawn->TeleportTo(It->GetActorLocation(), MyPawn->GetActorRotation());
				Client_DevPrint(TEXT("Телепорт: ") + It->RoomId.ToString());
				return;
			}
		}
		Client_DevPrint(TEXT("Нет такой комнаты — см. DarcRooms"));
	}
	else if (Is(TEXT("SkipIntro")))
	{
		for (TActorIterator<ADarcIntroDrive> It(World); It; ++It)
		{
			It->DevSkipRide();
		}
	}
	else if (Is(TEXT("EndMission")))
	{
		if (UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(this))
		{
			Tasks->DevFinishMission(FCString::Atoi(*Arg) != 0);
		}
	}
}
