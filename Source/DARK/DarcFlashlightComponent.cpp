// DarcFlashlightComponent.cpp
#include "DarcFlashlightComponent.h"
#include "DarcAssetSettings.h"
#include "DarcPlayerState.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UDarcFlashlightComponent::UDarcFlashlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork; // после движения камеры — луч не отстаёт
	SetIsReplicatedByDefault(true);
}

void UDarcFlashlightComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDarcFlashlightComponent, bOn);
}

void UDarcFlashlightComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer || !GetOwner())
	{
		return;
	}
	Spot = NewObject<USpotLightComponent>(GetOwner(), TEXT("FlashlightBeam"));
	Spot->SetMobility(EComponentMobility::Movable);
	Spot->SetUsingAbsoluteLocation(true);
	Spot->SetUsingAbsoluteRotation(true);
	Spot->SetupAttachment(GetOwner()->GetRootComponent());
	Spot->SetIntensityUnits(ELightUnits::Lumens);
	Spot->SetIntensity(Lumens);
	Spot->SetAttenuationRadius(Range);
	Spot->SetInnerConeAngle(ConeAngle * 0.45f);
	Spot->SetOuterConeAngle(ConeAngle);
	Spot->SetLightColor(FLinearColor(1.f, 0.93f, 0.82f)); // тёплый светодиод
	Spot->SetVolumetricScatteringIntensity(0.6f);        // луч виден в тумане
	Spot->SetVisibility(bOn);
	Spot->RegisterComponent();
	GetOwner()->AddInstanceComponent(Spot);
}

void UDarcFlashlightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Spot)
	{
		Spot->DestroyComponent();
		Spot = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UDarcFlashlightComponent::Toggle()
{
	Server_SetOn(!bOn);
}

void UDarcFlashlightComponent::Server_SetOn_Implementation(bool bNewOn)
{
	// Погибшему фонарик не нужен.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const ADarcPlayerState* PS = Pawn ? Pawn->GetPlayerState<ADarcPlayerState>() : nullptr;
	if (PS && !PS->bIsAlive)
	{
		bNewOn = false;
	}
	if (bOn != bNewOn)
	{
		bOn = bNewOn;
		OnRep_On(); // на сервере OnRep сам не вызывается
	}
}

void UDarcFlashlightComponent::OnRep_On()
{
	if (Spot)
	{
		Spot->SetVisibility(bOn);
	}
	if (const AActor* Owner = GetOwner())
	{
		UDarcAssetSettings::PlaySound(this, TEXT("FlashlightClick"), Owner->GetActorLocation(), 0.6f);
	}
}

void UDarcFlashlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Spot || !bOn || !Pawn)
	{
		return;
	}
	// Из глаз по направлению взгляда (у других игроков — по реплицированному взгляду), чуть
	// правее и ниже — как фонарь в руке; заодно не слепит собственную модель.
	FVector Eyes;
	FRotator View;
	Pawn->GetActorEyesViewPoint(Eyes, View);
	if (!Pawn->IsLocallyControlled())
	{
		View = Pawn->GetBaseAimRotation();
	}
	const FRotationMatrix Axes(View);
	Spot->SetWorldLocationAndRotation(Eyes + Axes.GetUnitAxis(EAxis::X) * 25.f + Axes.GetUnitAxis(EAxis::Y) * 18.f - Axes.GetUnitAxis(EAxis::Z) * 12.f, View);
}
