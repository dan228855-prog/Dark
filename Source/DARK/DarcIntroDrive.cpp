// DarcIntroDrive.cpp
#include "DarcIntroDrive.h"
#include "DarcAssetSettings.h"
#include "DarcPlayerController.h"
#include "DarcDriverHandsComponent.h"
#include "DarcFlashlightComponent.h"
#include "DARKCharacter.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundAttenuation.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

namespace DarcIntroDrive
{
	constexpr int32 MaxSeats = 5;
	constexpr float SeatedLift = 46.f; // от подушки сиденья до центра капсулы персонажа в позе сидя

	bool Has(const FString& Name, std::initializer_list<const TCHAR*> Keys)
	{
		for (const TCHAR* Key : Keys)
		{
			if (Name.Contains(Key, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	FBox Union(const TArray<FBox>& Boxes)
	{
		FBox Result(ForceInit);
		for (const FBox& Box : Boxes)
		{
			Result += Box;
		}
		return Result;
	}

	/** Пружина с затуханием: x догоняет цель с лёгким перелётом (подвеска). */
	void Spring(float& X, float& V, float Target, float K, float C, float Dt)
	{
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dt / (1.f / 120.f)), 1, 8);
		const float H = Dt / Steps;
		for (int32 i = 0; i < Steps; ++i)
		{
			V += (K * (Target - X) - C * V) * H;
			X += V * H;
		}
	}
}

ADarcIntroDrive::ADarcIntroDrive()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false); // машину каждая машина двигает сама по общему времени

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Route = CreateDefaultSubobject<USplineComponent>(TEXT("Route"));
	Route->SetupAttachment(RootComponent);
	CarRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CarRoot"));
	CarRoot->SetupAttachment(RootComponent);
	CarRoot->SetMobility(EComponentMobility::Movable);
	BodyPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyPivot"));
	BodyPivot->SetupAttachment(CarRoot);
}

void ADarcIntroDrive::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADarcIntroDrive, RideStartTime);
	DOREPLIFETIME(ADarcIntroDrive, Seats);
	DOREPLIFETIME(ADarcIntroDrive, bArrived);
	DOREPLIFETIME(ADarcIntroDrive, RadioIndex);
}

float ADarcIntroDrive::ServerTime() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void ADarcIntroDrive::BeginPlay()
{
	Super::BeginPlay();

	// Маршрут — в мировых координатах; сам актор стоит на месте, едет только CarRoot.
	CarRoot->SetUsingAbsoluteLocation(true);
	CarRoot->SetUsingAbsoluteRotation(true);
	Route->SetSplinePoints(PathPoints, ESplineCoordinateSpace::World, true);
	BuildCar();
	BuildProfile();
	UpdateCar(0.f);

	// Пока машина не тронулась — экран из темноты (игроки появляются и сразу садятся).
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->IsLocalController() && PC->PlayerCameraManager && !bArrived)
		{
			PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, StartDelay + 1.2f, FLinearColor::Black, false, false);
		}
	}

	if (HasAuthority() && PathPoints.Num() >= 2)
	{
		GetWorldTimerManager().SetTimer(StartTimer, this, &ADarcIntroDrive::ServerStartRide, FMath::Max(StartDelay, 0.1f), false);
	}
}

