// DarcIntroDrive.h
// Заезд в начале выезда: игроки сидят в машине (водитель — хост), машина сама едет по
// дороге до КПП, останавливается, игроки выходят — дальше обычная игра. Управление не
// отбирается: головой крутить можно (взгляд привязан к машине), ходить — нет.
//
// Сеть: сервер раздаёт места и время старта (по серверным часам), машину каждая машина
// двигает сама по одной и той же траектории — без рывков от репликации. Посадка/высадка —
// на сервере, клиенты повторяют её по OnRep (привязка к BodyPivot реплицируется сама).
//
// Модель: части машины (импорт без объединения) в массиве CarParts. Руль — части с
// «steeringwheel» в имени, колёса — «wheel»/«tire»/«tyre»/«rim» (по 4 углам), фары —
// «headlight», задние фонари — «taillight»/«rearlight». Сиденья — «seatfl»/«seatfr»/«rearseat».
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarcIntroDrive.generated.h"

class USplineComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USpotLightComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class ACharacter;

UCLASS()
class DARK_API ADarcIntroDrive : public AActor
{
	GENERATED_BODY()

public:
	ADarcIntroDrive();

	// --- Модель ---

	/** Все части модели машины (вершины в общих координатах машины). */
	UPROPERTY(EditAnywhere, Category = "Car")
	TArray<TObjectPtr<UStaticMesh>> CarParts;

	/** Поворот модели, чтобы её перед смотрел вдоль +X, градусы. */
	UPROPERTY(EditAnywhere, Category = "Car")
	float CarYaw = 0.f;

	UPROPERTY(EditAnywhere, Category = "Car", meta = (ClampMin = "0.01"))
	float CarScale = 1.f;

	/** Наклон рулевой колонки (вниз к панели — отрицательный), градусы. */
	UPROPERTY(EditAnywhere, Category = "Car")
	float SteeringTilt = -22.f;

	/** Сколько оборотов руля на угол колёс (передаточное число). */
	UPROPERTY(EditAnywhere, Category = "Car")
	float SteeringRatio = 9.f;

	/** Самая быстрая скорость вращения руля (обода), градусы в секунду. */
	UPROPERTY(EditAnywhere, Category = "Car")
	float MaxWheelTurnRate = 140.f;

	/** Сглаживание курса машины: курс берётся по хорде ± столько см маршрута. */
	UPROPERTY(EditAnywhere, Category = "Car")
	float YawSmoothing = 250.f;

	/** Сиденья в координатах машины (перед +X, право +Y, пол машины Z=0). Пусто — по частям «seat». */
	UPROPERTY(EditAnywhere, Category = "Car")
	TArray<FVector> SeatOffsets;

	/** Поправка ко всем местам (подогнать позу сидя под сиденье). */
	UPROPERTY(EditAnywhere, Category = "Car")
	FVector SeatAdjust = FVector::ZeroVector;

	/** Дополнительная поправка только для водителя (руки на руль: вперёд/назад X, вверх/вниз Z). */
	UPROPERTY(EditAnywhere, Category = "Car")
	FVector DriverAdjust = FVector::ZeroVector;

	// --- Маршрут ---

	/** Точки дороги (мировые) от начала до места остановки у КПП. */
	UPROPERTY(EditAnywhere, Category = "Route")
	TArray<FVector> PathPoints;

	/** Скорость, см/с (~30 км/ч), разгон и торможение, см/с². */
	UPROPERTY(EditAnywhere, Category = "Route")
	float MaxSpeed = 850.f;

	UPROPERTY(EditAnywhere, Category = "Route")
	float Acceleration = 220.f;

	UPROPERTY(EditAnywhere, Category = "Route")
	float Braking = 260.f;

	/** Через сколько после появления игроков машина трогается, с. */
	UPROPERTY(EditAnywhere, Category = "Route")
	float StartDelay = 1.5f;

	/** Сколько стоять после остановки, прежде чем все выйдут, с. */
	UPROPERTY(EditAnywhere, Category = "Route")
	float ExitDelay = 1.2f;

	// --- Подвеска ---

	/** Жёсткость и демпфирование «подвески» (кузов догоняет неровности с лёгким раскачиванием). */
	UPROPERTY(EditAnywhere, Category = "Suspension")
	float SpringStiffness = 70.f;

	UPROPERTY(EditAnywhere, Category = "Suspension")
	float SpringDamping = 6.f;

	/** Мелкая тряска на гравии, см. */
	UPROPERTY(EditAnywhere, Category = "Suspension")
	float RoadBumps = 1.2f;

