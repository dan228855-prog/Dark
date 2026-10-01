// DarcAssetSettings.cpp
#include "DarcAssetSettings.h"
#include "DarcTaskTypes.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "GameFramework/Actor.h"

UDarcAssetSettings::UDarcAssetSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("DARC Assets");

	// Пути, по которым их создаёт Tools/darc_setup.py.
	SliceMission = TSoftObjectPtr<UDarcMissionDefinition>(FSoftObjectPath(TEXT("/Game/DARC/Data/DA_Mission_Slice.DA_Mission_Slice")));
	RareEventTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/DARC/Data/DT_RareEvents.DT_RareEvents")));
}

UStaticMesh* UDarcAssetSettings::FindMesh(FName Slot)
{
	const TSoftObjectPtr<UStaticMesh>* Found = Get()->Meshes.Find(Slot);
	return Found ? Found->LoadSynchronous() : nullptr;
}

UMaterialInterface* UDarcAssetSettings::FindMaterial(FName Slot)
{
	const TSoftObjectPtr<UMaterialInterface>* Found = Get()->Materials.Find(Slot);
	return Found ? Found->LoadSynchronous() : nullptr;
}

USoundBase* UDarcAssetSettings::FindSound(FName Slot)
{
	const TSoftObjectPtr<USoundBase>* Found = Get()->Sounds.Find(Slot);
	return Found ? Found->LoadSynchronous() : nullptr;
}

void UDarcAssetSettings::ApplyVisual(UStaticMeshComponent* Component, FName Slot, const FVector& BoxSize, FName MaterialSlot)
{
	if (!Component)
	{
		return;
	}

	// Корень актора (физическое тело) двигать нельзя — только масштаб, центр остаётся в начале координат.
	const bool bIsRoot = Component->GetOwner() && Component->GetOwner()->GetRootComponent() == Component;

	// Всё, что строится/меняется во время игры, должно быть подвижным (Static нельзя менять в рантайме).
	Component->SetMobility(EComponentMobility::Movable);

	UStaticMesh* Mesh = Slot.IsNone() ? nullptr : FindMesh(Slot);
	if (Mesh)
	{
		Component->SetStaticMesh(Mesh);
		const FBox Bounds = Mesh->GetBoundingBox();
		FVector MeshSize = Bounds.GetSize().ComponentMax(FVector(1.f));

		// Модель «лежит» не вдоль коробки (дверь шириной по X, а проём по Y; длинная лампа
		// вдоль Y, а коробка вдоль X) — поворачиваем на 90° по рыскания. Только у дочерних
		// компонентов без своего поворота: корень двигать нельзя.
		const auto IsElongated = [](float A, float B) { return A > B * 1.3f; };
		const bool bBoxAlongX = IsElongated(BoxSize.X, BoxSize.Y);
		const bool bBoxAlongY = IsElongated(BoxSize.Y, BoxSize.X);
		const bool bMeshAlongX = IsElongated(MeshSize.X, MeshSize.Y);
		const bool bMeshAlongY = IsElongated(MeshSize.Y, MeshSize.X);
		const bool bSwap = !bIsRoot && Component->GetRelativeRotation().IsNearlyZero()
			&& ((bBoxAlongX && bMeshAlongY) || (bBoxAlongY && bMeshAlongX));
		const FVector Target = bSwap ? FVector(BoxSize.Y, BoxSize.X, BoxSize.Z) : BoxSize; // в осях модели

		// Вписывание: общий масштаб — по самому большому размеру коробки (модель получает
		// задуманный рост: дверь — высоту проёма, лампа — длину), затем каждая ось
		// дотягивается до коробки, но не больше чем в 1.5 раза / не меньше чем вдвое,
		// чтобы пропорции не ломались. Раньше брался минимум по осям — и тонкая коробка
		// (8 см у двери) сжимала всю модель.
		const int32 MainAxis = Target.X >= Target.Y && Target.X >= Target.Z ? 0 : (Target.Y >= Target.Z ? 1 : 2);
		const float Uniform = Target[MainAxis] / MeshSize[MainAxis];
		FVector Scale3;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float Fill = Target[Axis] / (MeshSize[Axis] * Uniform);
			Scale3[Axis] = Uniform * FMath::Clamp(Fill, 0.5f, 1.5f);
		}
		Component->SetRelativeScale3D(Scale3);

		if (!bIsRoot)
		{
			// Центр модели — в центр коробки, низ — на её «пол».
			const FRotator Rotation = bSwap ? FRotator(0.f, 90.f, 0.f) : Component->GetRelativeRotation();
			const FVector Center = Bounds.GetCenter();
			const FVector Pivot(-Center.X * Scale3.X, -Center.Y * Scale3.Y, -Bounds.Min.Z * Scale3.Z);
			Component->SetRelativeRotation(Rotation);
			Component->SetRelativeLocation(Rotation.RotateVector(Pivot));
		}
	}
	else
	{
		// Серая коробка: куб движка 100×100×100 с центром в середине — поднимаем на полвысоты.
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		Component->SetStaticMesh(Cube);
		Component->SetRelativeScale3D(BoxSize / 100.f);
		if (!bIsRoot)
		{
			Component->SetRelativeLocation(FVector(0.f, 0.f, BoxSize.Z * 0.5f));
		}
	}

	if (UMaterialInterface* Material = MaterialSlot.IsNone() ? nullptr : FindMaterial(MaterialSlot))
	{
		for (int32 i = 0; i < Component->GetNumMaterials(); ++i)
		{
			Component->SetMaterial(i, Material);
		}
	}
	else if (!Mesh)
	{
		Component->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
	}
}

