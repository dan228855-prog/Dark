// DarcPlayerOutfitComponent.cpp
#include "DarcPlayerOutfitComponent.h"
#include "DarcAssetSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "DARKCharacter.h"

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

	Outfit = NewObject<USkeletalMeshComponent>(Character, TEXT("OutfitMesh"));
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

	// Вид от первого лица: та же модель поверх рук шаблона (их анимация — источник позы).
	// Видна только самому игроку; голова (камера внутри неё) и ноги скрыты.
	const ADARKCharacter* FirstPersonCharacter = Cast<ADARKCharacter>(Character);
	USkeletalMeshComponent* Arms = FirstPersonCharacter ? FirstPersonCharacter->GetFirstPersonMesh() : nullptr;
	if (!Arms)
	{
		return;
	}
	OutfitFirstPerson = NewObject<USkeletalMeshComponent>(Character, TEXT("OutfitFirstPersonMesh"));
	OutfitFirstPerson->SetupAttachment(Arms);
	OutfitFirstPerson->SetSkeletalMeshAsset(Mesh);
	OutfitFirstPerson->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
}
