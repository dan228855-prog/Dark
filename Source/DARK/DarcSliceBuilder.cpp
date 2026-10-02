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
#include "DarcHintComponent.h"
#include "DarcBarrierGate.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
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
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include <type_traits>

namespace DarcSlice
{
	constexpr float WallHeight = 300.f;
	constexpr float WallThickness = 20.f;
	// Проём под реальную дверь: ~1.2 × 2.3 м (персонаж ~1.8 м). Было 2 × 2.2 м — створка
	// модели (79 см) не дотягивалась до краёв, дверь выглядела маленькой посреди широкой дыры.
	constexpr float DoorWidth = 120.f;
	constexpr float DoorHeight = 230.f;
	constexpr float DoorFrame = 3.f; // ширина дверной коробки внутри проёма

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
	StartAmbience();
	if (BlinkLights.Num() > 0)
	{
		GetWorldTimerManager().SetTimer(BlinkTimer, this, &ADarcSliceBuilder::ToggleBlinkLights, 0.9f, true);
	}
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
	UMaterialInterface* Material = UDarcAssetSettings::GetTiledMaterial(MaterialSlot, Mesh);
	Mesh->SetMaterial(0, Material ? Material : LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
}

void ADarcSliceBuilder::AddWall(const FVector2D& A, const FVector2D& B, const TArray<float>& DoorCenters, FName MaterialSlot)
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
		AddBox(Min, Max, MaterialSlot);
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
		// Дверная коробка: наличники по всей толщине стены внутри проёма. Створка встаёт в неё
		// вплотную — свет больше не «просачивается» в щели между створкой и стеной.
		Segment(D0, D0 + DoorFrame, 0.f, DoorHeight);
		Segment(D1 - DoorFrame, D1, 0.f, DoorHeight);
		Segment(D0 + DoorFrame, D1 - DoorFrame, DoorHeight - DoorFrame, DoorHeight);
		Cursor = D1;
	}
	Segment(Cursor, End, 0.f, WallHeight);
}

void ADarcSliceBuilder::BuildEnvironment()
{
	// Ночь: луна высоко впереди слева, тёмно-синее небо, низовой туман по земле, тёплые
	// натриевые фонари и окна. Всё подвижное (Lumen). Внутри здания без ламп — темно (фонарик).
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;

	// Луна — единственный «направленный» свет. Для неба она тоже источник (иначе небо чёрное),
	// но слабая и холодная: атмосфера при таком свете остаётся тёмно-синей, как ночью, а диск
	// бледный и крупнее солнечного. Высоко (−32°) — чтобы не читалась как закатное солнце.
	if (ADirectionalLight* Moon = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0.f, 0.f, 2000.f), FRotator(-32.f, 141.f, 0.f), Params))
	{
		Moon->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* MoonLight = Cast<UDirectionalLightComponent>(Moon->GetLightComponent()))
		{
			MoonLight->SetIntensity(0.8f); // люксы: полнолуние ~0.3, чуть ярче ради читаемости
			MoonLight->SetLightColor(FLinearColor(0.55f, 0.66f, 1.f));
			MoonLight->SetAtmosphereSunLight(true);
			MoonLight->SetAtmosphereSunDiskColorScale(FLinearColor(0.85f, 0.9f, 1.f));
			MoonLight->SetLightSourceAngle(1.6f); // крупный размытый диск, без «солнечной» короны
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
		Light->SetIntensity(1.5f);
		Light->SetLightColor(FLinearColor(0.6f, 0.7f, 1.f)); // лунная заливка теней
		Light->RecaptureSky();
	}

	if (AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>(FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator, Params))
	{
		// Два слоя: общий лёгкий ночной туман и плотный низовой (до ~1 м), стелющийся по земле.
		// Объёмный туман даёт ореолы у фонарей и луч фонарика.
		UExponentialHeightFogComponent* FogComponent = Fog->GetComponent();
		FogComponent->SetFogDensity(0.012f);
		FogComponent->SetFogHeightFalloff(0.35f);
		FogComponent->SetFogInscatteringColor(FLinearColor(0.02f, 0.025f, 0.05f));
		FogComponent->SetDirectionalInscatteringColor(FLinearColor(0.08f, 0.1f, 0.16f));
		FogComponent->SecondFogData.FogDensity = 0.35f;   // низовой слой
		FogComponent->SecondFogData.FogHeightFalloff = 3.f; // быстро редеет: держится у земли
		FogComponent->SecondFogData.FogHeightOffset = 0.f;
		FogComponent->SetVolumetricFog(true);
		FogComponent->SetVolumetricFogScatteringDistribution(0.7f);
		FogComponent->MarkRenderStateDirty();
	}

	// Автоэкспозиция для ночи (EV100): глаз «привыкает», но не до дня. Нижняя граница −1 —
	// ночь читается, но остаётся тёмной; внутри без света — почти темно, нужен фонарик.
	if (APostProcessVolume* ExposureVolume = GetWorld()->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		ExposureVolume->bUnbound = true;
		FPostProcessSettings& PP = ExposureVolume->Settings;
		PP.bOverride_AutoExposureMinBrightness = true;
		PP.AutoExposureMinBrightness = -1.f;
		PP.bOverride_AutoExposureMaxBrightness = true;
		PP.AutoExposureMaxBrightness = 6.f;
		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = -0.7f;
		// Холодная ночная гамма, мягкое свечение огней, виньетка.
		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(0.85f, 0.9f, 1.05f, 0.8f);
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 1.f;
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.6f;
	}
}

// ---------------------------------------------------------------------------
// Улица у КПП
// ---------------------------------------------------------------------------

UStaticMeshComponent* ADarcSliceBuilder::AddShape(const TCHAR* Shape, const FVector& Center, const FVector& Size,
	const FLinearColor& Color, const FRotator& Rotation, bool bCollision)
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;
	AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rotation, Center), Params);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->SetReplicates(false); // строится у каждого сам
	UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape)));
	Mesh->SetWorldScale3D(Size / 100.f);
	Mesh->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
	// Цвет — параметр Color у стандартного материала фигур.
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Mesh);
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Mesh->SetMaterial(0, Material);
	}
	return Mesh;
}

UPointLightComponent* ADarcSliceBuilder::AddLight(const FVector& Location, const FLinearColor& Color, float Lumens, float Radius)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetupAttachment(RootComponent);
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetIntensity(Lumens);
	Light->SetAttenuationRadius(Radius);
	Light->SetLightColor(Color);
	Light->SetCastShadows(false); // декоративные огни — без теней, дёшево
	Light->RegisterComponent();
	Light->SetWorldLocation(Location);
	AddInstanceComponent(Light);
	return Light;
}