USoundBase* UDarcAssetSettings::FindSoundVariant(FName Slot)
{
	// Варианты одного звука: Slot, Slot_1 … Slot_8 — берём случайный (шаги не звучат «пулемётом»).
	const TMap<FName, TSoftObjectPtr<USoundBase>>& Map = Get()->Sounds;
	TArray<const TSoftObjectPtr<USoundBase>*, TInlineAllocator<9>> Found;
	if (const TSoftObjectPtr<USoundBase>* Base = Map.Find(Slot))
	{
		Found.Add(Base);
	}
	for (int32 i = 1; i <= 8; ++i)
	{
		if (const TSoftObjectPtr<USoundBase>* Variant = Map.Find(FName(*FString::Printf(TEXT("%s_%d"), *Slot.ToString(), i))))
		{
			Found.Add(Variant);
		}
	}
	return Found.Num() > 0 ? Found[FMath::RandRange(0, Found.Num() - 1)]->LoadSynchronous() : nullptr;
}

bool UDarcAssetSettings::HasSound(FName Slot)
{
	return Get()->Sounds.Contains(Slot) || Get()->Sounds.Contains(FName(*(Slot.ToString() + TEXT("_1"))));
}

USoundAttenuation* UDarcAssetSettings::GetDefaultAttenuation(USoundBase* Sound)
{
	// У звука свои настройки затухания (заданы в редакторе) — не трогаем.
	if (!Sound || Sound->AttenuationSettings)
	{
		return nullptr;
	}
	// Иначе — общее объёмное затухание: звук слышно с направления источника, громко рядом
	// (до 3 м) и тише до полной тишины к ~30 м. Без этого импортированные звуки играли
	// «в голове» одинаково громко отовсюду.
	static TStrongObjectPtr<USoundAttenuation> Attenuation;
	if (!Attenuation.IsValid())
	{
		Attenuation.Reset(NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("DarcDefaultAttenuation")));
		FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
		Settings.bAttenuate = true;
		Settings.bSpatialize = true;
		Settings.AttenuationShape = EAttenuationShape::Sphere;
		Settings.AttenuationShapeExtents = FVector(300.f, 0.f, 0.f);
		Settings.FalloffDistance = 2700.f;
		Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		Settings.bAttenuateWithLPF = true; // вдали глуше
		Settings.LPFRadiusMin = 600.f;
		Settings.LPFRadiusMax = 3000.f;
	}
	return Attenuation.Get();
}

void UDarcAssetSettings::PlaySound(const UObject* WorldContextObject, FName Slot, const FVector& Location, float Volume)
{
	if (USoundBase* Sound = FindSoundVariant(Slot))
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Sound, Location, FRotator::ZeroRotator,
			Volume, FMath::FRandRange(0.95f, 1.05f), 0.f, GetDefaultAttenuation(Sound));
	}
}

UAudioComponent* UDarcAssetSettings::PlayLoopAttached(FName Slot, USceneComponent* AttachTo)
{
	USoundBase* Sound = FindSound(Slot);
	return (Sound && AttachTo)
		? UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
			false, 1.f, 1.f, 0.f, GetDefaultAttenuation(Sound))
		: nullptr;
}

void FDarcVisualSpec::ApplyTo(UStaticMeshComponent* Component) const
{
	if (!Component || Size.IsNearlyZero())
	{
		return;
	}
	UDarcAssetSettings::ApplyVisual(Component, Slot, Size, Material);
	if (!Offset.IsNearlyZero())
	{
		Component->AddRelativeLocation(Offset);
	}
}
