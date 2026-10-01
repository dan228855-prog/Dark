// DarcSliceBuilder.cpp
// Планировка (см, пол на Z=0, высота этажа 300). Здание — X 0..3400, Y -800..800:
//
//   Y 800 ┌────────┬──────────┬────────┬──────────┬─────────────┐
//         │        │ Рабочая  │ Тех.   │ Тех.     │             │
//         │ Лобби  │ комната  │ шкаф   │ помещ.   │  Серверная  │
//   Y 150 │        ├──[дв]────┴─[дв]───┴──[дв]────┤             │
//  вход ─▶[дв]   ─▶      К О Р И Д О Р   (щиток) [дв]  (заперта)  │
//   Y-150 │        ├──[дв]────┬─────────────────── ┤             │
//         │        │ Кладовая │   (закрыто)        │             │
//   Y-800 └────────┴──────────┴────────────────────┴─────────────┘
//         X 0      800       1600     2000       2600          3400
// Снаружи (X < 0): КПП с охранником, фонарь, спутниковая тарелка-ориентир, точка появления.

#include "DarcSliceBuilder.h"
#include "CarryableItem.h"
#include "DarcAssetSettings.h"
#include "DarcCardReader.h"
#include "DarcDataTransferStation.h"
#include "DarcFuseBox.h"
#include "DarcFuseItem.h"
#include "DarcGameplayLibrary.h"
#include "DarcGenerator.h"
#include "DarcHeavyObject.h"
#include "DarcItemSlot.h"
#include "DarcNpc.h"
#include "DarcPowerConsumerComponent.h"
#include "DarcPowerInlet.h"
#include "DarcPowerLamp.h"
#include "DarcRareEventAnchor.h"
#include "DarcRoomVolume.h"
#include "DarcSliceItems.h"
#include "DarcTerminal.h"
#include "InteractableDoor.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SphereComponent.h"
#include <type_traits>

namespace DarcSlice
{
	constexpr float WallHeight = 300.f;
	constexpr float WallThickness = 20.f;
	constexpr float DoorWidth = 200.f;
	constexpr float DoorHeight = 220.f;

	FText Txt(const TCHAR* Key) { return UDarcGameplayLibrary::UIText(Key); }

	FDarcVisualSpec Vis(FName Slot, const FVector& Size, FName Material = NAME_None, const FVector& Offset = FVector::ZeroVector)
	{
		FDarcVisualSpec Spec;
		Spec.Slot = Slot;
		Spec.Size = Size;
		Spec.Material = Material;
		Spec.Offset = Offset;
		return Spec;
	}

	FDarcNpcLine Line(const TCHAR* Key, float DelayBefore = 0.f, float Duration = 3.f)
	{
		FDarcNpcLine L;
		L.Text = Txt(Key);
		L.DelayBefore = DelayBefore;
		L.Duration = Duration;
		return L;
	}
}

ADarcSliceBuilder::ADarcSliceBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true; // геометрию должен построить каждый клиент
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

FTransform ADarcSliceBuilder::GetPlayerSpawn(int32 Index)
{
	// Игроки появляются на дороге к КПП, лицом к зданию, рядом друг с другом.
	return FTransform(FRotator::ZeroRotator, FVector(-2400.f, -200.f + 100.f * (Index % 5), 120.f));
}

void ADarcSliceBuilder::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Геометрия — сразу при появлении сборщика (и у сервера, и у клиента), до появления игроков на полу.
	if (GetWorld() && GetWorld()->IsGameWorld() && !bGeometryBuilt)
	{
		bGeometryBuilt = true;
		BuildEnvironment();
		BuildGeometry();
	}
}

void ADarcSliceBuilder::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		BuildGameplay();
	}
}

// ---------------------------------------------------------------------------
// Геометрия
// ---------------------------------------------------------------------------

void ADarcSliceBuilder::AddBox(const FVector& Min, const FVector& Max, FName MaterialSlot)
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;
	AStaticMeshActor* Box = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform((Min + Max) * 0.5f), Params);
	if (!Box)
	{
		return;
	}
	Box->SetReplicates(false); // строится у каждого сам
	UStaticMeshComponent* Mesh = Box->GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Mesh->SetWorldScale3D((Max - Min) / 100.f);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	UMaterialInterface* Material = UDarcAssetSettings::FindMaterial(MaterialSlot);
	Mesh->SetMaterial(0, Material ? Material : LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
}