void ADarcSliceBuilder::AddTree(const FVector2D& Location, float Height, float Radius)
{
	// Настоящая ель (слоты Characters: Tree, Tree_1 … — скелетные модели Megaplant), если есть.
	// Скелет не обновляется (bNoSkeletonUpdate) — по цене почти как статичная модель.
	const uint32 Hash = GetTypeHash(FIntPoint(FMath::RoundToInt(Location.X), FMath::RoundToInt(Location.Y)));
	if (USkeletalMesh* TreeMesh = UDarcAssetSettings::FindCharacterVariant(TEXT("Tree"), Hash))
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.ObjectFlags |= RF_Transient;
		const FRotator Yaw(0.f, static_cast<float>(Hash % 360), 0.f);
		if (AActor* Tree = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Yaw, FVector(Location, 0.f)), Params))
		{
			Tree->SetReplicates(false);
			USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Tree, TEXT("Tree"));
			Tree->SetRootComponent(Mesh);
			Mesh->SetSkeletalMeshAsset(TreeMesh);
			Mesh->bNoSkeletonUpdate = true;
			Mesh->SetComponentTickEnabled(false);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetWorldScale3D(FVector(Height / 1400.f)); // разброс роста
			Mesh->RegisterComponent();
			Mesh->SetWorldLocationAndRotation(FVector(Location, 0.f), Yaw);
		}
		// Ствол — простая коллизия.
		if (UStaticMeshComponent* Trunk = AddShape(TEXT("Cylinder"), FVector(Location, 200.f), FVector(45.f, 45.f, 400.f), FLinearColor::Black))
		{
			Trunk->SetHiddenInGame(true);
		}
		return;
	}

	// Ель силуэтом: ствол и два конуса. Коллизия только у ствола — сквозь «лапы» можно пройти.
	const FLinearColor Trunk(0.035f, 0.025f, 0.02f);
	const FLinearColor Needles(0.018f, 0.03f, 0.022f);
	AddShape(TEXT("Cylinder"), FVector(Location, Height * 0.2f), FVector(35.f, 35.f, Height * 0.4f), Trunk);
	AddShape(TEXT("Cone"), FVector(Location, Height * 0.45f), FVector(Radius * 2.f, Radius * 2.f, Height * 0.55f), Needles, FRotator::ZeroRotator, false);
	AddShape(TEXT("Cone"), FVector(Location, Height * 0.75f), FVector(Radius * 1.3f, Radius * 1.3f, Height * 0.5f), Needles, FRotator::ZeroRotator, false);
}

void ADarcSliceBuilder::StartAmbience()
{
	// Раньше фоновый звук не играл вообще: присланные эмбиент-треки лежали в проекте, но ни
	// один слот и ни одна строка кода их не запускали. Теперь — слоты AmbientMusic (2D, тихо,
	// везде), AmbientNight (2D, ночь за окном) и ServerRoomHum (3D, у сервера). Зацикливаем сами.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	auto Loop = [](USoundBase* Sound)
	{
		if (USoundWave* Wave = Cast<USoundWave>(Sound))
		{
			Wave->bLooping = true;
		}
		return Sound;
	};
	if (USoundBase* Music = Loop(UDarcAssetSettings::FindSound(TEXT("AmbientMusic"))))
	{
		if (UAudioComponent* Audio = UGameplayStatics::CreateSound2D(this, Music, 0.35f, 1.f, 0.f, nullptr, false, false))
		{
			Audio->Play();
		}
	}
	if (USoundBase* Night = Loop(UDarcAssetSettings::FindSound(TEXT("AmbientNight"))))
	{
		if (UAudioComponent* Audio = UGameplayStatics::CreateSound2D(this, Night, 0.3f, 1.f, 0.f, nullptr, false, false))
		{
			Audio->Play();
		}
	}
	if (USoundBase* Hum = Loop(UDarcAssetSettings::FindSound(TEXT("ServerRoomHum"))))
	{
		UGameplayStatics::SpawnSoundAtLocation(this, Hum, FVector(3100.f, 0.f, 150.f), FRotator::ZeroRotator, 0.6f, 1.f, 0.f,
			UDarcAssetSettings::GetDefaultAttenuation(Hum), nullptr, false);
	}
}

void ADarcSliceBuilder::ToggleBlinkLights()
{
	bBlinkOn = !bBlinkOn;
	for (UPointLightComponent* Light : BlinkLights)
	{
		if (Light)
		{
			Light->SetVisibility(bBlinkOn);
		}
	}
	for (UStaticMeshComponent* Mesh : BlinkMeshes)
	{
		if (Mesh)
		{
			Mesh->SetVisibility(bBlinkOn);
		}
	}
}

