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
#include "DARKCharacter.h"
#include "DarcBatteryItem.h"
#include "DarcInventoryComponent.h"
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
	DOREPLIFETIME(UDarcFlashlightComponent, Charge);
}

void UDarcFlashlightComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer || !GetOwner())
	{
		return;
	}
	Spot = NewObject<USpotLightComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), USpotLightComponent::StaticClass(), TEXT("FlashlightBeam")));
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
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Character, MakeUniqueObjectName(Character, UStaticMeshComponent::StaticClass(), Name));
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->SetReceivesDecals(false); // декали дороги на фонаре в руке «плывут»
		Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape)));
		return Part;
	};
	Housing = MakePart(TEXT("FlashlightHousing"), TEXT("Cylinder"));
	Lens = MakePart(TEXT("FlashlightLens"), TEXT("Cylinder"));
	Housing->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(Housing, FLinearColor(0.03f, 0.03f, 0.035f)));
	Housing->SetRelativeScale3D(FVector(0.045f, 0.045f, 0.2f)); // Ø4.5 × 20 см
	Housing->SetupAttachment(Character->GetRootComponent()); // в руку — в PlaceInHand (поза рук готова к первому кадру)
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

void UDarcFlashlightComponent::ResetHandPlacement()
{
	bPlacedInHand = false;
	PlaceInHand();
}

void UDarcFlashlightComponent::PlaceInHand()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	// Перекладываем, если сменилось «свой / чужой» (у клиента управление пешкой приходит
	// по сети чуть позже её появления).
	if (!Character || !Housing || (bPlacedInHand && bPlacedForLocal == Character->IsLocallyControlled()))
	{
		return;
	}
	bPlacedInHand = true;
	bPlacedForLocal = Character->IsLocallyControlled();

	// Своему игроку — в левую руку рук от первого лица (рисуется вместе с ними); остальным —
	// в левую руку модели тела. Смещение — в осях кости кисти: раньше оно считалось по взгляду
	// в первый кадр (игрок ещё сидел в машине), и фонарь оказывался над тыльной стороной ладони.
	const ADARKCharacter* FirstPersonCharacter = Cast<ADARKCharacter>(Character);
	USkeletalMeshComponent* Hand = Character->IsLocallyControlled() && FirstPersonCharacter
		? FirstPersonCharacter->GetFirstPersonMesh() : Character->GetMesh();
	const FName Bone(TEXT("hand_l"));
	if (Hand && Hand->DoesSocketExist(Bone))
	{
		Housing->AttachToComponent(Hand, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Bone);
		Housing->SetRelativeLocation(HandOffset);
	}
	else
	{
		// Нет руки — слева внизу у камеры.
		FVector Eyes;
		FRotator View;
		UDarcGameplayLibrary::GetAimViewPoint(Character, Eyes, View);
		const FRotationMatrix Axes(View);
		Housing->SetWorldLocationAndRotation(Eyes + Axes.GetUnitAxis(EAxis::X) * 38.f - Axes.GetUnitAxis(EAxis::Y) * 17.f - Axes.GetUnitAxis(EAxis::Z) * 16.f,
			FRotationMatrix::MakeFromZ(View.Vector()).Rotator());
		Housing->AttachToComponent(Character->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	}
	const bool bFirstPerson = Character->IsLocallyControlled();
	Housing->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
	Lens->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
	Housing->SetOwnerNoSee(false);
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
	// Батарейка села — включение меняет её на запасную (если есть), иначе фонарь не зажигается.
	if (bNewOn && Charge <= 0.f && !ServerUseSpareBattery())
	{
		UDarcAssetSettings::PlaySound(this, TEXT("FlashlightClick"), GetOwner()->GetActorLocation(), 0.4f);
		bNewOn = false;
	}
	if (bOn != bNewOn)
	{
		bOn = bNewOn;
		OnRep_On(); // на сервере OnRep сам не вызывается
	}
}

bool UDarcFlashlightComponent::ServerUseSpareBattery()
{
	AActor* Wearer = GetOwner();
	ADarcBatteryItem* Battery = Cast<ADarcBatteryItem>(ACarryableItem::FindItemHeldBy(Wearer));
	if (const UDarcInventoryComponent* Inventory = Wearer ? Wearer->FindComponentByClass<UDarcInventoryComponent>() : nullptr)
	{
		for (int32 i = 0; i < UDarcInventoryComponent::NumSlots && !Battery; ++i)
		{
			Battery = Cast<ADarcBatteryItem>(Inventory->GetSlotItem(i));
		}
	}
	if (!Battery)
	{
		return false;
	}
	Battery->ServerInsertInto(this);
	return true;
}