void ADarcIntroDrive::BuildCar()
{
	using namespace DarcIntroDrive;

	// Габариты частей — в координатах модели.
	TArray<FBox> PartBoxes;
	for (UStaticMesh* Mesh : CarParts)
	{
		PartBoxes.Add(Mesh ? Mesh->GetBoundingBox() : FBox(ForceInit));
	}
	const FQuat ModelRot = FRotator(0.f, CarYaw, 0.f).Quaternion();
	const FBox ModelBox = Union(PartBoxes);
	const FBox Rotated = ModelBox.IsValid
		? ModelBox.TransformBy(FTransform(ModelRot, FVector::ZeroVector, FVector(CarScale)))
		: FBox(FVector(-250.f, -100.f, 0.f), FVector(250.f, 100.f, 170.f));
	// Модель ставим так: центр кузова над началом координат, низ колёс — на Z=0.
	const FVector Offset(-Rotated.GetCenter().X, -Rotated.GetCenter().Y, -Rotated.Min.Z);
	const FTransform ModelToCar(ModelRot, Offset, FVector(CarScale));
	HalfWidth = Rotated.GetExtent().Y;
	const float HalfLength = Rotated.GetExtent().X;
	const float Height = Rotated.GetSize().Z;

	CarModel = NewObject<USceneComponent>(this, TEXT("CarModel"));
	CarModel->SetupAttachment(BodyPivot);
	CarModel->SetRelativeTransform(ModelToCar);
	CarModel->RegisterComponent();
	USceneComponent* WheelFrame = NewObject<USceneComponent>(this, TEXT("WheelFrame"));
	WheelFrame->SetupAttachment(CarRoot);
	WheelFrame->SetRelativeTransform(ModelToCar);
	WheelFrame->RegisterComponent();

	auto AddPart = [this](UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Location)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
		Part->SetupAttachment(Parent);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocation(Location);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetReceivesDecals(false);
		Part->bAffectDistanceFieldLighting = false;
		Part->RegisterComponent();
		return Part;
	};

	// Разложить части: руль, колёса (по углам), остальное — кузов.
	TArray<int32> SteeringParts;
	TArray<int32> WheelParts[4];
	TArray<FBox> HeadlightBoxes, TailBoxes, FrontSeats, RearSeats;
	FBox SteeringBox(ForceInit);
	for (int32 i = 0; i < CarParts.Num(); ++i)
	{
		UStaticMesh* Mesh = CarParts[i];
		if (!Mesh)
		{
			continue;
		}
		const FString Name = Mesh->GetName();
		const FBox CarBox = PartBoxes[i].TransformBy(ModelToCar);
		if (Has(Name, { TEXT("steeringwheel") }))
		{
			SteeringParts.Add(i);
			SteeringBox += PartBoxes[i];
			continue;
		}
		if (Has(Name, { TEXT("wheel"), TEXT("tire"), TEXT("tyre"), TEXT("rim") }) && !Has(Name, { TEXT("steering") }))
		{
			const FVector C = CarBox.GetCenter();
			WheelParts[(C.X > 0.f ? 0 : 2) + (C.Y < 0.f ? 0 : 1)].Add(i);
			continue;
		}
		UStaticMeshComponent* Part = AddPart(Mesh, CarModel, FVector::ZeroVector);
		if (Has(Name, { TEXT("headlight") }))
		{
			HeadlightBoxes.Add(CarBox);
			Part->SetCastShadow(false); // свет фары утоплен внутрь — стекло не должно его заслонять
			for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
			{
				Part->SetMaterial(Slot, UDarcAssetSettings::MakeColorMaterial(Part, FLinearColor(1.f, 0.93f, 0.8f), 20.f));
			}
		}
		else if (Has(Name, { TEXT("taillight"), TEXT("rearlight") }))
		{
			TailBoxes.Add(CarBox);
			for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
			{
				if (UMaterialInstanceDynamic* Red = Cast<UMaterialInstanceDynamic>(
					UDarcAssetSettings::MakeColorMaterial(Part, FLinearColor(1.f, 0.03f, 0.02f), 6.f)))
				{
					Part->SetMaterial(Slot, Red);
					BrakeLights.Add(Red);
				}
			}
		}
		if (Has(Name, { TEXT("seatfl"), TEXT("seatfr") }))
		{
			FrontSeats.Add(CarBox);
		}
		else if (Has(Name, { TEXT("rearseat") }))
		{
			RearSeats.Add(CarBox);
		}
	}
	if (!ModelBox.IsValid)
	{
		// Модели нет — серая коробка, чтобы было видно, где машина.
		UStaticMeshComponent* Box = AddPart(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")), BodyPivot, FVector(0.f, 0.f, 85.f));
		Box->SetRelativeScale3D(FVector(5.f, 2.f, 1.7f));
	}

	// Руль: вращение вокруг оси колонки (наклонена к панели).
	if (SteeringParts.Num() > 0)
	{
		const FVector Center = SteeringBox.GetCenter();
		const FQuat Tilt = FRotator(SteeringTilt, -CarYaw, 0.f).Quaternion();
		USceneComponent* Pivot = NewObject<USceneComponent>(this);
		Pivot->SetupAttachment(CarModel);
		Pivot->SetRelativeLocationAndRotation(Center, Tilt);
		Pivot->RegisterComponent();
		SteeringFrame = Pivot;
		// Радиус обода (в единицах модели): руль — диск, поперечный размер ≈ диаметр.
		SteeringRadius = FMath::Max(SteeringBox.GetExtent().X, SteeringBox.GetExtent().Y) * 0.9f;
		SteeringSpin = NewObject<USceneComponent>(this);
		SteeringSpin->SetupAttachment(Pivot);
		SteeringSpin->RegisterComponent();
		USceneComponent* Untilt = NewObject<USceneComponent>(this);
		Untilt->SetupAttachment(SteeringSpin);
		Untilt->SetRelativeRotation(Tilt.Inverse());
		Untilt->RegisterComponent();
		for (int32 i : SteeringParts)
		{
			AddPart(CarParts[i], Untilt, -Center);
		}
	}

	// Колёса: у каждого угла — поворот (только передние) и вращение вокруг своей оси.
	const FQuat Frame = FRotator(0.f, -CarYaw, 0.f).Quaternion();
	FVector2D BaseSum = FVector2D::ZeroVector;
	int32 BaseCount = 0;
	float RadiusSum = 0.f;
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		if (WheelParts[Corner].Num() == 0)
		{
			continue;
		}
		FBox Box(ForceInit);
		for (int32 i : WheelParts[Corner])
		{
			Box += PartBoxes[i];
		}
		const FVector Center = Box.GetCenter();
		const FBox CarBox = Box.TransformBy(ModelToCar);
		BaseSum += FVector2D(FMath::Abs(CarBox.GetCenter().X), FMath::Abs(CarBox.GetCenter().Y));
		RadiusSum += CarBox.GetExtent().Z;
		++BaseCount;

		USceneComponent* Pivot = NewObject<USceneComponent>(this);
		Pivot->SetupAttachment(WheelFrame);
		Pivot->SetRelativeLocationAndRotation(Center, Frame);
		Pivot->RegisterComponent();
		USceneComponent* Steer = NewObject<USceneComponent>(this);
		Steer->SetupAttachment(Pivot);
		Steer->RegisterComponent();
		USceneComponent* Spin = NewObject<USceneComponent>(this);
		Spin->SetupAttachment(Steer);
		Spin->RegisterComponent();
		USceneComponent* Unframe = NewObject<USceneComponent>(this);
		Unframe->SetupAttachment(Spin);
		Unframe->SetRelativeRotation(Frame.Inverse());
		Unframe->RegisterComponent();
		for (int32 i : WheelParts[Corner])
		{
			AddPart(CarParts[i], Unframe, -Center);
		}
		WheelSteer.Add(Steer);
		WheelSpin.Add(Spin);
		WheelIsFront.Add(Corner < 2);
		WheelPivots.Add(Pivot);
		WheelRest.Add(Center);
		WheelCarPos.Add(CarBox.GetCenter());
		WheelOffsets.Add(0.f);
	}
	if (BaseCount > 0)
	{
		WheelBase = BaseSum / BaseCount;
		WheelRadius = FMath::Max(RadiusSum / BaseCount, 10.f);
	}
	else
	{
		WheelBase = FVector2D(HalfLength * 0.6f, HalfWidth * 0.8f);
	}

	// Места: из сидений модели (водитель — со стороны руля), иначе — типовые для внедорожника.
	const float DriverSide = SteeringParts.Num() > 0 && SteeringBox.TransformBy(ModelToCar).GetCenter().Y > 0.f ? 1.f : -1.f;
	TArray<FVector> Cushions;
	auto Cushion = [](const FBox& Box, float Y)
	{
		return FVector(Box.GetCenter().X + Box.GetExtent().X * 0.15f, Y, Box.Min.Z + Box.GetSize().Z * 0.33f);
	};
	if (SeatOffsets.Num() > 0)
	{
		Cushions = SeatOffsets;
	}
	else
	{
		FrontSeats.Sort([DriverSide](const FBox& A, const FBox& B) { return A.GetCenter().Y * DriverSide > B.GetCenter().Y * DriverSide; });
		for (const FBox& Box : FrontSeats)
		{
			Cushions.Add(Cushion(Box, Box.GetCenter().Y));
		}
		if (RearSeats.Num() > 0)
		{
			const FBox Rear = Union(RearSeats);
			const float W = Rear.GetExtent().Y * 0.62f;
			Cushions.Add(Cushion(Rear, Rear.GetCenter().Y + W * DriverSide));
			Cushions.Add(Cushion(Rear, Rear.GetCenter().Y - W * DriverSide));
			Cushions.Add(Cushion(Rear, Rear.GetCenter().Y));
		}
		const float SeatZ = Height * 0.33f;
		const TArray<FVector> Defaults = {
			FVector(HalfLength * 0.05f, 40.f * DriverSide, SeatZ), FVector(HalfLength * 0.05f, -40.f * DriverSide, SeatZ),
			FVector(-HalfLength * 0.3f, 42.f * DriverSide, SeatZ), FVector(-HalfLength * 0.3f, -42.f * DriverSide, SeatZ),
			FVector(-HalfLength * 0.3f, 0.f, SeatZ) };
		for (int32 i = Cushions.Num(); i < MaxSeats; ++i)
		{
			Cushions.Add(Defaults[i]);
		}
	}
	for (const FVector& C : Cushions)
	{
		AutoSeats.Add(C + FVector(0.f, 0.f, SeatedLift) + SeatAdjust + (AutoSeats.Num() == 0 ? DriverAdjust : FVector::ZeroVector));
	}

	// Фары: два прожектора вперёд (тень — только от левого, ради FPS), свет в салоне.
	FBox Lamps = Union(HeadlightBoxes);
	if (!Lamps.IsValid)
	{
		Lamps = FBox(FVector(HalfLength - 30.f, -HalfWidth * 0.7f, 65.f), FVector(HalfLength - 10.f, HalfWidth * 0.7f, 80.f));
	}
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		USpotLightComponent* Spot = NewObject<USpotLightComponent>(this);
		Spot->SetupAttachment(BodyPivot);
		Spot->SetRelativeLocationAndRotation(FVector(Lamps.Max.X - HeadlightInset, Lamps.GetCenter().Y + Side * Lamps.GetExtent().Y * 0.75f, Lamps.GetCenter().Z),
			FRotator(-6.f, Side * 2.f, 0.f));
		Spot->SetIntensityUnits(ELightUnits::Lumens);
		Spot->SetIntensity(HeadlightLumens);
		Spot->SetAttenuationRadius(HeadlightRange);
		Spot->SetInnerConeAngle(14.f);
		Spot->SetOuterConeAngle(30.f);
		// Холодный белый (светодиодные фары): тёплый жёлтый в ночной цветокоррекции давал зелёный туман.
		Spot->SetLightColor(FLinearColor(0.9f, 0.95f, 1.f));
		Spot->SetCastShadows(Side < 0);
		Spot->SetVolumetricScatteringIntensity(0.12f); // в тумане не «стена света»
		Spot->RegisterComponent();
	}
	// Задние фонари слегка подсвечивают красным землю и всё позади машины.
	const FBox Tail = Union(TailBoxes).IsValid ? Union(TailBoxes)
		: FBox(FVector(-HalfLength, -HalfWidth * 0.8f, 85.f), FVector(-HalfLength + 10.f, HalfWidth * 0.8f, 100.f));
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		UPointLightComponent* Red = NewObject<UPointLightComponent>(this);
		Red->SetupAttachment(BodyPivot);
		Red->SetRelativeLocation(FVector(Tail.Min.X - 15.f, Tail.GetCenter().Y + Side * Tail.GetExtent().Y * 0.8f, Tail.GetCenter().Z));
		Red->SetIntensityUnits(ELightUnits::Lumens);
		Red->SetIntensity(TailLightLumens);
		Red->SetAttenuationRadius(320.f);
		Red->SetLightColor(FLinearColor(1.f, 0.05f, 0.03f));
		Red->SetCastShadows(false);
		Red->RegisterComponent();
	}

	UPointLightComponent* Cabin = NewObject<UPointLightComponent>(this);
	Cabin->SetupAttachment(BodyPivot);
	Cabin->SetRelativeLocation(FVector(AutoSeats.Num() > 0 ? AutoSeats[0].X + 70.f : 60.f, 0.f, Height * 0.55f));
	Cabin->SetIntensityUnits(ELightUnits::Lumens);
	Cabin->SetIntensity(12.f);
	Cabin->SetAttenuationRadius(170.f);
	Cabin->SetLightColor(FLinearColor(0.55f, 0.7f, 1.f));
	Cabin->SetCastShadows(false);
	Cabin->RegisterComponent();

	// Спидометр: число поверх экрана приборки (за рулём, чуть выше и дальше от водителя).
	if (SteeringParts.Num() > 0)
	{
		Speedo = NewObject<UTextRenderComponent>(this);
		Speedo->SetupAttachment(BodyPivot);
		Speedo->SetRelativeLocationAndRotation(SteeringBox.TransformBy(ModelToCar).GetCenter() + FVector(20.f, 0.f, 10.f) + SpeedoAdjust, SpeedoRotation);
		Speedo->SetHorizontalAlignment(EHTA_Center);
		Speedo->SetVerticalAlignment(EVRTA_TextCenter);
		Speedo->SetWorldSize(SpeedoSize);
		Speedo->SetTextRenderColor(FColor(215, 220, 230));
		if (UMaterialInterface* Unlit = UDarcAssetSettings::GetUnlitTextMaterial())
		{
			Speedo->SetTextMaterial(Unlit);
		}
		Speedo->SetText(FText::AsNumber(0));
		Speedo->SetCastShadow(false);
		Speedo->RegisterComponent();
	}

	// Простая коробка-коллизия: после остановки в машину нельзя зайти насквозь.
	UBoxComponent* Blocker = NewObject<UBoxComponent>(this, TEXT("CarBlocker"));
	Blocker->SetupAttachment(BodyPivot);
	Blocker->SetBoxExtent(FVector(HalfLength * 0.95f, HalfWidth * 0.9f, Height * 0.45f));
	Blocker->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f));
	Blocker->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Blocker->SetCollisionEnabled(bArrived ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Blocker->RegisterComponent();
}