void ADarcSliceBuilder::AddWall(const FVector2D& A, const FVector2D& B, const TArray<float>& DoorCenters)
{
	using namespace DarcSlice;
	const bool bAlongX = FMath::IsNearlyEqual(A.Y, B.Y);
	const float Start = bAlongX ? FMath::Min(A.X, B.X) : FMath::Min(A.Y, B.Y);
	const float End = bAlongX ? FMath::Max(A.X, B.X) : FMath::Max(A.Y, B.Y);
	const float Fixed = bAlongX ? A.Y : A.X;
	const float Half = WallThickness * 0.5f;

	auto Segment = [&](float S0, float S1, float Z0, float Z1)
	{
		if (S1 - S0 < 1.f)
		{
			return;
		}
		const FVector Min = bAlongX ? FVector(S0, Fixed - Half, Z0) : FVector(Fixed - Half, S0, Z0);
		const FVector Max = bAlongX ? FVector(S1, Fixed + Half, Z1) : FVector(Fixed + Half, S1, Z1);
		AddBox(Min, Max, TEXT("Wall"));
	};

	TArray<float> Doors = DoorCenters;
	Doors.Sort();
	float Cursor = Start;
	for (const float Center : Doors)
	{
		const float D0 = Center - DoorWidth * 0.5f;
		const float D1 = Center + DoorWidth * 0.5f;
		Segment(Cursor, D0, 0.f, WallHeight);
		Segment(D0, D1, DoorHeight, WallHeight); // перемычка над дверью
		Cursor = D1;
	}
	Segment(Cursor, End, 0.f, WallHeight);
}

void ADarcSliceBuilder::BuildEnvironment()
{
	// Вечер: низкое солнце, атмосфера, туман, небесный свет — всё подвижное (Lumen).
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;

	if (ADirectionalLight* Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0.f, 0.f, 2000.f), FRotator(-6.f, 35.f, 0.f), Params))
	{
		Sun->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			// Яркость в люксах (проект использует физические единицы, см. DefaultEngine.ini
			// r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True) — старое значение
			// 1.5 было на порядок темнее стандартного солнца шаблона (~10) и с низким углом
			// давало почти чёрный экран даже снаружи.
			SunLight->SetIntensity(20.f);
			SunLight->SetLightColor(FLinearColor(1.f, 0.62f, 0.4f));
			SunLight->SetAtmosphereSunLight(true);
		}
	}

	if (AActor* Sky = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params))
	{
		USkyAtmosphereComponent* Atmosphere = NewObject<USkyAtmosphereComponent>(Sky);
		Atmosphere->RegisterComponent();
		Sky->SetRootComponent(Atmosphere);
	}

	if (ASkyLight* SkyLight = GetWorld()->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		USkyLightComponent* Light = SkyLight->GetLightComponent();
		Light->SetMobility(EComponentMobility::Movable);
		Light->bRealTimeCapture = true;
		Light->SetIntensity(3.f);
		Light->RecaptureSky();
	}

	if (AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>(FVector(0.f, 0.f, -100.f), FRotator::ZeroRotator, Params))
	{
		Fog->GetComponent()->SetFogDensity(0.035f); // «лёгкий туман»
	}

	// Без своего PostProcessVolume экспозиция считается автоматически и может «зажать»
	// картинку темнее, чем задумано (особенно пока в кадре в основном серые коробки).
	// ВАЖНО: НЕ переключаем на AEM_Manual — в этом режиме яркость считается по
	// Aperture/ISO/ShutterSpeed камеры (как у настоящего фотоаппарата), а не по свету
	// в сцене; без ручной калибровки этих параметров экран гарантированно останется
	// тёмным, что и произошло при первой попытке. Вместо этого просто зажимаем
	// автоэкспозицию в узком ярком диапазоне (EV100) и чуть добавляем общую яркость —
	// движок по-прежнему сам считает экспозицию по сцене, но не может уйти в черноту.
	if (APostProcessVolume* ExposureVolume = GetWorld()->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		ExposureVolume->bUnbound = true;
		ExposureVolume->Settings.bOverride_AutoExposureMinBrightness = true;
		ExposureVolume->Settings.AutoExposureMinBrightness = 0.f;
		ExposureVolume->Settings.bOverride_AutoExposureMaxBrightness = true;
		ExposureVolume->Settings.AutoExposureMaxBrightness = 2.f;
		ExposureVolume->Settings.bOverride_AutoExposureBias = true;
		ExposureVolume->Settings.AutoExposureBias = 2.f;
	}
}

void ADarcSliceBuilder::AddRoom(const FVector2D& Min, const FVector2D& Max)
{
	// Пол комнаты (поверх земли) — отдельным материалом.
	AddBox(FVector(Min.X, Min.Y, 0.f), FVector(Max.X, Max.Y, 2.f), TEXT("Floor"));
}