void UDarcFlashlightComponent::ServerSetCharge(float NewCharge)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Charge = FMath::Clamp(NewCharge, 0.f, 1.f);
	}
}

void UDarcFlashlightComponent::FlickerFor(float Seconds)
{
	if (const UWorld* World = GetWorld())
	{
		FlickerUntil = FMath::Max(FlickerUntil, World->GetTimeSeconds() + Seconds);
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

	// Сервер: включённый фонарь садит батарейку, в ноль — гаснет.
	if (bOn && GetOwner() && GetOwner()->HasAuthority() && BatterySeconds > 0.f)
	{
		Charge = FMath::Max(0.f, Charge - DeltaTime / BatterySeconds);
		if (Charge <= 0.f)
		{
			bOn = false;
			OnRep_On();
		}
	}
	PlaceInHand(); // один раз: фонарь в руке виден и выключенным
	const bool bInHand = Housing && Housing->GetAttachSocketName() == FName(TEXT("hand_l"));
	if (bInHand)
	{
		// Каждый кадр из настроек — HandOffset/HandRotation можно крутить у компонента Flashlight
		// прямо во время игры и сразу видеть результат.
		Housing->SetRelativeLocation(HandOffset);
	}
	if (bInHand && !HandRotation.IsNearlyZero())
	{
		// Поворот задан — корпус зафиксирован в кисти (в осях кости hand_l), луч всё равно по взгляду.
		Housing->SetRelativeRotation(HandRotation);
	}
	else if (Housing && Pawn)
	{
		// Поворот не задан — корпус смотрит туда же, куда игрок.
		FVector AimEyes;
		FRotator AimView;
		UDarcGameplayLibrary::GetAimViewPoint(Pawn, AimEyes, AimView);
		if (!Pawn->IsLocallyControlled())
		{
			AimView = Pawn->GetBaseAimRotation();
		}
		Housing->SetWorldRotation(FRotationMatrix::MakeFromZ(AimView.Vector()).Rotator());
	}
	if (!Spot || !bOn || !Pawn)
	{
		return;
	}
	PlaceInHand();

	// Свет — из линзы фонаря в руке, по направлению взгляда (у других игроков — по
	// реплицированному взгляду).
	FVector Eyes;
	FRotator View;
	UDarcGameplayLibrary::GetAimViewPoint(Pawn, Eyes, View);
	if (!Pawn->IsLocallyControlled())
	{
		View = Pawn->GetBaseAimRotation();
	}
	const FVector Origin = Lens ? Lens->GetComponentLocation() + View.Vector() * 2.f : Eyes;
	Spot->SetWorldLocationAndRotation(Origin, View);

	// В упор — тусклее: свет 900 лм с 20 см давал белое пятно, в котором ничего не видно.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcFlashlight), false, Pawn);
	float Factor = 1.f;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + View.Vector() * DimDistance, ECC_Visibility, Params))
	{
		Factor = FMath::Clamp(FMath::Pow(Hit.Distance / DimDistance, 1.5f), 0.12f, 1.f);
	}
	// Батарейка ниже четверти — свет слабее и иногда проседает; рядом «тень» — мигает сильно.
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bShadowFlicker = Now < FlickerUntil;
	if (Now >= NextFlickerChange)
	{
		const float FlickerChance = bShadowFlicker ? 0.6f : (Charge < 0.25f ? FMath::Lerp(0.35f, 0.04f, Charge / 0.25f) : 0.f);
		FlickerFactor = FMath::FRand() < FlickerChance ? FMath::FRandRange(bShadowFlicker ? 0.f : 0.15f, 0.5f) : 1.f;
		NextFlickerChange = Now + FMath::FRandRange(0.04f, bShadowFlicker ? 0.12f : 0.25f);
	}
	const float ChargeFactor = Charge < 0.25f ? FMath::Lerp(0.4f, 1.f, Charge / 0.25f) : 1.f;
	Spot->SetIntensity(Lumens * Factor * ChargeFactor * FlickerFactor);
}