void ADarcIntroDrive::BuildProfile()
{
	RouteLength = PathPoints.Num() >= 2 ? Route->GetSplineLength() : 0.f;
	const float A = FMath::Max(Acceleration, 10.f);
	const float B = FMath::Max(Braking, 10.f);
	// Короткий маршрут — не успеваем разогнаться до максимума.
	TopSpeed = FMath::Min(MaxSpeed, FMath::Sqrt(2.f * RouteLength * A * B / (A + B)));
	TopSpeed = FMath::Max(TopSpeed, 1.f);
	AccelTime = TopSpeed / A;
	BrakeTime = TopSpeed / B;
	const float Cruise = RouteLength - TopSpeed * TopSpeed / (2.f * A) - TopSpeed * TopSpeed / (2.f * B);
	CruiseTime = FMath::Max(Cruise, 0.f) / TopSpeed;
}

float ADarcIntroDrive::DistanceAt(float Time, float& OutSpeed, float& OutAccel) const
{
	const float A = TopSpeed / FMath::Max(AccelTime, KINDA_SMALL_NUMBER);
	const float B = TopSpeed / FMath::Max(BrakeTime, KINDA_SMALL_NUMBER);
	OutSpeed = 0.f;
	OutAccel = 0.f;
	if (Time <= 0.f)
	{
		return 0.f;
	}
	const float DA = 0.5f * A * AccelTime * AccelTime;
	if (Time < AccelTime)
	{
		OutSpeed = A * Time;
		OutAccel = A;
		return 0.5f * A * Time * Time;
	}
	if (Time < AccelTime + CruiseTime)
	{
		OutSpeed = TopSpeed;
		return DA + TopSpeed * (Time - AccelTime);
	}
	const float T = Time - AccelTime - CruiseTime;
	if (T < BrakeTime)
	{
		OutSpeed = TopSpeed - B * T;
		OutAccel = -B;
		return FMath::Min(DA + TopSpeed * CruiseTime + TopSpeed * T - 0.5f * B * T * T, RouteLength);
	}
	return RouteLength;
}