void ADarcSliceBuilder::BuildExterior()
{
	// Раскладка: игроки появляются на дороге (X ≈ -2400) лицом к зданию (+X). Забор — по X = -1150
	// с воротами на дороге (Y от -250 до 250), будка КПП справа от ворот, вывеска — слева.
	const FLinearColor Dark(0.03f, 0.03f, 0.035f);
	const FLinearColor Concrete(0.25f, 0.25f, 0.26f);
	const FLinearColor Sodium(1.f, 0.55f, 0.25f);
	const FLinearColor WarmWindow(1.f, 0.7f, 0.4f);
	const FLinearColor Red(1.f, 0.05f, 0.03f);

	// Дорога — грунтовка от леса до здания.
	AddBox(FVector(-12000.f, -250.f, 0.f), FVector(0.f, 250.f, 1.5f),
		UDarcAssetSettings::FindMaterial(TEXT("Road")) ? FName(TEXT("Road")) : FName(TEXT("Ground")));

	// Забор: столбы через 300 см, сетка — тонкая тёмная «панель». Модель слота Fence, если задана.
	constexpr float FenceX = -1150.f;
	for (float Y = -2400.f; Y < 2400.f; Y += 300.f)
	{
		const float Center = Y + 150.f;
		if (FMath::Abs(Center) < 300.f)
		{
			continue; // ворота
		}
		AddShape(TEXT("Cylinder"), FVector(FenceX, Y, 125.f), FVector(8.f, 8.f, 250.f), Dark);
		if (UDarcAssetSettings::FindMesh(TEXT("Fence")))
		{
			FActorSpawnParameters Params;
			Params.Owner = this;
			Params.ObjectFlags |= RF_Transient;
			// Модель — дочерним компонентом: тогда ApplyVisual ставит её низом на землю по центру пролёта.
			if (AActor* Panel = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
				FTransform(FRotator(0.f, 90.f, 0.f), FVector(FenceX, Center, 0.f)), Params))
			{
				USceneComponent* Root = NewObject<USceneComponent>(Panel);
				Panel->SetRootComponent(Root);
				Root->RegisterComponent();
				Root->SetWorldLocationAndRotation(FVector(FenceX, Center, 0.f), FRotator(0.f, 90.f, 0.f));
				UStaticMeshComponent* PanelMesh = NewObject<UStaticMeshComponent>(Panel);
				PanelMesh->SetupAttachment(Root);
				PanelMesh->RegisterComponent();
				UDarcAssetSettings::ApplyVisual(PanelMesh, TEXT("Fence"), FVector(300.f, 6.f, 220.f));
			}
		}
		else
		{
			AddShape(TEXT("Cube"), FVector(FenceX, Center, 115.f), FVector(3.f, 296.f, 210.f), FLinearColor(0.06f, 0.06f, 0.07f));
		}
	}
	// Створки ворот (открыты внутрь) — декор без коллизии.
	AddShape(TEXT("Cube"), FVector(FenceX + 130.f, -330.f, 115.f), FVector(260.f, 4.f, 210.f), Dark, FRotator::ZeroRotator, false);
	AddShape(TEXT("Cube"), FVector(FenceX + 130.f, 330.f, 115.f), FVector(260.f, 4.f, 210.f), Dark, FRotator::ZeroRotator, false);
	// Шлагбаум поперёк дороги: с коллизией, сам плавно поднимается при подходе игрока.
	{
		FActorSpawnParameters GateParams;
		GateParams.Owner = this;
		GateParams.ObjectFlags |= RF_Transient;
		GetWorld()->SpawnActor<ADarcBarrierGate>(ADarcBarrierGate::StaticClass(), FTransform(FVector(FenceX + 40.f, 300.f, 0.f)), GateParams);
	}

	// Вывеска D.A.R.C. на заборе слева от ворот (текст — из String Table).
	AddShape(TEXT("Cube"), FVector(FenceX - 6.f, -750.f, 150.f), FVector(6.f, 600.f, 170.f), FLinearColor(0.02f, 0.02f, 0.025f));
	auto AddSignText = [this](const TCHAR* Key, const FVector& Location, float Size)
	{
		UTextRenderComponent* Text = NewObject<UTextRenderComponent>(this);
		Text->SetMobility(EComponentMobility::Movable);
		Text->SetupAttachment(RootComponent);
		Text->SetText(UDarcGameplayLibrary::UIText(Key));
		Text->SetWorldSize(Size);
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		Text->SetTextRenderColor(FColor(170, 170, 165));
		Text->RegisterComponent();
		// Текст читается со стороны -X (оттуда подходят игроки).
		Text->SetWorldLocationAndRotation(Location, FRotator(0.f, 180.f, 0.f));
		AddInstanceComponent(Text);
	};
	AddSignText(TEXT("Sign_DarcName"), FVector(FenceX - 10.f, -750.f, 175.f), 70.f);
	AddSignText(TEXT("Sign_DarcSub"), FVector(FenceX - 10.f, -750.f, 115.f), 16.f);

	// Будка: тёплое окно к дороге и натриевый фонарь на столбе между будкой и воротами.
	if (UStaticMeshComponent* BoothWindow = AddShape(TEXT("Cube"), FVector(-1052.f, 400.f, 150.f), FVector(4.f, 110.f, 80.f), WarmWindow, FRotator::ZeroRotator, false))
	{
		BoothWindow->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(BoothWindow, WarmWindow, 4.f));
	}
	AddLight(FVector(-1110.f, 400.f, 150.f), WarmWindow, 350.f, 450.f);
	AddShape(TEXT("Cylinder"), FVector(-1000.f, 200.f, 225.f), FVector(14.f, 14.f, 450.f), Dark);
	AddShape(TEXT("Cube"), FVector(-1000.f, 160.f, 450.f), FVector(20.f, 90.f, 10.f), Dark);
	AddShape(TEXT("Cylinder"), FVector(-1100.f, -300.f, 125.f), FVector(10.f, 10.f, 250.f), Dark); // фонарик на воротах
	AddLight(FVector(-1100.f, -300.f, 255.f), Sodium, 500.f, 600.f);

	// Тарелка: опора, «чаша» (сплюснутая сфера, наклонена к небу) и облучатель с красным огнём.
	const FVector DishBase(-1600.f, -1700.f, 0.f);
	AddShape(TEXT("Cylinder"), DishBase + FVector(0.f, 0.f, 350.f), FVector(160.f, 160.f, 700.f), Concrete);
	AddShape(TEXT("Cube"), DishBase + FVector(0.f, 0.f, 760.f), FVector(260.f, 200.f, 140.f), Concrete);
	const FRotator DishTilt(50.f, 150.f, 0.f);
	const FVector DishCenter = DishBase + FVector(0.f, 0.f, 1150.f);
	if (UDarcAssetSettings::FindMesh(TEXT("SatelliteDish")))
	{
		// Есть модель тарелки — она вместо серой «чаши».
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.ObjectFlags |= RF_Transient;
		if (AStaticMeshActor* Dish = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(DishBase), Params))
		{
			Dish->SetReplicates(false);
			UDarcAssetSettings::ApplyVisual(Dish->GetStaticMeshComponent(), TEXT("SatelliteDish"), FVector(1100.f, 1100.f, 1600.f));
		}
	}
	else
	{
		AddShape(TEXT("Sphere"), DishCenter, FVector(120.f, 1100.f, 1100.f), FLinearColor(0.3f, 0.3f, 0.32f), DishTilt);
	}
	// Облучатель — перед чашей по направлению, куда она смотрит.
	const FVector Feed = DishCenter + DishTilt.Vector() * 450.f;
	AddShape(TEXT("Cylinder"), (DishCenter + Feed) * 0.5f, FVector(12.f, 12.f, 450.f), Dark,
		FRotationMatrix::MakeFromZ(DishTilt.Vector()).Rotator(), false);
	BlinkLights.Add(AddLight(Feed, Red, 300.f, 500.f));
	if (UStaticMeshComponent* FeedLamp = AddShape(TEXT("Sphere"), Feed, FVector(25.f), Red, FRotator::ZeroRotator, false))
	{
		FeedLamp->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(FeedLamp, Red, 20.f));
		BlinkMeshes.Add(FeedLamp);
	}

	// Радиовышка за зданием с красными огнями.
	const FVector TowerBase(3900.f, 1500.f, 0.f);
	for (const FVector2D Leg : { FVector2D(-120.f, -120.f), FVector2D(120.f, -120.f), FVector2D(-120.f, 120.f), FVector2D(120.f, 120.f) })
	{
		// Ноги сходятся к вершине.
		const FVector Bottom = TowerBase + FVector(Leg, 0.f);
		const FVector Top = TowerBase + FVector(Leg * 0.2f, 4500.f);
		AddShape(TEXT("Cylinder"), (Bottom + Top) * 0.5f, FVector(14.f, 14.f, FVector::Dist(Bottom, Top)), Dark,
			FRotationMatrix::MakeFromZ(Top - Bottom).Rotator(), false);
	}
	for (const float Z : { 1500.f, 3000.f, 4500.f })
	{
		BlinkLights.Add(AddLight(TowerBase + FVector(0.f, 0.f, Z), Red, 600.f, 900.f));
		if (UStaticMeshComponent* Beacon = AddShape(TEXT("Sphere"), TowerBase + FVector(0.f, 0.f, Z), FVector(30.f), Red, FRotator::ZeroRotator, false))
		{
			Beacon->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(Beacon, Red, 25.f));
			BlinkMeshes.Add(Beacon);
		}
	}

	// Машина игроков слева от дороги, задние фонари горят.
	const FVector Car(-2700.f, -480.f, 0.f);
	AddShape(TEXT("Cube"), Car + FVector(0.f, 0.f, 95.f), FVector(460.f, 195.f, 110.f), FLinearColor(0.02f, 0.022f, 0.02f));
	AddShape(TEXT("Cube"), Car + FVector(-40.f, 0.f, 190.f), FVector(300.f, 185.f, 85.f), FLinearColor(0.02f, 0.022f, 0.02f));
	for (const float Y : { -75.f, 75.f })
	{
		if (UStaticMeshComponent* TailLight = AddShape(TEXT("Cube"), Car + FVector(-232.f, Y, 115.f), FVector(4.f, 25.f, 15.f), Red, FRotator::ZeroRotator, false))
		{
			TailLight->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(TailLight, Red, 10.f));
		}
		AddLight(Car + FVector(-260.f, Y, 115.f), Red, 120.f, 350.f);
		for (const float X : { -150.f, 150.f })
		{
			AddShape(TEXT("Cylinder"), Car + FVector(X, Y * 1.25f, 40.f), FVector(80.f, 80.f, 30.f), FLinearColor(0.01f, 0.01f, 0.01f),
				FRotator(0.f, 0.f, 90.f), false);
		}
	}

	// Лес: ели вокруг, кроме дороги, площадки у ворот, здания, тарелки и места появления.
	// Генератор с постоянным зерном — у всех игроков одинаковый лес.
	FRandomStream Stream(1337);
	auto IsFree = [](const FVector2D& P)
	{
		const auto In = [&P](float X0, float Y0, float X1, float Y1) { return P.X > X0 && P.X < X1 && P.Y > Y0 && P.Y < Y1; };
		return !In(-12000.f, -550.f, 0.f, 550.f)          // дорога
			&& !In(-1500.f, -1200.f, -500.f, 1000.f)      // ворота, будка, вывеска
			&& !In(-300.f, -1200.f, 4300.f, 1200.f)       // здание
			&& !In(1200.f, -2000.f, 3900.f, -1100.f)      // площадка генератора у южной стены
			&& !In(3500.f, 1100.f, 4300.f, 1900.f)        // вышка
			&& !In(-2300.f, -2400.f, -900.f, -1000.f);    // тарелка
	};
	int32 Placed = 0;
	// Настоящие ели (скелетные модели Megaplant) — дороже серых, поэтому их меньше.
	const int32 TreeCount = UDarcAssetSettings::FindCharacter(TEXT("Tree")) ? 130 : 220;
	for (int32 Attempt = 0; Attempt < 2000 && Placed < TreeCount; ++Attempt)
	{
		const FVector2D P(Stream.FRandRange(-9000.f, 9000.f), Stream.FRandRange(-9000.f, 9000.f));
		if (IsFree(P))
		{
			AddTree(P, Stream.FRandRange(900.f, 1800.f), Stream.FRandRange(220.f, 380.f));
			++Placed;
		}
	}

	// --- Мелочи окружения: камни на траве, мусор и ограждения у дороги и ворот ---
	// (у каждой машины одинаково — тот же генератор случайности)
	for (int32 Attempt = 0, Rocks = 0; Attempt < 600 && Rocks < 45; ++Attempt)
	{
		const FVector2D P(Stream.FRandRange(-6000.f, 6000.f), Stream.FRandRange(-6000.f, 6000.f));
		if (IsFree(P))
		{
			const float Size = Stream.FRandRange(50.f, 170.f);
			AddProp(TEXT("Rock"), FVector(P, -Size * 0.12f), Stream.FRandRange(0.f, 360.f), FVector(Size, Size * 0.85f, Size * 0.55f), Size > 90.f);
			++Rocks;
		}
	}
	// У дороги до ворот: конусы, бетонный блок, бочки, паллеты, брезент.
	AddProp(TEXT("TrafficBarrel"), FVector(-1500.f, -320.f, 0.f), 0.f, FVector(81.f, 81.f, 94.f));
	AddProp(TEXT("TrafficBarrel"), FVector(-1650.f, 330.f, 0.f), 30.f, FVector(81.f, 81.f, 94.f));
	AddProp(TEXT("Roadblock"), FVector(-1350.f, -700.f, 0.f), 80.f, FVector(56.f, 81.f, 119.f));
	AddProp(TEXT("Barrel"), FVector(-900.f, 700.f, 0.f), 0.f, FVector(58.f, 58.f, 84.f));
	AddProp(TEXT("Barrel"), FVector(-830.f, 760.f, 0.f), 40.f, FVector(58.f, 58.f, 84.f));
	AddProp(TEXT("Pallet"), FVector(-700.f, 620.f, 0.f), 12.f, FVector(116.f, 76.f, 13.f));
	AddProp(TEXT("Tarp"), FVector(-2100.f, 600.f, 0.f), 25.f, FVector(277.f, 151.f, 10.f), false);
	AddProp(TEXT("CableReel"), FVector(-400.f, -1000.f, 0.f), 0.f, FVector(153.f, 125.f, 153.f));
	// У здания: уличный электрошкаф, бочки, ящики.
	AddProp(TEXT("StreetElectricalBox"), FVector(-40.f, -500.f, 0.f), 90.f, FVector(48.f, 34.f, 179.f));
	AddProp(TEXT("Barrel"), FVector(-60.f, 650.f, 0.f), 0.f, FVector(58.f, 58.f, 84.f));
	AddProp(TEXT("Crate"), FVector(1200.f, 950.f, 0.f), 10.f, FVector(83.f, 66.f, 77.f));
	AddProp(TEXT("Pallet"), FVector(2000.f, 960.f, 0.f), -5.f, FVector(116.f, 76.f, 13.f));
	AddProp(TEXT("BarrelBlue"), FVector(1500.f, -1000.f, 0.f), 0.f, FVector(53.f, 52.f, 70.f));

	// Холмы на горизонте — тёмные силуэты в тумане.
	for (int32 i = 0; i < 12; ++i)
	{
		const float Angle = FMath::DegreesToRadians(i * 30.f + Stream.FRandRange(-10.f, 10.f));
		const float Distance = Stream.FRandRange(20000.f, 32000.f);
		const FVector Center(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, -1500.f);
		AddShape(TEXT("Sphere"), Center, FVector(Stream.FRandRange(14000.f, 24000.f), Stream.FRandRange(14000.f, 24000.f), Stream.FRandRange(5000.f, 9000.f)),
			FLinearColor(0.015f, 0.02f, 0.018f), FRotator::ZeroRotator, false);
	}
}

