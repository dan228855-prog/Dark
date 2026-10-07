// DarcPowerLamp.cpp
#include "DarcPowerLamp.h"
#include "Net/UnrealNetwork.h"
#include "DarcAssetSettings.h"
#include "DarcPowerConsumerComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"

ADarcPowerLamp::ADarcPowerLamp()
{
	bReplicates = true; // реплицируется компонент питания
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetRelativeLocation(FVector(0.f, 0.f, -30.f));
	// Явные единицы: ~люминесцентный светильник (люмены не зависят от настроек проекта).
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetIntensity(4000.f);
	Light->SetAttenuationRadius(1500.f);
	Light->SetLightColor(FLinearColor(0.9f, 0.95f, 1.f)); // холодный люминесцентный
	Light->SetVisibility(false);

	Glow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glow->SetCastShadow(false);
	Glow->SetVisibility(false);

	Power = CreateDefaultSubobject<UDarcPowerConsumerComponent>(TEXT("Power"));
}

void ADarcPowerLamp::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual); // модель — у каждой машины сама
	Visual->SetCastShadow(false); // плафон не должен затенять собственный свет

	// Рассеиватель: сплошная светящаяся панель по низу плафона. Была тонкая трубка — у модели
	// светильника середина корпуса пустая, и сквозь лампу был виден потолок («прозрачная» лампа).
	{
		const FBox Box = Visual->Bounds.GetBox().TransformBy(GetActorTransform().Inverse());
		const FVector Size = Box.GetSize();
		if (bStreetLight)
		{
			Glow->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
			Glow->SetRelativeLocation(FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z - 3.f));
			Glow->SetRelativeScale3D(FVector(0.16f, 0.16f, 0.06f));
		}
		else
		{
			Glow->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Glow->SetRelativeLocation(FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z - 0.6f));
			Glow->SetRelativeScale3D(FVector(FMath::Max(Size.X, 10.f) * 0.92f / 100.f, FMath::Max(Size.Y, 10.f) * 0.8f / 100.f, 0.012f));
		}
		const FLinearColor GlowColor = bStreetLight ? FLinearColor(1.f, 0.6f, 0.25f) : FLinearColor(0.85f, 0.92f, 1.f);
		GlowOnMaterial = UDarcAssetSettings::MakeColorMaterial(Glow, GlowColor, 12.f);
		// Погасшая панель — матовый молочный пластик (не дыра в плафоне).
		GlowOffMaterial = UDarcAssetSettings::MakeColorMaterial(Glow, FLinearColor(0.45f, 0.46f, 0.48f));
		Glow->SetMaterial(0, GlowOnMaterial);
		Light->SetRelativeLocation(FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z - 15.f));
	}
	if (bStreetLight)
	{
		Light->SetLightColor(FLinearColor(1.f, 0.55f, 0.25f));
		Light->SetIntensity(5000.f);
		Light->SetAttenuationRadius(2200.f);
	}
	else
	{
		Light->SetIntensity(Lumens);
		// Внутри здания: без рассеяния в тумане — иначе свет пятнами виден над крышей.
		Light->SetVolumetricScatteringIntensity(0.f);
		// Радиус — по комнате (было 15 м: лампы светили сквозь стены друг на друга, и каждая — с тенями,
		// отсюда падение FPS). Дальние лампы не рисуются вовсе — их всё равно не видно за стенами.
		Light->SetAttenuationRadius(900.f);
		Light->MaxDrawDistance = 3000.f;
		Light->MaxDistanceFadeRange = 600.f;
		Light->SetCastVolumetricShadow(false);
		Light->MarkRenderStateDirty();
	}

	// Делегаты срабатывают и на сервере, и у клиентов (по репликации) — свет у каждого свой.
	Power->OnPowerChanged.AddDynamic(this, &ADarcPowerLamp::HandlePowerChanged);
	Power->OnFlicker.AddDynamic(this, &ADarcPowerLamp::HandleFlicker);
	ApplyLit(Power->IsPowered());
}

void ADarcPowerLamp::HandlePowerChanged(bool bPowered, EDarcPowerSource Source)
{
	GetWorldTimerManager().ClearTimer(FlickerTimer);
	ApplyLit(bPowered);
}

void ADarcPowerLamp::SetLightVisible(bool bVisible)
{
	Light->SetVisibility(bVisible);
	if (bStreetLight || !GlowOffMaterial)
	{
		Glow->SetVisibility(bVisible);
		return;
	}
	// Рассеиватель комнатной лампы виден всегда, светится — только когда лампа горит.
	Glow->SetVisibility(true);
	Glow->SetMaterial(0, bVisible ? GlowOnMaterial : GlowOffMaterial);
}

void ADarcPowerLamp::ApplyLit(bool bLit)
{
	SetLightVisible(bLit);

	if (bLit && !Hum)
	{
		Hum = UDarcAssetSettings::PlayLoopAttached(TEXT("LampHum"), RootComponent);
		if (Hum)
		{
			// Гул слышен только у самой лампы, и одновременно гудят лишь ближайшие:
			// ламп на уровне десятки — иначе в коридоре гул складывается в громкий шум.
			static TStrongObjectPtr<USoundAttenuation> NearOnly;
			static TStrongObjectPtr<USoundConcurrency> FewNearest;
			if (!NearOnly.IsValid())
			{
				NearOnly.Reset(NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("DarcLampHumAttenuation")));
				FSoundAttenuationSettings& A = NearOnly->Attenuation;
				A.bAttenuate = true;
				A.bSpatialize = true;
				A.AttenuationShape = EAttenuationShape::Sphere;
				A.AttenuationShapeExtents = FVector(80.f, 0.f, 0.f);
				A.FalloffDistance = 420.f;
				A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;

				FewNearest.Reset(NewObject<USoundConcurrency>(GetTransientPackage(), TEXT("DarcLampHumConcurrency")));
				FewNearest->Concurrency.MaxCount = 6;
				FewNearest->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopFarthestThenOldest;
			}
			Hum->Stop();
			Hum->AttenuationSettings = NearOnly.Get();
			Hum->ConcurrencySet.Add(FewNearest.Get());
			Hum->Play(FMath::FRandRange(0.f, 5.f)); // лампы гудят не в унисон
		}
	}
	else if (!bLit && Hum)
	{
		Hum->Stop();
		Hum = nullptr;
	}
}

void ADarcPowerLamp::HandleFlicker(float Duration)
{
	if (!Power->IsPowered())
	{
		return;
	}
	FlickerEndTime = GetWorld()->GetTimeSeconds() + Duration;
	UDarcAssetSettings::PlaySound(this, TEXT("LampFlicker"), GetActorLocation());
	GetWorldTimerManager().SetTimer(FlickerTimer, this, &ADarcPowerLamp::FlickerStep, 0.07f, true);
}

void ADarcPowerLamp::FlickerStep()
{
	if (GetWorld()->GetTimeSeconds() >= FlickerEndTime)
	{
		GetWorldTimerManager().ClearTimer(FlickerTimer);
		ApplyLit(Power->IsPowered());
		return;
	}
	// Неровное мигание: чаще горит, иногда гаснет.
	SetLightVisible(FMath::FRand() > 0.45f);
}

void ADarcPowerLamp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcPowerLamp, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ADarcPowerLamp, bStreetLight, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ADarcPowerLamp, Lumens, COND_InitialOnly);
}
