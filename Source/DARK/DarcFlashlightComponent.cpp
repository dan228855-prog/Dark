// DarcFlashlightComponent.cpp
#include "DarcFlashlightComponent.h"
#include "DarcAssetSettings.h"
#include "DarcGameplayLibrary.h"
#include "DarcPlayerState.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
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
	BuildHousing();
}

void UDarcFlashlightComponent::BuildHousing()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	auto MakePart = [Character](const TCHAR* Name, const TCHAR* Shape)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Character, Name);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape)));
		return Part;
	};
	Housing = MakePart(TEXT("FlashlightHousing"), TEXT("Cylinder"));
	Lens = MakePart(TEXT("FlashlightLens"), TEXT("Cylinder"));
	Housing->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(Housing, FLinearColor(0.03f, 0.03f, 0.035f)));

	if (Character->IsLocallyControlled())
	{
		// У себя: у камеры, слева внизу, лучом вперёд — рисуется вместе с руками.
		UCameraComponent* Camera = Character->FindComponentByClass<UCameraComponent>();
		USceneComponent* Parent = Camera ? static_cast<USceneComponent*>(Camera) : Character->GetRootComponent();
		Housing->SetupAttachment(Parent);
		Housing->SetRelativeLocationAndRotation(FVector(38.f, -17.f, -16.f), FRotator(-90.f, 0.f, 0.f)); // ось цилиндра — вперёд
		Housing->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		Lens->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	}
	else
	{
		// У других: в левой руке модели (кость hand_l), иначе у груди.
		USkeletalMeshComponent* Body = Character->GetMesh();
		const bool bHand = Body && Body->DoesSocketExist(TEXT("hand_l"));
		Housing->SetupAttachment(bHand ? static_cast<USceneComponent*>(Body) : Character->GetRootComponent(), bHand ? FName(TEXT("hand_l")) : NAME_None);
		Housing->SetRelativeLocationAndRotation(bHand ? FVector(0.f, 8.f, 0.f) : FVector(25.f, -20.f, 40.f), FRotator(-90.f, 0.f, 0.f));
	}
	Housing->SetRelativeScale3D(FVector(0.045f, 0.045f, 0.2f)); // Ø4.5 × 20 см
	Housing->RegisterComponent();
	Character->AddInstanceComponent(Housing);

	// Линза — на переднем торце корпуса (в осях цилиндра: +Z).
	Lens->SetupAttachment(Housing);
	Lens->SetRelativeLocation(FVector(0.f, 0.f, 51.f));
	Lens->SetRelativeScale3D(FVector(1.1f, 1.1f, 0.03f));
	Lens->RegisterComponent();
	Character->AddInstanceComponent(Lens);
	UpdateLensGlow();
}

void UDarcFlashlightComponent::UpdateLensGlow()
{
	if (Lens)
	{
		Lens->SetMaterial(0, bOn
			? UDarcAssetSettings::MakeColorMaterial(Lens, FLinearColor(1.f, 0.95f, 0.85f), 15.f)
			: UDarcAssetSettings::MakeColorMaterial(Lens, FLinearColor(0.15f, 0.15f, 0.15f)));
	}
}

void UDarcFlashlightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (USceneComponent* Part : { static_cast<USceneComponent*>(Spot), static_cast<USceneComponent*>(Lens), static_cast<USceneComponent*>(Housing) })
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Spot = nullptr;
	Lens = nullptr;
	Housing = nullptr;
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
	UpdateLensGlow();
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
	UDarcGameplayLibrary::GetAimViewPoint(Pawn, Eyes, View);
	if (!Pawn->IsLocallyControlled())
	{
		View = Pawn->GetBaseAimRotation();
	}
	const FRotationMatrix Axes(View);
	const FVector Origin = Eyes + Axes.GetUnitAxis(EAxis::X) * 25.f + Axes.GetUnitAxis(EAxis::Y) * -15.f - Axes.GetUnitAxis(EAxis::Z) * 12.f;
	Spot->SetWorldLocationAndRotation(Origin, View);

	// В упор — тусклее: свет 900 лм с 20 см давал белое пятно, в котором ничего не видно.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcFlashlight), false, Pawn);
	float Factor = 1.f;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + View.Vector() * DimDistance, ECC_Visibility, Params))
	{
		Factor = FMath::Clamp(FMath::Pow(Hit.Distance / DimDistance, 1.5f), 0.12f, 1.f);
	}
	Spot->SetIntensity(Lumens * Factor);
}