void ADarcSliceBuilder::AddRoom(const FVector2D& Min, const FVector2D& Max)
{
	// Пол комнаты (поверх земли) — отдельным материалом.
	AddBox(FVector(Min.X, Min.Y, 0.f), FVector(Max.X, Max.Y, 2.f), TEXT("Floor"));
}

UStaticMeshComponent* ADarcSliceBuilder::AddProp(FName MeshSlot, const FVector& BottomCenter, float Yaw, const FVector& BoxSize,
	bool bCollision, FName MaterialSlot)
{
	// Пропс у каждой машины: модель — только картинка (вписана в коробку BoxSize и стоит
	// низом на BottomCenter), коллизия — простая невидимая коробка того же размера. Поэтому
	// ничего не висит в воздухе, сквозь мебель не пройти, а коллизия самой модели не важна.
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;
	AActor* Prop = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), BottomCenter), Params);
	if (!Prop)
	{
		return nullptr;
	}
	Prop->SetReplicates(false);
	USceneComponent* Root = NewObject<USceneComponent>(Prop, TEXT("Root"));
	Prop->SetRootComponent(Root);
	Root->RegisterComponent();
	Root->SetWorldLocationAndRotation(BottomCenter, FRotator(0.f, Yaw, 0.f));

	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Prop, TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->RegisterComponent();
	UDarcAssetSettings::ApplyVisual(Mesh, MeshSlot, BoxSize, MaterialSlot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (bCollision)
	{
		UBoxComponent* Box = NewObject<UBoxComponent>(Prop, TEXT("Collision"));
		Box->SetupAttachment(Root);
		Box->SetBoxExtent(BoxSize * 0.5f);
		Box->SetRelativeLocation(FVector(0.f, 0.f, BoxSize.Z * 0.5f));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
	}
	return Mesh;
}