void ADarcIntroDrive::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (RouteLength <= 0.f)
	{
		return;
	}
	UpdateCar(DeltaSeconds);
	UpdateParkedSound();

	// Сервер: доехали и постояли — все выходят.
	if (HasAuthority() && RideStartTime > 0.f && !bArrived
		&& ServerTime() - RideStartTime >= AccelTime + CruiseTime + BrakeTime + ExitDelay)
	{
		bArrived = true;
		OnRep_Arrived();
	}
}

void ADarcIntroDrive::UpdateCar(float DeltaSeconds)
{
	if (RouteLength <= 0.f)
	{
		return;
	}
	float Speed = 0.f, Accel = 0.f;
	const float Time = RideStartTime > 0.f ? ServerTime() - RideStartTime : 0.f;
	const float Distance = DistanceAt(Time, Speed, Accel);

	const FVector Point = Route->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	// Курс — по хорде (точка сзади → точка спереди), а не по касательной: на изломах маршрута
	// касательная прыгала, и машина с рулём дёргались (особенно в последнем повороте налево).
	auto YawAt = [this](float D)
	{
		const FVector Back = Route->GetLocationAtDistanceAlongSpline(FMath::Clamp(D - YawSmoothing, 0.f, RouteLength), ESplineCoordinateSpace::World);
		const FVector Front = Route->GetLocationAtDistanceAlongSpline(FMath::Clamp(D + YawSmoothing, 0.f, RouteLength), ESplineCoordinateSpace::World);
		const FVector Chord = (Front - Back).GetSafeNormal2D();
		return Chord.IsNearlyZero()
			? Route->GetDirectionAtDistanceAlongSpline(D, ESplineCoordinateSpace::World).Rotation().Yaw
			: Chord.Rotation().Yaw;
	};
	const float Yaw = YawAt(Distance);
	// Поворот впереди — среднее по нескольким точкам: руль начинает крутить заранее и плавно.
	float Turn = 0.f;
	for (const float Ahead : { 150.f, 300.f, 450.f })
	{
		Turn += FMath::FindDeltaAngleDegrees(Yaw, YawAt(FMath::Min(Distance + Ahead, RouteLength))) / 3.f;
	}

	// Земля под четырьмя колёсами → высота, наклон и крен, которые догоняет «подвеска».
	const FQuat Heading = FRotator(0.f, Yaw, 0.f).Quaternion();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcIntroDrive), false, this);
	auto Ground = [&](float X, float Y)
	{
		const FVector At = Point + Heading.RotateVector(FVector(X, Y, 0.f));
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByObjectType(Hit, At + FVector(0.f, 0.f, 600.f), At - FVector(0.f, 0.f, 1500.f),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			return Hit.ImpactPoint.Z;
		}
		return Point.Z;
	};
	const float FL = Ground(WheelBase.X, -WheelBase.Y), FR = Ground(WheelBase.X, WheelBase.Y);
	const float RL = Ground(-WheelBase.X, -WheelBase.Y), RR = Ground(-WheelBase.X, WheelBase.Y);
	float TargetZ = (FL + FR + RL + RR) * 0.25f;
	float TargetPitch = FMath::RadiansToDegrees(FMath::Atan2((FL + FR) - (RL + RR), 4.f * WheelBase.X));
	// Крен в UE: положительный — правый борт вниз. Правая земля выше — крен отрицательный.
	const float TargetRoll = FMath::RadiansToDegrees(FMath::Atan2((FL + RL) - (FR + RR), 4.f * WheelBase.Y));
	if (Speed > 30.f)
	{
		// Мелкие неровности гравия.
		TargetZ += FMath::PerlinNoise1D(Distance * 0.01f) * RoadBumps;
		TargetPitch += FMath::PerlinNoise1D(Distance * 0.013f + 17.f) * 0.35f;
	}
	if (!bCarPlaced)
	{
		SuspZ = TargetZ; SuspPitch = TargetPitch; SuspRoll = TargetRoll;
		LastYaw = Yaw;
		bCarPlaced = true;
	}
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	const float K = SpringStiffness, C = SpringDamping;
	DarcIntroDrive::Spring(SuspZ, SuspZVel, TargetZ, K, C, Dt);
	DarcIntroDrive::Spring(SuspPitch, SuspPitchVel, TargetPitch, K, C, Dt);
	DarcIntroDrive::Spring(SuspRoll, SuspRollVel, TargetRoll, K, C, Dt);
	CarRoot->SetWorldLocationAndRotation(FVector(Point.X, Point.Y, SuspZ), FRotator(SuspPitch, Yaw, SuspRoll));

	// Кузов: приседает при разгоне, клюёт при торможении, кренится наружу в повороте.
	const float Curvature = FMath::DegreesToRadians(Turn) / 300.f;
	const float Lateral = Speed * Speed * Curvature / 100.f; // м/с²
	LeanPitch = FMath::FInterpTo(LeanPitch, FMath::Clamp(Accel / 100.f * BodyLean, -3.f, 3.f), Dt, 3.f);
	LeanRoll = FMath::FInterpTo(LeanRoll, FMath::Clamp(-Lateral * BodyLean, -3.f, 3.f), Dt, 3.f); // наружу поворота
	BodyPivot->SetRelativeRotation(FRotator(LeanPitch, 0.f, LeanRoll));

	// Каждое колесо дотягивается до своей земли (кузов — средняя плоскость, колёса — ход подвески),
	// чтобы на неровном месте все четыре стояли на земле.
	const FTransform CarTransform = CarRoot->GetComponentTransform();
	for (int32 i = 0; i < WheelPivots.Num(); ++i)
	{
		const FVector Contact = CarTransform.TransformPosition(FVector(WheelCarPos[i].X, WheelCarPos[i].Y, 0.f));
		FHitResult Hit;
		float Target = 0.f;
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Contact + FVector(0.f, 0.f, 150.f), Contact - FVector(0.f, 0.f, 150.f),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Target = FMath::Clamp(Hit.ImpactPoint.Z - Contact.Z, -WheelTravel, WheelTravel);
		}
		WheelOffsets[i] = bCarPlaced && Dt > 0.f ? FMath::FInterpTo(WheelOffsets[i], Target, Dt, 18.f) : Target;
		WheelPivots[i]->SetRelativeLocation(WheelRest[i] + FVector(0.f, 0.f, WheelOffsets[i] / FMath::Max(CarScale, 0.01f)));
	}

	// Руль и колёса.
	const float SteerTarget = Speed > 5.f ? FMath::Clamp(Turn * 1.4f, -32.f, 32.f) : SteerAngle;
	// Мягко и не быстрее, чем человек крутит руль: иначе кисти не успевали за ободом.
	const float MaxSteerStep = MaxWheelTurnRate / FMath::Max(SteeringRatio, 1.f) * Dt;
	SteerAngle += FMath::Clamp(FMath::FInterpTo(SteerAngle, SteerTarget, Dt, 1.8f) - SteerAngle, -MaxSteerStep, MaxSteerStep);
	WheelAngle = FMath::Fmod(WheelAngle + Speed * Dt / WheelRadius, 2.f * PI);
	for (int32 i = 0; i < WheelSpin.Num(); ++i)
	{
		WheelSpin[i]->SetRelativeRotation(FQuat(FVector::YAxisVector, -WheelAngle));
		if (WheelIsFront[i])
		{
			WheelSteer[i]->SetRelativeRotation(FRotator(0.f, SteerAngle, 0.f));
		}
	}
	if (SteeringSpin)
	{
		SteeringSpin->SetRelativeRotation(FQuat(FVector::XAxisVector, FMath::DegreesToRadians(-SteerAngle * SteeringRatio)));
	}

	// Стоп-сигналы и звук мотора.
	const bool bBraking = Accel < -1.f || (RideStartTime > 0.f && Speed <= 0.f && !bArrived);
	for (UMaterialInstanceDynamic* Light : BrakeLights)
	{
		Light->SetScalarParameterValue(TEXT("Intensity"), bBraking ? 16.f : 6.f);
	}
	const int32 Kmh = FMath::RoundToInt(Speed * 0.036f);
	if (Speedo && Kmh != LastKmh)
	{
		LastKmh = Kmh;
		Speedo->SetText(FText::AsNumber(Kmh));
	}
	if (EngineAudio)
	{
		// Запись салона живая — тон меняем слегка, иначе «плывёт».
		EngineAudio->SetPitchMultiplier(0.9f + 0.25f * Speed / FMath::Max(MaxSpeed, 1.f));
	}

	// Свой игрок в машине: взгляд поворачивает вместе с машиной, голову — не дальше предела.
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(LastYaw, Yaw);
	LastYaw = Yaw;
	for (const TWeakObjectPtr<APawn>& Weak : SeatedLocally)
	{
		APawn* Pawn = Weak.Get();
		APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		if (PC && PC->IsLocalController())
		{
			FRotator View = PC->GetControlRotation();
			View.Yaw += DeltaYaw;
			const float Relative = FMath::Clamp(FMath::FindDeltaAngleDegrees(Yaw, View.Yaw), -LookYawLimit, LookYawLimit);
			View.Yaw = Yaw + Relative;
			PC->SetControlRotation(View);
		}
	}
}