void ADarcSliceBuilder::BuildGeometry()
{
	using namespace DarcSlice;

	// Земля вокруг (лес/КПП) и пол здания.
	AddBox(FVector(-3500.f, -2500.f, -20.f), FVector(4000.f, 2500.f, 0.f), TEXT("Ground"));
	AddRoom(FVector2D(0.f, -800.f), FVector2D(800.f, 800.f));      // лобби
	AddRoom(FVector2D(800.f, -150.f), FVector2D(2600.f, 150.f));   // коридор
	AddRoom(FVector2D(800.f, 150.f), FVector2D(2600.f, 800.f));    // северные комнаты
	AddRoom(FVector2D(800.f, -800.f), FVector2D(2600.f, -150.f));  // южные комнаты
	AddRoom(FVector2D(2600.f, -800.f), FVector2D(3400.f, 800.f));  // серверная

	// Внешние стены здания (вход — в западной стене по центру).
	AddWall(FVector2D(0.f, -800.f), FVector2D(0.f, 800.f), { 0.f });
	AddWall(FVector2D(3400.f, -800.f), FVector2D(3400.f, 800.f));
	AddWall(FVector2D(0.f, 800.f), FVector2D(3400.f, 800.f));
	AddWall(FVector2D(0.f, -800.f), FVector2D(3400.f, -800.f));

	// Лобби → коридор (проём без двери).
	AddWall(FVector2D(800.f, -800.f), FVector2D(800.f, 800.f), { 0.f });

	// Стены коридора с дверями в комнаты.
	AddWall(FVector2D(800.f, 150.f), FVector2D(2600.f, 150.f), { 1200.f, 1800.f, 2300.f });
	AddWall(FVector2D(800.f, -150.f), FVector2D(2600.f, -150.f), { 1200.f });

	// Перегородки между комнатами.
	AddWall(FVector2D(1600.f, 150.f), FVector2D(1600.f, 800.f));
	AddWall(FVector2D(2000.f, 150.f), FVector2D(2000.f, 800.f));
	AddWall(FVector2D(1600.f, -800.f), FVector2D(1600.f, -150.f));

	// Серверная (дверь по центру, заперта).
	AddWall(FVector2D(2600.f, -800.f), FVector2D(2600.f, 800.f), { 0.f });

	// Потолок — внутри должно быть темно без ламп.
	AddBox(FVector(0.f, -800.f, WallHeight), FVector(3400.f, 800.f, WallHeight + 20.f), TEXT("Ceiling"));

	// Мебель-коробки: стол в рабочей комнате, стеллажи, будка КПП, ориентир-тарелка.
	AddBox(FVector(1250.f, 600.f, 0.f), FVector(1550.f, 760.f, 75.f), TEXT("Furniture"));    // стол
	AddBox(FVector(2350.f, 650.f, 0.f), FVector(2580.f, 780.f, 90.f), TEXT("Furniture"));    // полка с модулем
	AddBox(FVector(1400.f, -780.f, 0.f), FVector(1580.f, -640.f, 90.f), TEXT("Furniture"));  // шкаф с запасным предохранителем
	AddBox(FVector(-1050.f, 250.f, 0.f), FVector(-750.f, 550.f, 260.f), TEXT("Booth"));      // будка КПП
	AddBox(FVector(-1700.f, -1700.f, 0.f), FVector(-1500.f, -1500.f, 900.f), TEXT("Furniture")); // опора тарелки

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;

	// Ориентир — спутниковая тарелка (локально у каждого).
	if (AStaticMeshActor* Dish = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FVector(-1600.f, -1600.f, 900.f)), Params))
	{
		Dish->SetReplicates(false);
		UDarcAssetSettings::ApplyVisual(Dish->GetStaticMeshComponent(), TEXT("SatelliteDish"), FVector(900.f, 900.f, 500.f));
	}

	// Табличка в кладовой с кодом серверной (цифры из сида выезда — у всех одинаковые).
	if (AActor* Sign = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FRotator(0.f, 90.f, 0.f), FVector(1000.f, -785.f, 160.f)), Params))
	{
		UDarcCodeTextComponent* CodeText = NewObject<UDarcCodeTextComponent>(Sign);
		CodeText->CodeKey = TEXT("ServerDoorCode");
		CodeText->SetWorldSize(30.f);
		CodeText->SetTextRenderColor(FColor(200, 40, 30));
		Sign->SetRootComponent(CodeText);
		CodeText->RegisterComponent();
	}

	// Кружка на столе — «мелочь класса C» и цель левитации. Своя у каждой машины (локальная физика).
	if (AStaticMeshActor* Mug = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FVector(1450.f, 680.f, 76.f)), Params))
	{
		Mug->SetReplicates(false);
		UStaticMeshComponent* MugMesh = Mug->GetStaticMeshComponent();
		MugMesh->SetMobility(EComponentMobility::Movable);
		UDarcAssetSettings::ApplyVisual(MugMesh, TEXT("Mug"), FVector(10.f, 10.f, 12.f));
		MugMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		MugMesh->SetSimulatePhysics(true);
		Mug->Tags.Add(TEXT("LevitationMug"));
	}

	// Точки появления игроков (до 5, кооп) — GetPlayerSpawn() существовал, но его никто не
	// вызывал, поэтому на карте не было ни одного PlayerStart и движок спавнил игрока в
	// (0,0,0) по умолчанию — мимо пола, отсюда ощущение «падения» в пустоту. Ставим сами
	// APlayerStart по тем же координатам, что и раньше планировалось.
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FTransform SpawnTransform = GetPlayerSpawn(Index);
		if (APlayerStart* Start = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), SpawnTransform, Params))
		{
			Start->SetReplicates(false);
			Start->PlayerStartTag = *FString::Printf(TEXT("DarcSliceStart%d"), Index);
		}
	}
}