void ADarcSliceBuilder::AddWallWithWindows(const FVector2D& A, const FVector2D& B, float Z0, float Z1,
	const TArray<float>& WindowCenters, FName MaterialSlot, int32 LitEvery)
{
	// Стена этажа с окнами: подоконник Z0..Sill, простенки, перемычка, стекло в проёме.
	// Часть окон светится тёплым светом изнутри (каждое LitEvery-е) — здание «живое» снаружи.
	using namespace DarcSlice;
	const bool bAlongX = FMath::IsNearlyEqual(A.Y, B.Y);
	const float Start = bAlongX ? FMath::Min(A.X, B.X) : FMath::Min(A.Y, B.Y);
	const float End = bAlongX ? FMath::Max(A.X, B.X) : FMath::Max(A.Y, B.Y);
	const float Fixed = bAlongX ? A.Y : A.X;
	const float Half = WallThickness * 0.5f;
	const float Sill = Z0 + 90.f;
	const float Head = Z0 + 210.f;
	constexpr float WindowWidth = 140.f;

	auto Box = [&](float S0, float S1, float H0, float H1, float Inset, FName Slot)
	{
		if (S1 - S0 < 1.f || H1 - H0 < 1.f)
		{
			return;
		}
		const float T = Half - Inset;
		AddBox(bAlongX ? FVector(S0, Fixed - T, H0) : FVector(Fixed - T, S0, H0),
			bAlongX ? FVector(S1, Fixed + T, H1) : FVector(Fixed + T, S1, H1), Slot);
	};

	TArray<float> Windows = WindowCenters;
	Windows.Sort();
	float Cursor = Start;
	int32 Index = 0;
	for (const float Center : Windows)
	{
		const float W0 = Center - WindowWidth * 0.5f;
		const float W1 = Center + WindowWidth * 0.5f;
		Box(Cursor, W0, Z0, Z1, 0.f, MaterialSlot);
		Box(W0, W1, Z0, Sill, 0.f, MaterialSlot);
		Box(W0, W1, Head, Z1, 0.f, MaterialSlot);
		// Стекло — тонкое, по центру стены; горящие окна — светящаяся панель за стеклом.
		const FVector GlassCenter = bAlongX ? FVector(Center, Fixed, (Sill + Head) * 0.5f) : FVector(Fixed, Center, (Sill + Head) * 0.5f);
		const FVector GlassSize = bAlongX ? FVector(WindowWidth, 3.f, Head - Sill) : FVector(3.f, WindowWidth, Head - Sill);
		const bool bLit = LitEvery > 0 && (Index++ % LitEvery) == 0;
		if (UStaticMeshComponent* Glass = AddShape(TEXT("Cube"), GlassCenter, GlassSize, FLinearColor::Black))
		{
			Glass->SetMaterial(0, bLit
				? UDarcAssetSettings::MakeColorMaterial(Glass, FLinearColor(1.f, 0.62f, 0.3f), 3.f)
				: UDarcAssetSettings::MakeColorMaterial(Glass, FLinearColor(0.02f, 0.025f, 0.03f)));
		}
		Cursor = W1;
	}
	Box(Cursor, End, Z0, Z1, 0.f, MaterialSlot);
}