	/** Крен/клевок кузова от ускорений, градусы на м/с². */
	UPROPERTY(EditAnywhere, Category = "Suspension")
	float BodyLean = 0.6f;

	/** Ход подвески колеса вверх/вниз, см: каждое колесо дотягивается до земли само. */
	UPROPERTY(EditAnywhere, Category = "Suspension")
	float WheelTravel = 14.f;

	// --- Спидометр (цифры на приборке поверх экрана) ---

	/** Поправка места цифр относительно руля (координаты машины, см) и их поворот. */
	UPROPERTY(EditAnywhere, Category = "Speedometer")
	FVector SpeedoAdjust = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Speedometer")
	FRotator SpeedoRotation = FRotator(0.f, 180.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "Speedometer")
	float SpeedoSize = 3.f;

	// --- Свет и звук ---

	UPROPERTY(EditAnywhere, Category = "Lights")
	float HeadlightLumens = 9000.f;

	UPROPERTY(EditAnywhere, Category = "Lights")
	float HeadlightRange = 4500.f;

	/** Насколько источник света фар утоплен в корпус фары (назад от её переднего края), см. */
	UPROPERTY(EditAnywhere, Category = "Lights")
	float HeadlightInset = 10.f;

	/** Красная подсветка от задних фонарей (каждый), люмены. */
	UPROPERTY(EditAnywhere, Category = "Lights")
	float TailLightLumens = 40.f;

	/** Слоты звуков: музыка в машине — случайный из Slot, Slot_1 … Slot_8; мотор — петля. */
	UPROPERTY(EditAnywhere, Category = "Sound")
	FName RadioSlot = TEXT("CarRadio");

	UPROPERTY(EditAnywhere, Category = "Sound")
	FName EngineSlot = TEXT("CarEngine");

	/** Мотор на холостых, когда все вышли (снаружи, тихо). Нет звука в слоте — остаётся глухой EngineSlot. */
	UPROPERTY(EditAnywhere, Category = "Sound")
	FName IdleSlot = TEXT("CarIdle");

	UPROPERTY(EditAnywhere, Category = "Sound", meta = (ClampMin = "0", ClampMax = "1"))
	float IdleVolume = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Sound")
	FName DoorSlot = TEXT("CarDoor");

	UPROPERTY(EditAnywhere, Category = "Sound")
	float RadioVolume = 0.55f;

	/** Голос эфира между треками (джинглы, диджей, время, погода): Slot, Slot_1 … Slot_31. */
	UPROPERTY(EditAnywhere, Category = "Sound")
	FName RadioVoiceSlot = TEXT("RadioVoice");

	/** Вероятность вставки голоса между треками (0..1). */
	UPROPERTY(EditAnywhere, Category = "Sound", meta = (ClampMin = "0", ClampMax = "1"))
	float RadioVoiceChance = 0.6f;

	/** После выхода: музыка и мотор глухо, «за закрытыми дверями» (частота среза, Гц). */
	UPROPERTY(EditAnywhere, Category = "Sound")
	float ParkedLowPassHz = 900.f;

	/** Дальше этого от машины (см) свой игрок отошёл — радио и мотор выключаются насовсем. */
	UPROPERTY(EditAnywhere, Category = "Sound")
	float RadioOffDistance = 5000.f;

	/** Анимации (слоты Animations): водитель и пассажиры (Car_Sit, Car_Sit_1 …). */
	UPROPERTY(EditAnywhere, Category = "Animation")
	FName DriverAnimation = TEXT("Car_Drive");

	UPROPERTY(EditAnywhere, Category = "Animation")
	FName PassengerAnimation = TEXT("Car_Sit");

	/** Насколько можно повернуть голову от направления машины, градусы. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float LookYawLimit = 120.f;

	/** Корень всего, что двигается (кузов + колёса). */
	UPROPERTY(VisibleAnywhere, Category = "Car")
	TObjectPtr<USceneComponent> CarRoot;

	/** Кузов с раскачкой подвески; к нему привязаны игроки. */
	UPROPERTY(VisibleAnywhere, Category = "Car")
	TObjectPtr<USceneComponent> BodyPivot;

	UPROPERTY(VisibleAnywhere, Category = "Route")
	TObjectPtr<USplineComponent> Route;

	UFUNCTION(BlueprintPure, Category = "Car")
	bool IsRiding() const { return RideStartTime > 0.f && !bArrived; }

	/** Куда тянуть кисть водителя (мировые координаты): обод руля слева (-1) / справа (+1). */
	FVector GetHandGrip(float Side) const;

	/** Куда смотрит локоть: вниз и наружу от линии плечо — кисть. */
	FVector GetElbowHint(float Side, const FVector& Shoulder, const FVector& Grip) const;