FVector ADarcIntroDrive::GetHandGrip(float Side) const
{
	if (!SteeringFrame)
	{
		return BodyPivot->GetComponentLocation();
	}
	// Точка на ободе (в рамке руля: X — ось колонки, Y — вправо, Z — вверх), повёрнутая вместе
	// с рулём, но не дальше предела — иначе руки перекручивались бы.
	const float Scale = FMath::Max(CarScale, 0.01f);
	const float A = FMath::DegreesToRadians(HandGripAngle);
	const FVector Rim(0.f, Side * SteeringRadius * FMath::Cos(A), SteeringRadius * FMath::Sin(A));
	const float Spin = FMath::Clamp(-SteerAngle * SteeringRatio, -HandFollowLimit, HandFollowLimit);
	const FVector Turned = FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Spin)).RotateVector(Rim);
	const FVector Adjust(HandGripAdjust.X, Side * HandGripAdjust.Y, HandGripAdjust.Z);
	return SteeringFrame->GetComponentTransform().TransformPosition(Turned + Adjust / Scale);
}

FVector ADarcIntroDrive::GetElbowHint(float Side, const FVector& Shoulder, const FVector& Grip) const
{
	// Локти вниз и чуть в стороны, как у человека за рулём.
	const FTransform Frame = BodyPivot->GetComponentTransform();
	return (Shoulder + Grip) * 0.5f - Frame.GetUnitAxis(EAxis::Z) * 35.f + Frame.GetUnitAxis(EAxis::Y) * Side * 20.f;
}