void ADarcSliceBuilder::BuildGeometry()
{
	using namespace DarcSlice;

	// Земля вокруг (лес/КПП) и пол здания.
	AddBox(FVector(-12000.f, -12000.f, -20.f), FVector(12000.f, 12000.f, 0.f), TEXT("Ground"));
	AddRoom(FVector2D(0.f, -800.f), FVector2D(800.f, 800.f));      // лобби
	AddRoom(FVector2D(800.f, -150.f), FVector2D(2600.f, 150.f));   // коридор
	AddRoom(FVector2D(800.f, 150.f), FVector2D(2600.f, 800.f));    // северные комнаты
	AddRoom(FVector2D(800.f, -800.f), FVector2D(2600.f, -150.f));  // южные комнаты
	AddRoom(FVector2D(2600.f, -800.f), FVector2D(3400.f, 800.f));  // серверная

	// --- Первый этаж ---
	// Внешние стены (вход — в западной стене по центру). Бетон снаружи (WallOutside), если задан.
	const FName Outside = UDarcAssetSettings::FindMaterial(TEXT("WallOutside")) ? FName(TEXT("WallOutside")) : FName(TEXT("Wall"));
	AddWall(FVector2D(0.f, -800.f), FVector2D(0.f, 800.f), { 0.f }, Outside);
	AddWall(FVector2D(3400.f, -800.f), FVector2D(3400.f, 800.f), {}, Outside);
	AddWall(FVector2D(0.f, 800.f), FVector2D(3400.f, 800.f), {}, Outside);
	AddWall(FVector2D(0.f, -800.f), FVector2D(3400.f, -800.f), {}, Outside);

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

	// --- Перекрытие между этажами с проёмом над лестницей ---
	// Лестница — в лобби вдоль южной стены, поднимается на восток (X 100 → 700).
	constexpr float StairY0 = -790.f;
	constexpr float StairY1 = -640.f;
	constexpr float StairX0 = 100.f;
	constexpr int32 StepCount = 20;
	constexpr float StepRun = 30.f;
	const float StepRise = (WallHeight + 22.f) / StepCount;
	const float HoleX1 = StairX0 + StepCount * StepRun + 20.f;
	auto Slab = [this](const FVector& Min, const FVector& Max)
	{
		AddBox(FVector(Min.X, Min.Y, WallHeight), FVector(Max.X, Max.Y, WallHeight + 18.f), TEXT("Ceiling")); // снизу — потолок
		AddBox(FVector(Min.X, Min.Y, WallHeight + 18.f), FVector(Max.X, Max.Y, WallHeight + 22.f), TEXT("Floor")); // сверху — пол
	};
	Slab(FVector(0.f, StairY1, 0.f), FVector(3400.f, 800.f, 0.f));
	Slab(FVector(0.f, -800.f, 0.f), FVector(StairX0, StairY1, 0.f));
	Slab(FVector(HoleX1, -800.f, 0.f), FVector(3400.f, StairY1, 0.f));
	for (int32 Step = 0; Step < StepCount; ++Step)
	{
		const float X = StairX0 + Step * StepRun;
		AddBox(FVector(X, StairY0, 0.f), FVector(X + StepRun, StairY1, (Step + 1) * StepRise), TEXT("Floor"));
	}
	// Перила вокруг проёма на втором этаже.
	const FLinearColor Rail(0.08f, 0.08f, 0.09f);
	const float Floor2 = WallHeight + 22.f;
	AddShape(TEXT("Cube"), FVector((StairX0 + HoleX1) * 0.5f, StairY1 + 3.f, Floor2 + 50.f), FVector(HoleX1 - StairX0, 5.f, 100.f), Rail);
	AddShape(TEXT("Cube"), FVector(StairX0 - 3.f, (StairY0 + StairY1) * 0.5f, Floor2 + 50.f), FVector(5.f, StairY1 - StairY0, 100.f), Rail);

	// --- Второй этаж: архив (открытый зал с двумя секциями), окна по периметру ---
	const float Z2 = Floor2;
	const float Z2Top = WallHeight * 2.f + 22.f;
	AddWallWithWindows(FVector2D(0.f, -800.f), FVector2D(0.f, 800.f), WallHeight, Z2Top, { -450.f, 0.f, 450.f }, Outside, 2);
	AddWallWithWindows(FVector2D(3400.f, -800.f), FVector2D(3400.f, 800.f), WallHeight, Z2Top, { -400.f, 400.f }, Outside, 0);
	AddWallWithWindows(FVector2D(0.f, 800.f), FVector2D(3400.f, 800.f), WallHeight, Z2Top, { 500.f, 1300.f, 2100.f, 2900.f }, Outside, 3);
	AddWallWithWindows(FVector2D(0.f, -800.f), FVector2D(3400.f, -800.f), WallHeight, Z2Top, { 1100.f, 1900.f, 2700.f }, Outside, 2);
	// Перегородка между секциями архива (широкий проём).
	{
		const float Half = WallThickness * 0.5f;
		AddBox(FVector(1700.f - Half, -800.f, Z2), FVector(1700.f + Half, -150.f, Z2Top), TEXT("Wall"));
		AddBox(FVector(1700.f - Half, 150.f, Z2), FVector(1700.f + Half, 800.f, Z2Top), TEXT("Wall"));
	}
	// Крыша.
	AddBox(FVector(-20.f, -820.f, Z2Top), FVector(3420.f, 820.f, Z2Top + 25.f), TEXT("Ceiling"));

	// Архив: стеллажи, ящики, паллеты — зал не пустой.
	for (const float X : { 1000.f, 1350.f })
	{
		AddProp(TEXT("Shelf"), FVector(X, 450.f, Z2), 90.f, FVector(76.f, 203.f, 180.f));
		AddProp(TEXT("Shelf"), FVector(X, -350.f, Z2), 90.f, FVector(76.f, 203.f, 180.f));
	}
	AddProp(TEXT("Crate"), FVector(2200.f, 500.f, Z2), 15.f, FVector(83.f, 66.f, 77.f));
	AddProp(TEXT("CardboardBox"), FVector(2450.f, -500.f, Z2), -10.f, FVector(124.f, 84.f, 62.f));
	AddProp(TEXT("Pallet"), FVector(3000.f, 300.f, Z2), 5.f, FVector(116.f, 76.f, 13.f));
	AddProp(TEXT("Desk"), FVector(2900.f, -450.f, Z2), 0.f, FVector(81.f, 166.f, 75.f));
	AddProp(TEXT("BarrelBlue"), FVector(3200.f, 600.f, Z2), 0.f, FVector(53.f, 52.f, 70.f));

	// --- Мебель первого этажа (модели вместо коробок «из ДСП») ---
	// Рабочая комната: письменный стол (на нём монитор каталога и кружка).
	AddProp(TEXT("Desk"), FVector(1400.f, 690.f, 0.f), 90.f, FVector(81.f, 166.f, 75.f));
	// Тех. помещение: стеллаж с интерфейсным модулем.
	AddProp(TEXT("ShelfLow"), FVector(2465.f, 715.f, 0.f), 0.f, FVector(203.f, 76.f, 90.f));
	// Кладовая: стол с запасным предохранителем, ящики.
	AddProp(TEXT("Table"), FVector(1490.f, -700.f, 0.f), 0.f, FVector(152.f, 86.f, 77.f));
	AddProp(TEXT("CardboardBox"), FVector(950.f, -700.f, 0.f), 0.f, FVector(124.f, 84.f, 62.f));
	AddProp(TEXT("Crate"), FVector(1000.f, -320.f, 0.f), 30.f, FVector(83.f, 66.f, 77.f));
	// Серверная: стол под консоль сервера.
	AddProp(TEXT("Desk"), FVector(3330.f, -300.f, 0.f), 0.f, FVector(81.f, 166.f, 75.f));
	// Будка КПП.
	AddBox(FVector(-1050.f, 250.f, 0.f), FVector(-750.f, 550.f, 260.f), TEXT("Booth"));

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;

	// Улица у КПП: дорога, забор, шлагбаум, вывеска, лес, холмы, тарелка, вышка, машина.
	BuildExterior();

	// Разметка на полу серверной: сюда катить стойку (точка доставки задачи MoveServerRack).
	AddShape(TEXT("Cube"), FVector(3150.f, 450.f, 3.f), FVector(150.f, 150.f, 1.f), FLinearColor(0.8f, 0.6f, 0.05f), FRotator::ZeroRotator, false);
	AddShape(TEXT("Cube"), FVector(3150.f, 450.f, 3.5f), FVector(130.f, 130.f, 1.f), FLinearColor(0.05f, 0.05f, 0.05f), FRotator::ZeroRotator, false);

	// Вентиляционная решётка над второй дверью коридора (северная стена) — она же «динамик»
	// для звуковых событий (якорь Speaker ставит сервер в BuildGameplay в эту же точку).
	AddProp(TEXT("Vent"), FVector(1800.f, 137.f, 238.f), 0.f, FVector(70.f, 6.f, 55.f), false);

	// Невидимые стены по краю земли: раньше земля кончалась за забором (75 × 50 м), игрок
	// уходил в лес и падал с края — отсюда «провал под карту». Сетка безопасности в
	// ADarcPlayerController остаётся на случай других дыр.
	constexpr float Edge = 11800.f;
	for (const FVector& Wall : { FVector(Edge, 0.f, 0.f), FVector(-Edge, 0.f, 0.f), FVector(0.f, Edge, 0.f), FVector(0.f, -Edge, 0.f) })
	{
		const bool bAlongY = !FMath::IsNearlyZero(Wall.X);
		if (UStaticMeshComponent* Blocker = AddShape(TEXT("Cube"), Wall + FVector(0.f, 0.f, 1000.f),
			bAlongY ? FVector(100.f, 2.f * Edge, 2000.f) : FVector(2.f * Edge, 100.f, 2000.f), FLinearColor::Black))
		{
			Blocker->SetHiddenInGame(true);
		}
	}

	// Табличка в кладовой с кодом серверной: тёмная пластина на стене, подпись из String Table
	// и цифры (из сида выезда — у всех одинаковые). Раньше были просто красные цифры в воздухе.
	{
		const FVector PlatePos(1000.f, -788.f, 160.f);
		AddShape(TEXT("Cube"), PlatePos, FVector(70.f, 3.f, 45.f), FLinearColor(0.05f, 0.05f, 0.055f), FRotator::ZeroRotator, false);
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		Label->SetMobility(EComponentMobility::Movable);
		Label->SetupAttachment(RootComponent);
		Label->SetText(UDarcGameplayLibrary::UIText(TEXT("Sign_ServerCode")));
		Label->SetWorldSize(7.f);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetTextRenderColor(FColor(200, 200, 190));
		Label->RegisterComponent();
		Label->SetWorldLocationAndRotation(PlatePos + FVector(0.f, 2.5f, 12.f), FRotator(0.f, 90.f, 0.f));
		AddInstanceComponent(Label);

		UDarcCodeTextComponent* CodeText = NewObject<UDarcCodeTextComponent>(this);
		CodeText->CodeKey = TEXT("ServerDoorCode");
		CodeText->SetMobility(EComponentMobility::Movable);
		CodeText->SetupAttachment(RootComponent);
		CodeText->SetWorldSize(18.f);
		CodeText->SetTextRenderColor(FColor(230, 230, 220));
		CodeText->RegisterComponent();
		CodeText->SetWorldLocationAndRotation(PlatePos + FVector(0.f, 2.5f, -6.f), FRotator(0.f, 90.f, 0.f));
		AddInstanceComponent(CodeText);
	}

	// Кружка на столе — «мелочь класса C» и цель левитации. Своя у каждой машины (локальная физика).
	if (AStaticMeshActor* Mug = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FVector(1450.f, 700.f, 80.f)), Params))
	{
		Mug->SetReplicates(false);
		UStaticMeshComponent* MugMesh = Mug->GetStaticMeshComponent();
		MugMesh->SetMobility(EComponentMobility::Movable);
		UDarcAssetSettings::ApplyVisual(MugMesh, TEXT("Mug"), FVector(10.f, 10.f, 12.f));
		MugMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		MugMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // на кружку не наступить и не «уехать» на ней
		MugMesh->SetSimulatePhysics(true);
		Mug->Tags.Add(TEXT("LevitationMug"));
	}

	// Точки появления игроков (до 5, кооп).
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

