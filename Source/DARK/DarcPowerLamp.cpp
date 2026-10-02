// DarcPowerLamp.cpp
#include "DarcPowerLamp.h"
#include "Net/UnrealNetwork.h"
#include "DarcAssetSettings.h"
#include "DarcPowerConsumerComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
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
	Light->SetIntensity(2600.f);
	Light->SetAttenuationRadius(1300.f);
	Light->SetLightColor(FLinearColor(0.9f, 0.95f, 1.f)); // холодный люминесцентный
	Light->SetVisibility(false);

	Power = CreateDefaultSubobject<UDarcPowerConsumerComponent>(TEXT("Power"));
}

void ADarcPowerLamp::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual); // модель — у каждой машины сама
	if (bStreetLight)
	{
		Light->SetLightColor(FLinearColor(1.f, 0.55f, 0.25f));
		Light->SetIntensity(5000.f);
		Light->SetAttenuationRadius(2200.f);
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

void ADarcPowerLamp::ApplyLit(bool bLit)
{
	Light->SetVisibility(bLit);

	if (bLit && !Hum)
	{
		Hum = UDarcAssetSettings::PlayLoopAttached(TEXT("LampHum"), RootComponent);
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
	Light->SetVisibility(FMath::FRand() > 0.45f);
}

void ADarcPowerLamp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcPowerLamp, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ADarcPowerLamp, bStreetLight, COND_InitialOnly);
}
