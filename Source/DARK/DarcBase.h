// DarcBase.h
// База D.A.R.C. между выездами (docs/DARC_base_corridor.md): гараж + прямой коридор.
//  - ADarcBaseCurator — куратор в лобби (первая дверь справа). Его не видно, говорит через динамики
//    лобби: при первом входе игрока выдаёт задание, после выезда сначала принимает данные.
//    Заодно выдаёт факты базы за пройденный выезд (открывают комнаты) и выбирает карту следующего выезда.
//  - ADarcBaseCar — машина в гараже: после выдачи задания E — сесть; когда сели все живые — переезд.
//  - ADarcMissionTerminal — терминал «Объекты» в лобби: список выездов (пройденные — повторить,
//    следующий по сюжету — открыт, дальше — «засекречено»), выбор видят все на экране лобби.
//  - ADarcLightDoor — закрытая дверь у тупика: из-под порога мерцает свет, после 7-го уровня горит
//    постоянно, за дверью гул, который смолкает при приближении (мастер-документ, разд. 102).
// Ставит всё это скрипт Tools/darc_build_base.py. Реплики — ключи ST_UI, голос — слоты звуков с тем же именем.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcBase.generated.h"

class UBoxComponent;
class URectLightComponent;
class UAudioComponent;

/** Один выезд в списке куратора. */
USTRUCT()
struct FDarcBaseMission
{
	GENERATED_BODY()

	/** Номер уровня (1..7). */
	UPROPERTY(Config, EditAnywhere, Category = "Base")
	int32 Level = 1;

	/** Карта выезда, например /Game/DARC/Maps/1LvlMap. */
	UPROPERTY(Config, EditAnywhere, Category = "Base")
	FString Map;

	/** Реплики при выдаче задания (ключи ST_UI). */
	UPROPERTY(Config, EditAnywhere, Category = "Base")
	TArray<FName> Briefing;

	/** Реплики при возвращении после успешного выезда этого уровня. */
	UPROPERTY(Config, EditAnywhere, Category = "Base")
	TArray<FName> Return;

	/** Факты кампании за успешный выезд — открывают комнаты базы (Base.RestRoom и т.п.). */
	UPROPERTY(Config, EditAnywhere, Category = "Base")
	TArray<FName> Facts;
};

UCLASS(Config = Game)
class DARK_API ADarcBaseCurator : public AActor
{
	GENERATED_BODY()

public:
	ADarcBaseCurator();

	/** Зона лобби: первый вошедший игрок запускает разговор. Размер ставит скрипт сборки. */
	UPROPERTY(VisibleAnywhere, Category = "Base")
	TObjectPtr<UBoxComponent> Lobby;

	/** Выезды по порядку (Config/DefaultGame.ini, [/Script/DARK.DarcBaseCurator]). */
	UPROPERTY(Config, EditAnywhere, Category = "Base")
	TArray<FDarcBaseMission> Missions;

	/** Последствия выезда: если в кампании есть факт — после приёма данных куратор добавляет реплику. */
	UPROPERTY(Config, EditAnywhere, Category = "Base")
	TMap<FName, FName> ReturnFactLines;

	/** Реплики при возвращении после проваленного выезда. */
	UPROPERTY(Config, EditAnywhere, Category = "Base")
	TArray<FName> FailedReturn;

	/** Радиус, в котором слышны динамики лобби, см. */
	UPROPERTY(EditAnywhere, Category = "Base")
	float SpeakerRadius = 1600.f;

	/** Задание выдано — машина готова. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Base")
	bool bTaskGiven = false;

	/** Карта, куда поедет машина (выбор на терминале, по умолчанию — следующий по сюжету). */
	FString GetNextMap() const;

	/** Следующий уровень по сюжету и выбранный на терминале. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Base")
	int32 StoryLevel = 1;
	UPROPERTY(ReplicatedUsing = OnRep_Selected, BlueprintReadOnly, Category = "Base")
	int32 SelectedLevel = 1;

	/** Можно ли выбрать уровень (пройден или следующий по сюжету, и у него есть карта). */
	bool IsLevelAvailable(int32 Level) const;
	const FDarcBaseMission* FindMission(int32 Level) const;

	/** Сервер: выбор на терминале (с проверкой). */
	void SelectLevel(int32 Level);

	/** Экран в лобби с выбранным объектом (надпись ставит скрипт, Id — тег "DARC.MissionScreen"). */
	void RefreshScreens() const;

	static ADarcBaseCurator* Find(const UObject* WorldContextObject);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnLobbyEnter(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void SayNextLine();
	void SayLine(FName Key, float Duration);

	UFUNCTION()
	void OnRep_Selected();

	TArray<FName> Script;
	int32 ScriptIndex = 0;
	bool bStarted = false;
	FString NextMap;
	FTimerHandle LineTimer;
};

UCLASS()
class DARK_API ADarcBaseCar : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcBaseCar();

	/** Невидимая коробка вокруг модели машины: по ней наводимся и жмём E. Модель ставит скрипт отдельно. */
	UPROPERTY(VisibleAnywhere, Category = "Base")
	TObjectPtr<UBoxComponent> Body;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Кто уже сел (PlayerState). */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<APlayerState>> Seated;

	bool bDeparting = false;
	FTimerHandle DepartTimer;

	void CheckDeparture();
	void Travel();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FadeOut();
};

UCLASS()
class DARK_API ADarcMissionTerminal : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcMissionTerminal();

	/** Невидимая коробка поверх модели терминала (модель ставит скрипт). */
	UPROPERTY(VisibleAnywhere, Category = "Base")
	TObjectPtr<UBoxComponent> Body;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	/** Окно выбора у этого игрока (вызывает его контроллер по команде сервера). */
	static void OpenSelectionUI(APlayerController* Owner);
};

UCLASS(Config = Game)
class DARK_API ADarcLightDoor : public AActor
{
	GENERATED_BODY()

public:
	ADarcLightDoor();

	/** Свет из щели под дверью (направлен в коридор). */
	UPROPERTY(VisibleAnywhere, Category = "Base")
	TObjectPtr<URectLightComponent> Glow;

	/** Факт, после которого свет горит постоянно (после 7-го уровня). */
	UPROPERTY(Config, EditAnywhere, Category = "Base")
	FName SteadyFact = TEXT("Arc1.Level7Done");

	/** Ближе этого гул за дверью смолкает, см. */
	UPROPERTY(EditAnywhere, Category = "Base")
	float QuietDistance = 260.f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Steady)
	bool bSteady = false;

	UFUNCTION()
	void OnRep_Steady();

	void Flicker();
	FTimerHandle FlickerTimer;
	float BaseIntensity = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Hum;
};