FVector ADarcIntroDrive::SeatLocal(int32 Index) const
{
	return AutoSeats.IsValidIndex(Index) ? AutoSeats[Index] : FVector(0.f, 0.f, 100.f);
}

FVector ADarcIntroDrive::ExitLocation(int32 Index) const
{
	// Выходим в свою дверь: сбоку от места, за габаритом кузова; средний сзади — правее и дальше назад.
	const FVector Seat = SeatLocal(Index);
	const bool bMiddle = FMath::Abs(Seat.Y) < 15.f;
	const float Side = (bMiddle || Seat.Y > 0.f) ? 1.f : -1.f;
	const FVector Local(Seat.X - (bMiddle ? 90.f : 0.f), Side * (HalfWidth + 75.f), 0.f);
	const FVector At = CarRoot->GetComponentTransform().TransformPosition(Local);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcIntroExit), false, this);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, At + FVector(0.f, 0.f, 400.f), At - FVector(0.f, 0.f, 1500.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint + FVector(0.f, 0.f, 100.f);
	}
	return At + FVector(0.f, 0.f, 100.f);
}

void ADarcIntroDrive::ServerStartRide()
{
	RideStartTime = ServerTime();
	RadioIndex = FMath::RandRange(1, MAX_int32 - 1); // сид программы эфира — одинаковый у всех машин
	ServerSeatNewcomers();
	OnRep_RideStart();
	// Опоздавшие (клиент подключился во время поездки) — тоже в машину.
	GetWorldTimerManager().SetTimer(SeatTimer, this, &ADarcIntroDrive::ServerSeatNewcomers, 0.5f, true);
}

void ADarcIntroDrive::ServerSeatNewcomers()
{
	if (bArrived)
	{
		GetWorldTimerManager().ClearTimer(SeatTimer);
		return;
	}
	// Порядок контроллеров: первым идёт хост — он за рулём.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (!Cast<ACharacter>(Pawn) || Seats.Contains(Pawn) || Seats.Num() >= FMath::Min(DarcIntroDrive::MaxSeats, AutoSeats.Num()))
		{
			continue;
		}
		Seats.Add(Pawn);
		SeatLocally(Pawn, Seats.Num() - 1);
	}
}

void ADarcIntroDrive::OnRep_RideStart()
{
	StartRadio();
	if (!EngineAudio)
	{
		EngineAudio = UDarcAssetSettings::PlayLoopAttached(EngineSlot, BodyPivot);
		if (EngineAudio)
		{
			EngineAudio->SetVolumeMultiplier(0.45f);
		}
	}
}

