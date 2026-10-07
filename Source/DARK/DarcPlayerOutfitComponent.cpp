// DarcPlayerOutfitComponent.cpp
#include "DarcPlayerOutfitComponent.h"
#include "DarcAssetSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "DARKCharacter.h"
#include "CarryableItem.h"
#include "DarcFlashlightComponent.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"

UDarcPlayerOutfitComponent::UDarcPlayerOutfitComponent()
{
	PrimaryComponentTick.bCanEverTick = true;           // только у своего игрока (присед), включается в BeginPlay
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true); // сам ничего не шлёт — нужен, чтобы появиться у клиентов
}

void UDarcPlayerOutfitComponent::BeginPlay()
{
	Super::BeginPlay();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	USkeletalMesh* Mesh = UDarcAssetSettings::FindCharacter(TEXT("Player"));
	if (!Mesh)
	{
		Mesh = UDarcAssetSettings::FindCharacter(TEXT("Guard"));
	}
	if (!Body || !Mesh || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	Outfit = NewObject<USkeletalMeshComponent>(Character, MakeUniqueObjectName(Character, USkeletalMeshComponent::StaticClass(), TEXT("OutfitMesh")));
	Outfit->SetupAttachment(Body);
	Outfit->SetSkeletalMeshAsset(Mesh);
	Outfit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Декали дороги/площадки на движущемся теле «плывут».
	Outfit->SetReceivesDecals(false);
	// Видимость — как у тела шаблона: свой игрок своё тело не видит (только тень), другие — видят.
	Outfit->SetOwnerNoSee(Body->bOwnerNoSee);
	Outfit->bCastHiddenShadow = true;
	Outfit->RegisterComponent();
	Character->AddInstanceComponent(Outfit);
	Outfit->SetLeaderPoseComponent(Body);

	// Манекен остаётся источником анимации, но не рисуется.
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Body->SetVisibility(false, false);

	// Вид от первого лица: та же модель поверх рук шаблона (их анимация — источник позы).
	// Видна только самому игроку; голова (камера внутри неё) и ноги скрыты.
	const ADARKCharacter* FirstPersonCharacter = Cast<ADARKCharacter>(Character);
	USkeletalMeshComponent* Arms = FirstPersonCharacter ? FirstPersonCharacter->GetFirstPersonMesh() : nullptr;
	if (!Arms)
	{
		return;
	}
	OutfitFirstPerson = NewObject<USkeletalMeshComponent>(Character, MakeUniqueObjectName(Character, USkeletalMeshComponent::StaticClass(), TEXT("OutfitFirstPersonMesh")));
	OutfitFirstPerson->SetupAttachment(Arms);
	OutfitFirstPerson->SetSkeletalMeshAsset(Mesh);
	OutfitFirstPerson->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OutfitFirstPerson->SetReceivesDecals(false);
	OutfitFirstPerson->SetOnlyOwnerSee(true);
	OutfitFirstPerson->SetCastShadow(false);
	OutfitFirstPerson->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	OutfitFirstPerson->RegisterComponent();
	Character->AddInstanceComponent(OutfitFirstPerson);
	OutfitFirstPerson->SetLeaderPoseComponent(Arms);
	// Голову (с бородой, бровями, кепкой — всё висит на ней) и ноги прячем. Модель повторяет
	// позу рук шаблона и берёт у них же видимость костей, поэтому прячем и там (руки шаблона
	// всё равно невидимы; камера держится за сокет head, он от этого не меняется).
	for (const FName Bone : { FName(TEXT("neck_01")), FName(TEXT("head")), FName(TEXT("thigh_l")), FName(TEXT("thigh_r")) })
	{
		for (USkeletalMeshComponent* Target : { OutfitFirstPerson.Get(), Arms })
		{
			if (Target && Target->GetBoneIndex(Bone) != INDEX_NONE)
			{
				Target->HideBoneByName(Bone, EPhysBodyOp::PBO_None);
			}
		}
	}
	Arms->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Arms->SetVisibility(false, false); // стандартные руки манекена больше не видны

	if (Character->IsLocallyControlled())
	{
		GetWorld()->GetTimerManager().SetTimer(ArmsTimer, this, &UDarcPlayerOutfitComponent::UpdateArmsVisibility, 0.1f, true, 0.f);
		SetComponentTickEnabled(true);
	}
}

void UDarcPlayerOutfitComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const ADARKCharacter* FirstPersonCharacter = Cast<ADARKCharacter>(Character);
	USkeletalMeshComponent* Arms = FirstPersonCharacter ? FirstPersonCharacter->GetFirstPersonMesh() : nullptr;
	USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	if (!Arms || !Body || Body->GetBoneIndex(TEXT("head")) == INDEX_NONE)
	{
		return;
	}
	if (!FirstPersonBaseLocation.IsSet())
	{
		FirstPersonBaseLocation = Arms->GetRelativeLocation();
	}
	// Высота головы тела над ногами (низ капсулы): стоя запоминаем, в приседе — насколько ниже.
	const float Feet = Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float HeadAboveFeet = Body->GetSocketLocation(TEXT("head")).Z - Feet;
	float Target = 0.f;
	if (!Character->bIsCrouched)
	{
		StandingHeadAboveFeet = HeadAboveFeet;
	}
	else if (StandingHeadAboveFeet > 0.f)
	{
		Target = -FMath::Clamp(StandingHeadAboveFeet - HeadAboveFeet, 0.f, 110.f);
	}
	CrouchOffset = FMath::FInterpTo(CrouchOffset, Target, DeltaTime, 10.f);
	Arms->SetRelativeLocation(FirstPersonBaseLocation.GetValue() + FVector(0.f, 0.f, CrouchOffset));
}

void UDarcPlayerOutfitComponent::UpdateArmsVisibility()
{
	if (!OutfitFirstPerson)
	{
		return;
	}
	const UDarcFlashlightComponent* Flashlight = GetOwner()->FindComponentByClass<UDarcFlashlightComponent>();
	const bool bRight = ACarryableItem::FindItemHeldBy(GetOwner()) != nullptr;
	const bool bLeft = Flashlight && Flashlight->IsOn();
	auto Show = [this](TOptional<bool>& State, bool bWanted, const TCHAR* Bone)
	{
		if (State.IsSet() && State.GetValue() == bWanted)
		{
			return;
		}
		State = bWanted;
		if (OutfitFirstPerson->GetBoneIndex(Bone) == INDEX_NONE)
		{
			return;
		}
		if (bWanted)
		{
			OutfitFirstPerson->UnHideBoneByName(Bone);
		}
		else
		{
			OutfitFirstPerson->HideBoneByName(Bone, EPhysBodyOp::PBO_None);
		}
	};
	Show(bRightShown, bRight, TEXT("upperarm_r"));
	Show(bLeftShown, bLeft, TEXT("upperarm_l"));
}
