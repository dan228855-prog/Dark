// DarcDriverHandsComponent.h
// Руки водителя на руле: поза берётся из анимации вождения, а кисти каждый кадр
// «дотягиваются» до обода руля (IK из двух костей: плечо — локоть — кисть). Анимация из пака
// сделана под другую машину, без этого руки висели в воздухе.
//
// Как устроено: копия скелета (UPoseableMeshComponent) повторяет позу манекена, поверх неё
// правятся руки, и видимая модель (leader pose) берёт позу уже с копии. Так и для тела (его
// видят другие игроки), и для рук от первого лица. Работает у каждой машины сама (не по сети).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarcDriverHandsComponent.generated.h"

class USkeletalMeshComponent;
class UPoseableMeshComponent;
class USkinnedMeshComponent;
class ADarcIntroDrive;

UCLASS(ClassGroup = (DARC))
class DARK_API UDarcDriverHandsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarcDriverHandsComponent();

	/** Начать держать руль машины (вызывает ADarcIntroDrive при посадке водителя). */
	void Begin(ADarcIntroDrive* InCar);

	/** Отпустить руль: вернуть обычную позу и камеру. */
	void End();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FPoseLayer
	{
		TWeakObjectPtr<USkeletalMeshComponent> Source;
		TWeakObjectPtr<UPoseableMeshComponent> Pose;
		TArray<TWeakObjectPtr<USkinnedMeshComponent>> Followers;
	};

	void AddLayer(USkeletalMeshComponent* Source, bool bHideHead);
	void SolveArm(UPoseableMeshComponent* Pose, float Side) const;

	TWeakObjectPtr<ADarcIntroDrive> Car;
	TArray<FPoseLayer> Layers;
	bool bSavedFirstPersonScale = true;
	bool bSavedFirstPersonFov = true;
	bool bChangedCamera = false;
};