void ADarcIntroDrive::StartRadio()
{
	if (RadioAudio)
	{
		return;
	}
	BuildRadioProgram();
	if (RadioProgram.Num() == 0)
	{
		return;
	}
	// Музыка звучит из машины: в салоне — полная громкость, снаружи затихает к ~25 м.
	static TStrongObjectPtr<USoundAttenuation> Attenuation;
	if (!Attenuation.IsValid())
	{
		Attenuation.Reset(NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("DarcCarRadioAttenuation")));
		FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
		Settings.bAttenuate = true;
		Settings.bSpatialize = true;
		Settings.AttenuationShape = EAttenuationShape::Sphere;
		Settings.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
		Settings.FalloffDistance = 2200.f;
		Settings.bAttenuateWithLPF = true;
		Settings.LPFRadiusMin = 400.f;
		Settings.LPFRadiusMax = 2000.f;
	}
	RadioAudio = UGameplayStatics::SpawnSoundAttached(RadioProgram[0], BodyPivot, NAME_None,
		FVector(40.f, 0.f, 100.f), EAttachLocation::KeepRelativeOffset, true, RadioVolume, 1.f, 0.f, Attenuation.Get(),
		nullptr, /*bAutoDestroy*/ false);
	if (RadioAudio)
	{
		RadioAudio->Stop();
		RadioAudio->OnAudioFinished.AddDynamic(this, &ADarcIntroDrive::OnRadioItemFinished);
		PlayRadioAt(FMath::Max(ServerTime() - RideStartTime, 0.f));
	}
}

void ADarcIntroDrive::BuildRadioProgram()
{
	auto Collect = [](FName Base, int32 Max)
	{
		TArray<USoundBase*> Found;
		for (int32 i = 0; i <= Max; ++i)
		{
			const FName Slot = i == 0 ? Base : FName(*FString::Printf(TEXT("%s_%d"), *Base.ToString(), i));
			if (USoundBase* Sound = UDarcAssetSettings::FindSound(Slot))
			{
				Found.Add(Sound);
			}
		}
		return Found;
	};
	TArray<USoundBase*> Tracks = Collect(RadioSlot, 31);
	const TArray<USoundBase*> Voices = Collect(RadioVoiceSlot, 31);
	RadioProgram.Reset();
	RadioProgramLength = 0.f;
	if (Tracks.Num() == 0)
	{
		return;
	}
	// Сид реплицируется — у всех игроков один и тот же порядок. Два круга треков (второй
	// перемешан заново), между треками иногда голос эфира, один и тот же голос не подряд.
	FRandomStream Random(RadioIndex);
	USoundBase* LastVoice = nullptr;
	for (int32 Round = 0; Round < 2; ++Round)
	{
		for (int32 i = Tracks.Num() - 1; i > 0; --i)
		{
			Tracks.Swap(i, Random.RandRange(0, i));
		}
		for (USoundBase* Track : Tracks)
		{
			if (Voices.Num() > 0 && Random.FRand() < RadioVoiceChance)
			{
				USoundBase* Voice = Voices[Random.RandRange(0, Voices.Num() - 1)];
				if (Voice == LastVoice && Voices.Num() > 1)
				{
					Voice = Voices[(Voices.IndexOfByKey(Voice) + 1) % Voices.Num()];
				}
				LastVoice = Voice;
				RadioProgram.Add(Voice);
			}
			RadioProgram.Add(Track);
		}
	}
	for (USoundBase* Item : RadioProgram)
	{
		RadioProgramLength += FMath::Max(Item->GetDuration(), 1.f);
	}
}

void ADarcIntroDrive::PlayRadioAt(float Time)
{
	if (!RadioAudio || RadioProgram.Num() == 0 || RadioProgramLength <= 0.f || bRadioOff)
	{
		return;
	}
	// Программа закольцована: найти пункт и смещение внутри него по общему времени.
	float T = FMath::Fmod(Time, RadioProgramLength);
	for (USoundBase* Item : RadioProgram)
	{
		const float Duration = FMath::Max(Item->GetDuration(), 1.f);
		if (T < Duration)
		{
			RadioAudio->SetSound(Item);
			RadioAudio->Play(FMath::Max(T, 0.f));
			return;
		}
		T -= Duration;
	}
}

void ADarcIntroDrive::OnRadioItemFinished()
{
	// Следующий пункт — по серверному времени (а не «следующий по списку»): игроки не расходятся.
	PlayRadioAt(FMath::Max(ServerTime() - RideStartTime, 0.f) + 0.05f);
}

void ADarcIntroDrive::UpdateParkedSound()
{
	if (!bParkedSound || bRadioOff)
	{
		return;
	}
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (Pawn && FVector::Dist(Pawn->GetActorLocation(), CarRoot->GetComponentLocation()) > RadioOffDistance)
	{
		bRadioOff = true;
		for (UAudioComponent* Audio : { RadioAudio.Get(), EngineAudio.Get() })
		{
			if (Audio)
			{
				Audio->FadeOut(2.5f, 0.f);
			}
		}
	}
}

void ADarcIntroDrive::OnRep_Seats()
{
	for (int32 i = 0; i < Seats.Num(); ++i)
	{
		if (Seats[i] && !SeatedLocally.Contains(Seats[i].Get()) && !bArrived)
		{
			SeatLocally(Seats[i], i);
		}
	}
}