// ---------------------------------------------------------------------------
// Игровые объекты (сервер)
// ---------------------------------------------------------------------------

template <class T>
T* ADarcSliceBuilder::SpawnDeferred(const FVector& Location, const FRotator& Rotation)
{
	return GetWorld()->SpawnActorDeferred<T>(T::StaticClass(), FTransform(Rotation, Location), this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
}

void ADarcSliceBuilder::Finish(AActor* Actor, const FVector& Location, const FRotator& Rotation)
{
	if (Actor)
	{
		Actor->FinishSpawning(FTransform(Rotation, Location));
	}
}

AInteractableDoor* ADarcSliceBuilder::SpawnDoor(const FVector2D& WallPoint, bool bWallAlongX, bool bLocked, FName MemoryId)
{
	using namespace DarcSlice;
	// Петля — у края проёма; створка уходит вдоль стены (локальная ось Y двери).
	const FVector Hinge = bWallAlongX
		? FVector(WallPoint.X - DoorWidth * 0.5f, WallPoint.Y, 0.f)
		: FVector(WallPoint.X, WallPoint.Y - DoorWidth * 0.5f, 0.f);
	const FRotator Rotation = bWallAlongX ? FRotator(0.f, -90.f, 0.f) : FRotator::ZeroRotator;

	AInteractableDoor* Door = SpawnDeferred<AInteractableDoor>(Hinge, Rotation);
	Door->bIsLocked = bLocked;
	Door->MemoryId = MemoryId;
	Door->PromptOpen = Txt(TEXT("Door_Open"));
	Door->PromptClose = Txt(TEXT("Door_Close"));
	Door->PromptLocked = Txt(TEXT("Door_Locked"));
	// Створка вписывается по центру петли и сдвигается вдоль стены на полширины.
	Door->VisualSpec = Vis(TEXT("Door"), FVector(8.f, DoorWidth - 4.f, DoorHeight - 4.f), TEXT("DoorMaterial"), FVector(0.f, DoorWidth * 0.5f, 0.f));
	Finish(Door, Hinge, Rotation);
	return Door;
}

ADarcPowerLamp* ADarcSliceBuilder::SpawnLamp(const FVector& Location, FName CircuitId)
{
	using namespace DarcSlice;
	ADarcPowerLamp* Lamp = SpawnDeferred<ADarcPowerLamp>(Location);
	Lamp->Power->CircuitId = CircuitId;
	Lamp->VisualSpec = Vis(TEXT("Lamp"), FVector(120.f, 25.f, 8.f));
	Finish(Lamp, Location);
	return Lamp;
}

void ADarcSliceBuilder::SpawnRoomVolume(FName RoomId, const FVector2D& Min, const FVector2D& Max)
{
	const FVector Center((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, 150.f);
	ADarcRoomVolume* Volume = SpawnDeferred<ADarcRoomVolume>(Center);
	Volume->RoomId = RoomId;
	Finish(Volume, Center);
	Volume->Bounds->SetBoxExtent(FVector((Max.X - Min.X) * 0.5f, (Max.Y - Min.Y) * 0.5f, 150.f));
}

void ADarcSliceBuilder::BuildGameplay()
{
	using namespace DarcSlice;

	// --- Комнаты для памяти мира и событий ---
	SpawnRoomVolume(TEXT("Outside"), FVector2D(-3500.f, -2500.f), FVector2D(-20.f, 2500.f));
	SpawnRoomVolume(TEXT("Lobby"), FVector2D(0.f, -800.f), FVector2D(800.f, 800.f));
	SpawnRoomVolume(TEXT("Corridor"), FVector2D(800.f, -150.f), FVector2D(2600.f, 150.f));
	SpawnRoomVolume(TEXT("WorkRoom"), FVector2D(800.f, 150.f), FVector2D(1600.f, 800.f));
	SpawnRoomVolume(TEXT("TechCabinetRoom"), FVector2D(1600.f, 150.f), FVector2D(2000.f, 800.f));
	SpawnRoomVolume(TEXT("TechRoom"), FVector2D(2000.f, 150.f), FVector2D(2600.f, 800.f));
	SpawnRoomVolume(TEXT("Storage"), FVector2D(800.f, -800.f), FVector2D(1600.f, -150.f));
	SpawnRoomVolume(TEXT("ServerRoom"), FVector2D(2600.f, -800.f), FVector2D(3400.f, 800.f));

	// --- Двери ---
	SpawnDoor(FVector2D(0.f, 0.f), false, false, TEXT("EntranceDoor"));
	SpawnDoor(FVector2D(1200.f, 150.f), true, false, TEXT("WorkRoomDoor"));
	AInteractableDoor* CabinetDoor = SpawnDoor(FVector2D(1800.f, 150.f), true, false, TEXT("TechCabinetDoor"));
	CabinetDoor->SetDoorOpen(true, nullptr); // «дверь была открыта»
	SpawnDoor(FVector2D(2300.f, 150.f), true, false, TEXT("TechRoomDoor"));
	SpawnDoor(FVector2D(1200.f, -150.f), true, false, TEXT("StorageDoor"));
	AInteractableDoor* ServerDoor = SpawnDoor(FVector2D(2600.f, 0.f), false, true, TEXT("ServerDoor"));

	// --- Электрика: щит в коридоре (три щитка), лампы ---
	auto MakeFuseBox = [this](const FVector& Location, FName Circuit, bool bHasFuse, const TArray<FName>& Collateral, bool bTaskOnBlow)
	{
		const FRotator FaceCorridor(0.f, 90.f, 0.f);
		ADarcFuseBox* Box = SpawnDeferred<ADarcFuseBox>(Location, FaceCorridor);
		Box->CircuitId = Circuit;
		Box->bHasFuse = bHasFuse;
		Box->bBreakerOn = true;
		Box->CollateralCircuits = Collateral;
		Box->PromptInsertFuse = Txt(TEXT("Fuse_Insert"));
		Box->PromptBreakerOn = Txt(TEXT("Breaker_On"));
		Box->PromptBreakerOff = Txt(TEXT("Breaker_Off"));
		if (bTaskOnBlow)
		{
			Box->bAddTaskOnBlow = true;
			Box->TaskOnBlow.TaskId = TEXT("RestoreCorridorLight");
			Box->TaskOnBlow.Title = Txt(TEXT("Task_RestoreCorridorLight"));
			Box->TaskOnBlow.bRequired = false;
		}
		Box->VisualSpec = Vis(TEXT("FuseBox"), FVector(50.f, 15.f, 70.f), NAME_None, FVector(0.f, 0.f, 90.f));
		Finish(Box, Location, FaceCorridor);
	};
	MakeFuseBox(FVector(1980.f, -140.f, 0.f), TEXT("Server"), false, { TEXT("Corridor") }, true);
	MakeFuseBox(FVector(2060.f, -140.f, 0.f), TEXT("Corridor"), true, {}, false);
	MakeFuseBox(FVector(2140.f, -140.f, 0.f), TEXT("Building"), true, {}, false);

	auto MakeItem = [this](auto* TypeTag, const FVector& Location, FName MeshSlot, const FVector& Size, const TCHAR* MemoryId)
	{
		using TItem = std::remove_pointer_t<decltype(TypeTag)>;
		TItem* Item = SpawnDeferred<TItem>(Location);
		Item->MemoryId = MemoryId;
		Item->PromptPickUp = Txt(TEXT("Carry_PickUp"));
		Item->PromptDrop = Txt(TEXT("Carry_Drop"));
		Item->VisualSpec = Vis(MeshSlot, Size);
		Finish(Item, Location); // физику свободного предмета включает он сам у каждой машины
		return Item;
	};
	MakeItem(static_cast<ADarcFuseItem*>(nullptr), FVector(2060.f, -100.f, 20.f), TEXT("Fuse"), FVector(12.f, 4.f, 4.f), TEXT("Fuse_Main"));
	MakeItem(static_cast<ADarcFuseItem*>(nullptr), FVector(1490.f, -710.f, 100.f), TEXT("Fuse"), FVector(12.f, 4.f, 4.f), TEXT("Fuse_Spare"));
	MakeItem(static_cast<ADarcInterfaceModule*>(nullptr), FVector(2460.f, 715.f, 100.f), TEXT("InterfaceModule"), FVector(25.f, 15.f, 8.f), TEXT("InterfaceModule"));

	SpawnLamp(FVector(400.f, 0.f, 290.f), TEXT("Building"));
	SpawnLamp(FVector(1200.f, 475.f, 290.f), TEXT("Building"));
	SpawnLamp(FVector(1800.f, 475.f, 290.f), TEXT("Building"));
	SpawnLamp(FVector(2300.f, 475.f, 290.f), TEXT("Building"));
	SpawnLamp(FVector(1200.f, -475.f, 290.f), TEXT("Building"));
	for (const float X : { 1000.f, 1500.f, 2000.f, 2450.f })
	{
		SpawnLamp(FVector(X, 0.f, 290.f), TEXT("Corridor"));
	}
	SpawnLamp(FVector(3000.f, 0.f, 290.f), TEXT("Server"));
	SpawnLamp(FVector(-700.f, -200.f, 380.f), TEXT("Building")); // фонарь у КПП

	// --- Рабочая комната: терминал каталога ---
	{
		const FVector Loc(1320.f, 700.f, 75.f);
		const FRotator Rot(0.f, -90.f, 0.f);
		ADarcTerminal* Catalog = SpawnDeferred<ADarcTerminal>(Loc, Rot);
		Catalog->CodeKey = TEXT("CatalogCode");
		Catalog->TaskIdOnUnlock = TEXT("OpenCatalog");
		Catalog->Power->CircuitId = TEXT("Building");
		Catalog->LockedTitle = Txt(TEXT("Screen_EnterCode"));
		Catalog->UnlockedTitle = Txt(TEXT("Screen_CatalogTitle"));
		Catalog->ScrambledWords = { Txt(TEXT("File_Corrupted_1")), Txt(TEXT("File_Corrupted_2")), Txt(TEXT("File_Corrupted_3")), Txt(TEXT("File_Corrupted_4")) };
		Catalog->ScreenLines = { Txt(TEXT("File_VerifyNote")), Txt(TEXT("File_MainStorage")) };
		Catalog->PromptUse = Txt(TEXT("Terminal_Use"));
		Catalog->PromptBusy = Txt(TEXT("Terminal_Busy"));
		Catalog->PromptNoPower = Txt(TEXT("Terminal_NoPower"));
		Catalog->VisualSpec = Vis(TEXT("Terminal"), FVector(50.f, 45.f, 45.f));
		Finish(Catalog, Loc, Rot);
	}

	// --- Кладовая: генератор и табличка с кодом серверной ---
	{
		const FVector Loc(1150.f, -500.f, 60.f);
		ADarcGenerator* Generator = SpawnDeferred<ADarcGenerator>(Loc);
		Generator->PromptGrab = Txt(TEXT("Heavy_Grab"));
		Generator->PromptRelease = Txt(TEXT("Heavy_Release"));
		Generator->PromptReattachCable = Txt(TEXT("Heavy_ReattachCable"));
		Generator->VisualSpec = Vis(TEXT("Generator"), FVector(110.f, 70.f, 90.f));
		Finish(Generator, Loc);
	}

	// --- Тех. шкаф: исчезающая комната ---
	{
		// Шкаф — тяжёлый объект: его можно сдвинуть вдвоём, а «комната» переставляет его сама.
		const FVector CabinetLoc(1800.f, 650.f, 100.f);
		ADarcHeavyObject* Cabinet = SpawnDeferred<ADarcHeavyObject>(CabinetLoc);
		Cabinet->bHasFragileCable = false;
		Cabinet->PromptGrab = Txt(TEXT("Heavy_Grab"));
		Cabinet->PromptRelease = Txt(TEXT("Heavy_Release"));
		Cabinet->VisualSpec = Vis(TEXT("Cabinet"), FVector(120.f, 60.f, 200.f));
		Finish(Cabinet, CabinetLoc);

		const FVector Loc(1800.f, 450.f, 0.f);
		ADarcRareEventAnchor* Anchor = SpawnDeferred<ADarcRareEventAnchor>(Loc);
		Anchor->SupportedEventTypes = { TEXT("VanishingRoom") };
		Anchor->RoomId = TEXT("TechCabinetRoom");
		Anchor->Behavior = EDarcAnchorBehavior::VanishingRoom;
		Anchor->RoomDoor = CabinetDoor;
		Anchor->TargetActor = Cabinet;
		Finish(Anchor, Loc);
	}

	// --- Тех. помещение: только модуль (уже на полке) ---

	// --- Якоря: динамик в коридоре, кружка, окно ---
	{
		const FVector Loc(1700.f, 130.f, 260.f);
		ADarcRareEventAnchor* Speaker = SpawnDeferred<ADarcRareEventAnchor>(Loc);
		Speaker->SupportedEventTypes = { TEXT("Speaker") };
		Speaker->RoomId = TEXT("Corridor");
		Speaker->VisualSpec = Vis(TEXT("Speaker"), FVector(30.f, 15.f, 30.f));
		Finish(Speaker, Loc);
	}
	{
		const FVector Loc(1400.f, 680.f, 0.f);
		ADarcRareEventAnchor* Levitation = SpawnDeferred<ADarcRareEventAnchor>(Loc);
		Levitation->SupportedEventTypes = { TEXT("Levitation") };
		Levitation->RoomId = TEXT("WorkRoom");
		Levitation->Behavior = EDarcAnchorBehavior::Levitate;
		Levitation->TargetTag = TEXT("LevitationMug");
		Levitation->EffectDuration = 25.f;
		Finish(Levitation, Loc);
	}
	{
		const FVector Loc(-30.f, 500.f, 240.f);
		ADarcRareEventAnchor* Window = SpawnDeferred<ADarcRareEventAnchor>(Loc);
		Window->SupportedEventTypes = { TEXT("WindowLight") };
		Window->Behavior = EDarcAnchorBehavior::LightOn;
		Window->VisualSpec = Vis(TEXT("Window"), FVector(4.f, 120.f, 90.f), TEXT("WindowGlass"));
		Finish(Window, Loc);
	}

	// --- Серверная: дверь (карта/код), станция, слоты, ввод генератора, стойка ---
	{
		const FVector ReaderLoc(2585.f, 160.f, 110.f);
		const FRotator FaceWest(0.f, 180.f, 0.f);
		ADarcCardReader* Reader = SpawnDeferred<ADarcCardReader>(ReaderLoc, FaceWest);
		Reader->AcceptedClass = ADarcKeycard::StaticClass();
		Reader->DoorToUnlock = ServerDoor;
		Reader->TaskIdOnAccess = TEXT("ServerDoorCard");
		Reader->PromptSwipe = Txt(TEXT("Card_Swipe"));
		Reader->VisualSpec = Vis(TEXT("CardReader"), FVector(8.f, 12.f, 18.f));
		Finish(Reader, ReaderLoc, FaceWest);

		const FVector KeypadLoc(2585.f, -160.f, 110.f);
		ADarcTerminal* Keypad = SpawnDeferred<ADarcTerminal>(KeypadLoc, FaceWest);
		Keypad->CodeKey = TEXT("ServerDoorCode");
		Keypad->DoorToUnlock = ServerDoor;
		Keypad->TaskIdOnUnlock = TEXT("ServerDoorKeypad");
		Keypad->LockedTitle = Txt(TEXT("Screen_EnterCode"));
		Keypad->UnlockedTitle = Txt(TEXT("Screen_AccessGranted"));
		Keypad->PromptUse = Txt(TEXT("Terminal_Use"));
		Keypad->PromptBusy = Txt(TEXT("Terminal_Busy"));
		Keypad->VisualSpec = Vis(TEXT("Keypad"), FVector(8.f, 15.f, 20.f));
		Finish(Keypad, KeypadLoc, FaceWest);
	}
	{
		const FRotator FaceWest(0.f, 180.f, 0.f);

		const FVector InterfaceLoc(3250.f, 140.f, 100.f);
		ADarcItemSlot* InterfaceSlot = SpawnDeferred<ADarcItemSlot>(InterfaceLoc, FaceWest);
		InterfaceSlot->AcceptedClass = ADarcInterfaceModule::StaticClass();
		InterfaceSlot->TaskIdOnInsert = TEXT("ConnectInterface");
		InterfaceSlot->PromptInsert = Txt(TEXT("Slot_Insert"));
		InterfaceSlot->PromptRemove = Txt(TEXT("Slot_Remove"));
		InterfaceSlot->VisualSpec = Vis(TEXT("Socket"), FVector(10.f, 30.f, 6.f));
		Finish(InterfaceSlot, InterfaceLoc, FaceWest);

		const FVector DriveLoc(3250.f, -140.f, 100.f);
		ADarcDataDrive* Drive = SpawnDeferred<ADarcDataDrive>(DriveLoc);
		Drive->MemoryId = TEXT("ArchiveDrive");
		Drive->PromptPickUp = Txt(TEXT("Carry_PickUp"));
		Drive->PromptDrop = Txt(TEXT("Carry_Drop"));
		Drive->VisualSpec = Vis(TEXT("Drive"), FVector(14.f, 9.f, 3.f));
		Finish(Drive, DriveLoc);

		ADarcItemSlot* DriveSlot = SpawnDeferred<ADarcItemSlot>(DriveLoc, FaceWest);
		DriveSlot->AcceptedClass = ADarcDataDrive::StaticClass();
		DriveSlot->InitialItem = Drive;
		DriveSlot->TaskIdOnRemove = TEXT("TakeDrive");
		DriveSlot->PromptInsert = Txt(TEXT("Slot_Insert"));
		DriveSlot->PromptRemove = Txt(TEXT("Slot_Remove"));
		DriveSlot->VisualSpec = Vis(TEXT("Socket"), FVector(10.f, 30.f, 6.f));
		Finish(DriveSlot, DriveLoc, FaceWest);

		const FVector StationLoc(3320.f, 0.f, 0.f);
		ADarcDataTransferStation* Station = SpawnDeferred<ADarcDataTransferStation>(StationLoc, FaceWest);
		Station->InterfaceSlot = InterfaceSlot;
		Station->DriveSlot = DriveSlot;
		Station->TaskIdOnComplete = TEXT("CopyArchive");
		Station->PromptStart = Txt(TEXT("Transfer_Start"));
		Station->VisualSpec = Vis(TEXT("ServerRackStatic"), FVector(80.f, 120.f, 200.f));
		Finish(Station, StationLoc, FaceWest);

		const FVector InletLoc(3380.f, -450.f, 40.f);
		ADarcPowerInlet* Inlet = SpawnDeferred<ADarcPowerInlet>(InletLoc, FaceWest);
		Inlet->CircuitId = TEXT("Server");
		Inlet->PromptConnect = Txt(TEXT("Inlet_Connect"));
		Inlet->PromptDisconnect = Txt(TEXT("Inlet_Disconnect"));
		Inlet->VisualSpec = Vis(TEXT("PowerInlet"), FVector(10.f, 30.f, 30.f));
		Finish(Inlet, InletLoc, FaceWest);

		// Консоль сервера: работает, только когда сервер запитан. Здесь виден «лишний файл».
		const FVector ConsoleLoc(3320.f, -300.f, 75.f);
		ADarcTerminal* Console = SpawnDeferred<ADarcTerminal>(ConsoleLoc, FaceWest);
		Console->bStartsUnlocked = true;
		Console->Power->CircuitId = TEXT("Server");
		Console->UnlockedTitle = Txt(TEXT("Screen_ArchiveTitle"));
		Console->ScreenLines = { Txt(TEXT("Screen_ArchiveList")), Txt(TEXT("Screen_ExtraFileHint")) };
		Console->CommandTasks.Add(TEXT("COPY"), TEXT("TakeExtraFile"));
		Console->PromptUse = Txt(TEXT("Terminal_Use"));
		Console->PromptBusy = Txt(TEXT("Terminal_Busy"));
		Console->PromptNoPower = Txt(TEXT("Terminal_NoPower"));
		Console->VisualSpec = Vis(TEXT("Terminal"), FVector(50.f, 45.f, 45.f));
		Finish(Console, ConsoleLoc, FaceWest);

		// Тяжёлая стойка: перекатить к рабочей точке у станции.
		ATargetPoint* WorkPoint = GetWorld()->SpawnActor<ATargetPoint>(FVector(3150.f, 450.f, 0.f), FRotator::ZeroRotator);
		const FVector RackLoc(2800.f, 550.f, 100.f);
		ADarcHeavyObject* Rack = SpawnDeferred<ADarcHeavyObject>(RackLoc);
		Rack->DeliveryTarget = WorkPoint;
		Rack->TaskIdOnDelivered = TEXT("MoveServerRack");
		Rack->PromptGrab = Txt(TEXT("Heavy_Grab"));
		Rack->PromptRelease = Txt(TEXT("Heavy_Release"));
		Rack->PromptReattachCable = Txt(TEXT("Heavy_ReattachCable"));
		Rack->VisualSpec = Vis(TEXT("ServerRack"), FVector(70.f, 90.f, 200.f));
		Finish(Rack, RackLoc);
	}

	// --- КПП: охранник с картой доступа ---
	{
		const FVector CardLoc(-900.f, 120.f, 100.f);
		ADarcKeycard* Card = SpawnDeferred<ADarcKeycard>(CardLoc);
		Card->MemoryId = TEXT("AccessCard");
		Card->PromptPickUp = Txt(TEXT("Carry_PickUp"));
		Card->PromptDrop = Txt(TEXT("Carry_Drop"));
		Card->VisualSpec = Vis(TEXT("Keycard"), FVector(8.5f, 5.4f, 0.5f));
		Finish(Card, CardLoc);

		const FVector GuardLoc(-900.f, 80.f, 0.f);
		const FRotator FacePlayers(0.f, 180.f, 0.f);
		ADarcNpc* Guard = SpawnDeferred<ADarcNpc>(GuardLoc, FacePlayers);
		Guard->SpeakerName = Txt(TEXT("Speaker_Guard"));
		Guard->GreetingLines = { Line(TEXT("Guard_Greeting"), 0.3f, 3.f) };
		Guard->ItemToGive = Card;
		Guard->TaskIdOnGive = TEXT("GetAccessCard");
		Guard->GiveLines = { Line(TEXT("Guard_GiveCard"), 0.2f, 2.5f) };
		Guard->AcceptedClass = ADarcDataDrive::StaticClass();
		Guard->BeforeReceiveLines = { Line(TEXT("Guard_Finished"), 0.2f, 2.5f) };
		Guard->TaskIdOnReceive = TEXT("DeliverDrive");
		Guard->ReceiveLines = { Line(TEXT("Guard_Good"), 0.3f, 2.f), Line(TEXT("Guard_GoBack"), 1.5f, 3.f) };
		Guard->PromptTalk = Txt(TEXT("Npc_Talk"));
		Guard->PromptHandOver = Txt(TEXT("Npc_HandOver"));
		Guard->VisualSpec = Vis(TEXT("Guard"), FVector(50.f, 50.f, 180.f));
		Finish(Guard, GuardLoc, FacePlayers);
	}
}
