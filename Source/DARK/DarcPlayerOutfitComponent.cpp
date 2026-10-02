// DarcPlayerOutfitComponent.cpp
#include "DarcPlayerOutfitComponent.h"
#include "DarcAssetSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"

UDarcPlayerOutfitComponent::UDarcPlayerOutfitComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

	Outfit = NewObject<USkeletalMeshComponent>(Character, TEXT("Outfit"));
	Outfit->SetupAttachment(Body);
	Outfit->SetSkeletalMeshAsset(Mesh);
	Outfit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Видимость — как у тела шаблона: свой игрок своё тело не видит (только тень), другие — видят.
	Outfit->SetOwnerNoSee(Body->bOwnerNoSee);
	Outfit->bCastHiddenShadow = true;
	Outfit->RegisterComponent();
	Character->AddInstanceComponent(Outfit);
	Outfit->SetLeaderPoseComponent(Body);

	// Манекен остаётся источником анимации, но не рисуется.
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Body->SetVisibility(false, false);
}
