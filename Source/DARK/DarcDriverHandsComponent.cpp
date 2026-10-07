// DarcDriverHandsComponent.cpp
#include "DarcDriverHandsComponent.h"
#include "DarcIntroDrive.h"
#include "DARKCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TwoBoneIK.h"

namespace DarcDriverHands
{
	const FName Upper[2] = { TEXT("upperarm_l"), TEXT("upperarm_r") };
	const FName Lower[2] = { TEXT("lowerarm_l"), TEXT("lowerarm_r") };
	const FName Hand[2] = { TEXT("hand_l"), TEXT("hand_r") };
}

UDarcDriverHandsComponent::UDarcDriverHandsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// После анимации (поза манекена уже посчитана) и до отправки кадра на отрисовку.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bAutoActivate = true;
}

void UDarcDriverHandsComponent::Begin(ADarcIntroDrive* InCar)
{
	Car = InCar;
	ADARKCharacter* Character = Cast<ADARKCharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	AddLayer(Character->GetMesh(), false);
	AddLayer(Character->GetFirstPersonMesh(), true);

	// Руки от первого лица обычно рисуются «ужатыми» к камере — тогда они не совпали бы
	// с рулём. В машине рисуем их в обычном масштабе.
	if (UCameraComponent* Camera = Character->GetFirstPersonCameraComponent())
	{
		bSavedFirstPersonScale = Camera->bEnableFirstPersonScale;
		bSavedFirstPersonFov = Camera->bEnableFirstPersonFieldOfView;
		Camera->bEnableFirstPersonScale = false;
		Camera->bEnableFirstPersonFieldOfView = false;
		bChangedCamera = true;
	}
}

void UDarcDriverHandsComponent::AddLayer(USkeletalMeshComponent* Source, bool bHideHead)
{
	if (!Source || !Source->GetSkeletalMeshAsset())
	{
		return;
	}
	FPoseLayer Layer;
	Layer.Source = Source;
	UPoseableMeshComponent* Pose = NewObject<UPoseableMeshComponent>(GetOwner());
	Pose->SetupAttachment(Source);
	Pose->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset());
	Pose->SetVisibility(false);
	Pose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pose->RegisterComponent();
	if (bHideHead)
	{
		for (const FName Bone : { FName(TEXT("neck_01")), FName(TEXT("head")), FName(TEXT("thigh_l")), FName(TEXT("thigh_r")) })
		{
			if (Pose->GetBoneIndex(Bone) != INDEX_NONE)
			{
				Pose->HideBoneByName(Bone, EPhysBodyOp::PBO_None);
			}
		}
	}
	Layer.Pose = Pose;

	// Видимые модели, которые повторяли позу манекена, теперь берут её с копии.
	TArray<USkinnedMeshComponent*> Meshes;
	GetOwner()->GetComponents<USkinnedMeshComponent>(Meshes);
	for (USkinnedMeshComponent* Mesh : Meshes)
	{
		if (Mesh != Pose && Mesh->LeaderPoseComponent.Get() == Source)
		{
			Mesh->SetLeaderPoseComponent(Pose);
			Layer.Followers.Add(Mesh);
		}
	}
	Layers.Add(Layer);
}

void UDarcDriverHandsComponent::End()
{
	for (FPoseLayer& Layer : Layers)
	{
		for (const TWeakObjectPtr<USkinnedMeshComponent>& Follower : Layer.Followers)
		{
			if (Follower.IsValid() && Layer.Source.IsValid())
			{
				Follower->SetLeaderPoseComponent(Layer.Source.Get());
			}
		}
		if (Layer.Pose.IsValid())
		{
			Layer.Pose->DestroyComponent();
		}
	}
	Layers.Empty();
	if (bChangedCamera)
	{
		if (ADARKCharacter* Character = Cast<ADARKCharacter>(GetOwner()))
		{
			if (UCameraComponent* Camera = Character->GetFirstPersonCameraComponent())
			{
				Camera->bEnableFirstPersonScale = bSavedFirstPersonScale;
				Camera->bEnableFirstPersonFieldOfView = bSavedFirstPersonFov;
			}
		}
		bChangedCamera = false;
	}
	Car.Reset();
}

void UDarcDriverHandsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	End();
	Super::EndPlay(EndPlayReason);
}

void UDarcDriverHandsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Car.IsValid())
	{
		return;
	}
	for (FPoseLayer& Layer : Layers)
	{
		if (!Layer.Source.IsValid() || !Layer.Pose.IsValid())
		{
			continue;
		}
		UPoseableMeshComponent* Pose = Layer.Pose.Get();
		Pose->CopyPoseFromSkeletalComponent(Layer.Source.Get());
		SolveArm(Pose, -1.f);
		SolveArm(Pose, 1.f);
		Pose->RefreshBoneTransforms();
	}
}

void UDarcDriverHandsComponent::SolveArm(UPoseableMeshComponent* Pose, float Side) const
{
	using namespace DarcDriverHands;
	const int32 i = Side < 0.f ? 0 : 1;
	if (Pose->GetBoneIndex(Upper[i]) == INDEX_NONE || Pose->GetBoneIndex(Lower[i]) == INDEX_NONE || Pose->GetBoneIndex(Hand[i]) == INDEX_NONE)
	{
		return;
	}
	FTransform Root = Pose->GetBoneTransformByName(Upper[i], EBoneSpaces::WorldSpace);
	FTransform Joint = Pose->GetBoneTransformByName(Lower[i], EBoneSpaces::WorldSpace);
	FTransform End = Pose->GetBoneTransformByName(Hand[i], EBoneSpaces::WorldSpace);
	const FVector Grip = Car->GetHandGrip(Side);
	const FVector Elbow = Car->GetElbowHint(Side, Root.GetLocation(), Grip);
	AnimationCore::SolveTwoBoneIK(Root, Joint, End, Elbow, Grip, false, 1.0, 1.0);
	Pose->SetBoneTransformByName(Upper[i], Root, EBoneSpaces::WorldSpace);
	Pose->SetBoneTransformByName(Lower[i], Joint, EBoneSpaces::WorldSpace);
	Pose->SetBoneTransformByName(Hand[i], End, EBoneSpaces::WorldSpace);
}