	/** Хват руля: поправка точки кисти от обода (координаты руля: X — к панели, Y — наружу, Z — вверх), см. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	FVector HandGripAdjust = FVector(-6.f, 0.f, -3.f);

	/** Хват по ободу: угол от горизонтали (0 — «9 и 3 часа», вверх — положительный), градусы. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float HandGripAngle = 15.f;

	/** Руки поворачиваются вместе с рулём не дальше этого угла (дальше — «перехват»), градусы. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float HandFollowLimit = 60.f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Время старта по серверным часам (0 — ещё не поехали). */
	UPROPERTY(ReplicatedUsing = OnRep_RideStart)
	float RideStartTime = 0.f;

	/** Кто на каком месте (индекс = место, 0 — водитель). */
	UPROPERTY(ReplicatedUsing = OnRep_Seats)
	TArray<TObjectPtr<APawn>> Seats;

	UPROPERTY(ReplicatedUsing = OnRep_Arrived)
	bool bArrived = false;

	/** Какой трек играет (одинаковый у всех). */
	UPROPERTY(Replicated)
	int32 RadioIndex = 0;

	UFUNCTION()
	void OnRep_RideStart();

	UFUNCTION()
	void OnRep_Seats();

	UFUNCTION()
	void OnRep_Arrived();

private:
	void BuildCar();
	void BuildProfile();
	float DistanceAt(float Time, float& OutSpeed, float& OutAccel) const;
	void UpdateCar(float DeltaSeconds);
	FVector SeatLocal(int32 Index) const;
	FVector ExitLocation(int32 Index) const;

	/** Сервер: стартовать и посадить всех, кто уже есть (и потом опоздавших). */
	void ServerStartRide();
	void ServerSeatNewcomers();

	void SeatLocally(APawn* Pawn, int32 Index);
	void UnseatLocally(APawn* Pawn, int32 Index);
	void BeginExit();
	void FinishExit();
	void StartRadio();
	/** Эфир: треки в перемешанном порядке, между ними иногда голос. Одинаковый у всех (сид — RadioIndex). */
	void BuildRadioProgram();
	/** Включить то, что по программе звучит в момент Time (секунды с начала поездки). */
	void PlayRadioAt(float Time);
	UFUNCTION()
	void OnRadioItemFinished();
	/** Машина стоит у КПП: глушим звук «за дверями», далеко ушли — выключаем. */
	void UpdateParkedSound();

	float ServerTime() const;

	// Части модели.
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CarModel;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> SteeringSpin;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> WheelSteer;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> WheelSpin;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BrakeLights;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RadioAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> EngineAudio;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> RadioProgram;

	float RadioProgramLength = 0.f;
	bool bParkedSound = false;
	bool bRadioOff = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> WheelPivots;

	/** Рамка руля (ось колонки — X) без вращения руля. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> SteeringFrame;

	float SteeringRadius = 18.f;

	UPROPERTY(Transient)
	TObjectPtr<class UTextRenderComponent> Speedo;

	TArray<FVector> AutoSeats;      // места в координатах BodyPivot
	TArray<bool> WheelIsFront;
	TArray<FVector> WheelRest;      // место колеса в координатах модели (без хода подвески)
	TArray<FVector> WheelCarPos;    // центр колеса в координатах машины
	TArray<float> WheelOffsets;     // текущий ход подвески, см
	int32 LastKmh = -1;
	FVector2D WheelBase = FVector2D(150.f, 80.f); // половины колёсной базы и колеи
	float WheelRadius = 38.f;
	float HalfWidth = 100.f;

	// Профиль скорости: разгон, ход, торможение.
	float RouteLength = 0.f;
	float AccelTime = 0.f, CruiseTime = 0.f, BrakeTime = 0.f, TopSpeed = 0.f;

	// Подвеска (у каждой машины своя, детерминированная цель).
	bool bCarPlaced = false;
	float SuspZ = 0.f, SuspZVel = 0.f;
	float SuspPitch = 0.f, SuspPitchVel = 0.f;
	float SuspRoll = 0.f, SuspRollVel = 0.f;
	float LeanPitch = 0.f, LeanRoll = 0.f;
	float SteerAngle = 0.f;
	float WheelAngle = 0.f;
	float LastYaw = 0.f;
	float PrevSpeed = 0.f;
	bool bLocalExitDone = false;

	TSet<TWeakObjectPtr<APawn>> SeatedLocally;
	TMap<TWeakObjectPtr<APawn>, bool> SavedUseYaw;
	FTimerHandle StartTimer, SeatTimer, ExitTimer;
};