void ADarcSliceBuilder::AddHint(AActor* Actor, FName NameKey, FName HintKey)
{
	UDarcHintComponent::AddHint(Actor, NameKey, HintKey); // ВРЕМЕННО — см. DarcHintComponent.h
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
	// Материал DoorMaterial — только для серой коробки; у модели двери остаётся её собственный.
	// Створка — ровно по коробке (без щелей) и толще, чтобы не просвечивала.
	Door->VisualSpec = Vis(TEXT("Door"), FVector(8.f, DoorWidth - 2.f * DoorFrame, DoorHeight - DoorFrame), TEXT("DoorMaterial"), FVector(0.f, DoorWidth * 0.5f, 0.f));
	Finish(Door, Hinge, Rotation);
	return Door;
}

ADarcPowerLamp* ADarcSliceBuilder::SpawnLamp(const FVector& Location, FName CircuitId, bool bStreetLight)
{
	using namespace DarcSlice;
	ADarcPowerLamp* Lamp = SpawnDeferred<ADarcPowerLamp>(Location);
	Lamp->Power->CircuitId = CircuitId;
	Lamp->bStreetLight = bStreetLight;
	Lamp->VisualSpec = bStreetLight ? Vis(TEXT("StreetLamp"), FVector(50.f, 30.f, 15.f)) : Vis(TEXT("Lamp"), FVector(120.f, 25.f, 8.f));
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
		Box->PromptInsertFuseBreakerOn = Txt(TEXT("Fuse_InsertBreakerOn"));
		Box->PromptBreakerOn = Txt(TEXT("Breaker_On"));
		Box->PromptBreakerOff = Txt(TEXT("Breaker_Off"));
		if (bTaskOnBlow)
		{
			Box->bAddTaskOnBlow = true;
			Box->TaskOnBlow.TaskId = TEXT("RestoreCorridorLight");
			Box->TaskOnBlow.Title = Txt(TEXT("Task_RestoreCorridorLight"));
			Box->TaskOnBlow.bRequired = false;
		}
		// Лицо щитка — локальная +X (в коридор): тонкая сторона коробки — по X. Было наоборот —
		// щитки стояли к игроку боком.
		Box->VisualSpec = Vis(TEXT("FuseBox"), FVector(15.f, 50.f, 70.f), NAME_None, FVector(0.f, 0.f, 90.f));
		Finish(Box, Location, FaceCorridor);
		// Своё название у каждого щитка; у щитка без предохранителя — что именно делать.
		AddHint(Box, *(FString(TEXT("Name_FuseBox_")) + Circuit.ToString()), bHasFuse ? FName(TEXT("Hint_FuseBox")) : FName(TEXT("Hint_FuseBox_NoFuse")));
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
		AddHint(Item, *(FString(TEXT("Name_")) + MeshSlot.ToString()), *(FString(TEXT("Hint_")) + MeshSlot.ToString()));
		return Item;
	};
	MakeItem(static_cast<ADarcFuseItem*>(nullptr), FVector(2060.f, -100.f, 20.f), TEXT("Fuse"), FVector(12.f, 4.f, 4.f), TEXT("Fuse_Main"));
	MakeItem(static_cast<ADarcFuseItem*>(nullptr), FVector(1490.f, -700.f, 85.f), TEXT("Fuse"), FVector(12.f, 4.f, 4.f), TEXT("Fuse_Spare")); // на столе в кладовой
	MakeItem(static_cast<ADarcInterfaceModule*>(nullptr), FVector(2465.f, 715.f, 85.f), TEXT("InterfaceModule"), FVector(25.f, 15.f, 8.f), TEXT("InterfaceModule")); // на столе в тех. помещении

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
	// Второй этаж (архив).
	for (const float X : { 600.f, 1300.f, 2300.f, 3000.f })
	{
		SpawnLamp(FVector(X, 0.f, 610.f), TEXT("Building"));
	}
	// Натриевый фонарь у ворот (столб строит BuildExterior): тёплый, ярче и дальше комнатных ламп.
	SpawnLamp(FVector(-1000.f, 120.f, 440.f), TEXT("Building"), true);

	// --- Рабочая комната: терминал каталога ---
	{
		const FVector Loc(1360.f, 705.f, 75.f); // на письменном столе
		const FRotator Rot(0.f, -90.f, 0.f);   // лицом в комнату (на юг)
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
		AddHint(Catalog, TEXT("Name_Terminal"), TEXT("Hint_Terminal"));
	}

	// --- Переносной генератор: в кладовой, длинной стороной к двери ---
	// 160 × 80 × 100 см: проходит в двери (1.14 м в свету) боком; 170 кг — тяжёлый.
	{
		const FVector Loc(1200.f, -560.f, 55.f);
		const FRotator Rot(0.f, 90.f, 0.f);
		ADarcGenerator* Generator = SpawnDeferred<ADarcGenerator>(Loc, Rot);
		Generator->PromptReattachCable = Txt(TEXT("Heavy_ReattachCable"));
		Generator->MassKg = 170.f;
		Generator->VisualSpec = Vis(TEXT("Generator"), FVector(160.f, 80.f, 100.f));
		Finish(Generator, Loc, Rot);
		AddHint(Generator, TEXT("Name_Generator"), TEXT("Hint_Generator"));
	}

	// --- Тех. шкаф: исчезающая комната ---
	{
		// Шкаф — тяжёлый объект: его можно сдвинуть вдвоём, а «комната» переставляет его сама.
		const FVector CabinetLoc(1800.f, 650.f, 85.f);
		ADarcHeavyObject* Cabinet = SpawnDeferred<ADarcHeavyObject>(CabinetLoc);
		Cabinet->bHasFragileCable = false;
		Cabinet->MassKg = 220.f; // тяжёлое техническое оборудование
		// Металлический электрошкаф (модель слота TechCabinet; без модели — металлическая коробка).
		Cabinet->VisualSpec = Vis(TEXT("TechCabinet"), FVector(129.f, 40.f, 165.f), TEXT("TechCabinetMaterial"));
		Finish(Cabinet, CabinetLoc);
		AddHint(Cabinet, TEXT("Name_TechCabinet"));

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
		// Звук «из вентиляции»: якорь — в решётке над второй дверью коридора (решётку строит
		// BuildGeometry у каждой машины), своей модели у якоря нет.
		const FVector Loc(1800.f, 128.f, 265.f);
		ADarcRareEventAnchor* Speaker = SpawnDeferred<ADarcRareEventAnchor>(Loc);
		Speaker->SupportedEventTypes = { TEXT("Speaker") };
		Speaker->RoomId = TEXT("Corridor");
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
		const FVector Loc(-12.f, 500.f, 195.f); // окно в наружной стене лобби (вплотную к стене)
		ADarcRareEventAnchor* Window = SpawnDeferred<ADarcRareEventAnchor>(Loc);
		Window->SupportedEventTypes = { TEXT("WindowLight") };
		Window->Behavior = EDarcAnchorBehavior::LightOn;
		Window->VisualSpec = Vis(TEXT("Window"), FVector(4.f, 120.f, 90.f), TEXT("WindowGlass"));
		Finish(Window, Loc);
	}

	// --- Серверная: дверь (карта/код), станция, слоты, ввод генератора, стойка ---
	{
		// Считыватель — слева от двери серверной (если стоять к ней лицом из коридора), вплотную.
		// Было: Y = 160 — внутри стены соседней комнаты.
		const FVector ReaderLoc(2586.f, -88.f, 115.f);
		const FRotator FaceWest(0.f, 180.f, 0.f);
		ADarcCardReader* Reader = SpawnDeferred<ADarcCardReader>(ReaderLoc, FaceWest);
		Reader->AcceptedClass = ADarcKeycard::StaticClass();
		Reader->DoorToUnlock = ServerDoor;
		Reader->TaskIdOnAccess = TEXT("ServerDoorCard");
		Reader->PromptSwipe = Txt(TEXT("Card_Swipe"));
		Reader->VisualSpec = Vis(TEXT("CardReader"), FVector(8.f, 12.f, 18.f));
		Finish(Reader, ReaderLoc, FaceWest);
		AddHint(Reader, TEXT("Name_CardReader"), TEXT("Hint_CardReader"));

		const FVector KeypadLoc(2586.f, 88.f, 115.f); // справа от двери
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
		AddHint(Keypad, TEXT("Name_Keypad"), TEXT("Hint_Keypad"));
	}
	{
		const FRotator FaceWest(0.f, 180.f, 0.f);

		// Разъёмы — на лицевой панели сервера (станция: центр X 3320, глубина 80 → лицо X 3280),
		// а не в воздухе перед ним.
		const FVector InterfaceLoc(3277.f, 35.f, 110.f);
		ADarcItemSlot* InterfaceSlot = SpawnDeferred<ADarcItemSlot>(InterfaceLoc, FaceWest);
		InterfaceSlot->AcceptedClass = ADarcInterfaceModule::StaticClass();
		InterfaceSlot->TaskIdOnInsert = TEXT("ConnectInterface");
		InterfaceSlot->PromptInsert = Txt(TEXT("Slot_Insert"));
		InterfaceSlot->PromptRemove = Txt(TEXT("Slot_Remove"));
		InterfaceSlot->VisualSpec = Vis(TEXT("Socket"), FVector(6.f, 30.f, 14.f));
		Finish(InterfaceSlot, InterfaceLoc, FaceWest);
		AddHint(InterfaceSlot, TEXT("Name_InterfaceSlot"), TEXT("Hint_InterfaceSlot"));

		const FVector DriveLoc(3277.f, -35.f, 110.f);
		ADarcDataDrive* Drive = SpawnDeferred<ADarcDataDrive>(DriveLoc);
		Drive->MemoryId = TEXT("ArchiveDrive");
		Drive->PromptPickUp = Txt(TEXT("Carry_PickUp"));
		Drive->PromptDrop = Txt(TEXT("Carry_Drop"));
		Drive->VisualSpec = Vis(TEXT("Drive"), FVector(14.f, 9.f, 3.f));
		Finish(Drive, DriveLoc);
		AddHint(Drive, TEXT("Name_Drive"), TEXT("Hint_Drive"));

		ADarcItemSlot* DriveSlot = SpawnDeferred<ADarcItemSlot>(DriveLoc, FaceWest);
		DriveSlot->AcceptedClass = ADarcDataDrive::StaticClass();
		DriveSlot->InitialItem = Drive;
		DriveSlot->TaskIdOnRemove = TEXT("TakeDrive");
		DriveSlot->PromptInsert = Txt(TEXT("Slot_Insert"));
		DriveSlot->PromptRemove = Txt(TEXT("Slot_Remove"));
		DriveSlot->VisualSpec = Vis(TEXT("Socket"), FVector(6.f, 30.f, 14.f));
		Finish(DriveSlot, DriveLoc, FaceWest);
		AddHint(DriveSlot, TEXT("Name_DriveSlot"), TEXT("Hint_DriveSlot"));

		const FVector StationLoc(3320.f, 0.f, 0.f);
		ADarcDataTransferStation* Station = SpawnDeferred<ADarcDataTransferStation>(StationLoc, FaceWest);
		Station->InterfaceSlot = InterfaceSlot;
		Station->DriveSlot = DriveSlot;
		Station->TaskIdOnComplete = TEXT("CopyArchive");
		Station->PromptStart = Txt(TEXT("Transfer_Start"));
		Station->VisualSpec = Vis(TEXT("ServerRackStatic"), FVector(80.f, 120.f, 200.f), TEXT("ServerMaterial"));
		Finish(Station, StationLoc, FaceWest);
		AddHint(Station, TEXT("Name_Server"), TEXT("Hint_Server"));

		// Ввод питания — в серверной, на восточной стене, лицом в зал.
		const FVector InletLoc(3388.f, -450.f, 40.f);
		ADarcPowerInlet* Inlet = SpawnDeferred<ADarcPowerInlet>(InletLoc, FaceWest);
		Inlet->CircuitId = TEXT("Server");
		Inlet->PromptConnect = Txt(TEXT("Inlet_Connect"));
		Inlet->PromptDisconnect = Txt(TEXT("Inlet_Disconnect"));
		Inlet->VisualSpec = Vis(TEXT("PowerInlet"), FVector(10.f, 30.f, 30.f));
		Finish(Inlet, InletLoc, FaceWest);
		AddHint(Inlet, TEXT("Name_Inlet"), TEXT("Hint_Inlet"));

		// Консоль сервера: работает, только когда сервер запитан. Здесь виден «лишний файл».
		const FVector ConsoleLoc(3320.f, -300.f, 75.f); // на столе (BuildGeometry), лицом в зал
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
		AddHint(Console, TEXT("Name_Console"), TEXT("Hint_Console"));

		// Тяжёлая стойка: перекатить к рабочей точке у станции.
		ATargetPoint* WorkPoint = GetWorld()->SpawnActor<ATargetPoint>(FVector(3150.f, 450.f, 0.f), FRotator::ZeroRotator);
		const FVector RackLoc(2800.f, 550.f, 100.f);
		ADarcHeavyObject* Rack = SpawnDeferred<ADarcHeavyObject>(RackLoc);
		Rack->DeliveryTarget = WorkPoint;
		Rack->TaskIdOnDelivered = TEXT("MoveServerRack");
		Rack->PromptReattachCable = Txt(TEXT("Heavy_ReattachCable"));
		Rack->PromptCableTooFar = Txt(TEXT("Heavy_CableTooFar"));
		Rack->MassKg = 120.f;
		// Кабель питания от розетки на северной стене: видно провод, дальше длины его выдёргивает.
		Rack->CableAnchor = FVector(3000.f, 788.f, 40.f);
		Rack->CableLength = 450.f;
		Rack->VisualSpec = Vis(TEXT("ServerRack"), FVector(70.f, 90.f, 200.f), TEXT("ServerMaterial"));
		Finish(Rack, RackLoc);
		AddHint(Rack, TEXT("Name_Rack"), TEXT("Hint_Rack"));
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
		AddHint(Card, TEXT("Name_Keycard"), TEXT("Hint_Keycard"));

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
		AddHint(Guard, TEXT("Name_Guard"), TEXT("Hint_Guard"));
	}
}