void ADarcIntroDrive::SeatLocally(APawn* Pawn, int32 Index)
{
	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!Character)
	{
		return;
	}
	SeatedLocally.Add(Pawn);
	SavedUseYaw.Add(Pawn, Character->bUseControllerRotationYaw);
	Character->bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Character->AttachToComponent(BodyPivot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Character->SetActorRelativeLocation(SeatLocal(Index));
	Character->SetActorRelativeRotation(FRotator::ZeroRotator);

	// Поза: водитель держит руль, остальные сидят (разные варианты по местам).
	UAnimationAsset* Anim = nullptr;
	if (Index == 0)
	{
		Anim = UDarcAssetSettings::FindAnimation(DriverAnimation);
	}
	else
	{
		TArray<UAnimationAsset*> Variants;
		for (int32 i = 0; i <= 8; ++i)
		{
			const FName Slot = i == 0 ? PassengerAnimation : FName(*FString::Printf(TEXT("%s_%d"), *PassengerAnimation.ToString(), i));
			if (UAnimationAsset* Found = UDarcAssetSettings::FindAnimation(Slot))
			{
				Variants.Add(Found);
			}
		}
		Anim = Variants.Num() > 0 ? Variants[(Index - 1) % Variants.Num()] : UDarcAssetSettings::FindAnimation(DriverAnimation);
	}
	if (Anim)
	{
		Character->GetMesh()->PlayAnimation(Anim, true);
		if (ADARKCharacter* Template = Cast<ADARKCharacter>(Character))
		{
			// Руки от первого лица — та же поза (камера на голове опускается вместе с ней).
			Template->GetFirstPersonMesh()->PlayAnimation(Anim, true);
		}
	}
	if (Index == 0)
	{
		// Кисти водителя — на обод руля (анимация из пака под другую машину).
		UDarcDriverHandsComponent* Hands = NewObject<UDarcDriverHandsComponent>(Character, TEXT("DriverHands"));
		Hands->RegisterComponent();
		Hands->Begin(this);
	}

	if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		if (PC->IsLocalController())
		{
			PC->SetIgnoreMoveInput(true);
			PC->SetControlRotation(FRotator(0.f, CarRoot->GetComponentRotation().Yaw, 0.f));
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 1.2f, FLinearColor::Black, false, false);
			}
		}
	}
}

void ADarcIntroDrive::OnRep_Arrived()
{
	if (!bArrived)
	{
		return;
	}
	BeginExit();
}

void ADarcIntroDrive::BeginExit()
{
	if (bLocalExitDone)
	{
		return;
	}
	// Короткое затемнение у своего игрока — за это время все выходят наружу.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->IsLocalController() && PC->PlayerCameraManager && PC->GetPawn() && SeatedLocally.Contains(PC->GetPawn()))
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.35f, FLinearColor::Black, false, true);
		}
	}
	UDarcAssetSettings::PlaySound(this, DoorSlot, CarRoot->GetComponentLocation(), 0.8f);
	GetWorldTimerManager().SetTimer(ExitTimer, this, &ADarcIntroDrive::FinishExit, 0.4f, false);
}

void ADarcIntroDrive::FinishExit()
{
	bLocalExitDone = true;
	GetWorldTimerManager().ClearTimer(SeatTimer);
	for (int32 i = 0; i < Seats.Num(); ++i)
	{
		if (Seats[i] && SeatedLocally.Contains(Seats[i].Get()))
		{
			UnseatLocally(Seats[i], i);
		}
	}
	SeatedLocally.Empty();
	// Двери закрыты, мотор не заглушен: музыка и холостые обороты — глухо, из салона.
	bParkedSound = true;
	if (EngineAudio)
	{
		EngineAudio->SetPitchMultiplier(0.75f);
		EngineAudio->SetVolumeMultiplier(0.3f);
	}
	for (UAudioComponent* Audio : { RadioAudio.Get(), EngineAudio.Get() })
	{
		if (Audio)
		{
			Audio->SetLowPassFilterEnabled(true);
			Audio->SetLowPassFilterFrequency(ParkedLowPassHz);
		}
	}
	if (RadioAudio)
	{
		RadioAudio->SetVolumeMultiplier(RadioVolume * 0.8f);
	}
	// Гул салона сменяется холостыми оборотами снаружи — тихо, без фильтра (это звук улицы).
	if (UAudioComponent* Idle = UDarcAssetSettings::PlayLoopAttached(IdleSlot, BodyPivot))
	{
		if (EngineAudio)
		{
			EngineAudio->FadeOut(1.f, 0.f);
		}
		Idle->SetVolumeMultiplier(IdleVolume);
		Idle->FadeIn(1.f, IdleVolume);
		EngineAudio = Idle; // дальше его, как и мотор, глушит уход далеко от машины
	}
	if (UBoxComponent* Blocker = FindComponentByClass<UBoxComponent>())
	{
		Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

void ADarcIntroDrive::UnseatLocally(APawn* Pawn, int32 Index)
{
	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!Character)
	{
		return;
	}
	if (UDarcDriverHandsComponent* Hands = Character->FindComponentByClass<UDarcDriverHandsComponent>())
	{
		Hands->End();
		Hands->DestroyComponent();
	}
	// Фонарь вкладывался в руку, пока игрок сидел, — переложить, когда поза рук станет обычной.
	FTimerHandle FlashlightTimer;
	GetWorldTimerManager().SetTimer(FlashlightTimer, FTimerDelegate::CreateWeakLambda(Character, [Character]()
	{
		if (UDarcFlashlightComponent* Flashlight = Character->FindComponentByClass<UDarcFlashlightComponent>())
		{
			Flashlight->ResetHandPlacement();
		}
	}), 0.3f, false);
	const FVector Exit = ExitLocation(Index);
	const float CarYawNow = CarRoot->GetComponentRotation().Yaw;
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->SetActorLocationAndRotation(Exit, FRotator(0.f, CarYawNow, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (const bool* UseYaw = SavedUseYaw.Find(Pawn))
	{
		Character->bUseControllerRotationYaw = *UseYaw;
	}
	Character->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	if (ADARKCharacter* Template = Cast<ADARKCharacter>(Character))
	{
		Template->GetFirstPersonMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	}
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		if (HasAuthority())
		{
			if (ADarcPlayerController* Darc = Cast<ADarcPlayerController>(PC))
			{
				Darc->SetSafeSpawnLocation(Exit);
			}
		}
		if (PC->IsLocalController())
		{
			PC->SetIgnoreMoveInput(false);
			PC->SetControlRotation(FRotator(0.f, CarYawNow, 0.f));
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.6f, FLinearColor::Black, false, false);
			}
		}
	}
}
