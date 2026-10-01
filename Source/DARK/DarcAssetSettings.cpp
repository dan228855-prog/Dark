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
		// Вписываем модель в нужную коробку целиком, сохраняя пропорции, и ставим на «пол» коробки.
		Component->SetStaticMesh(Mesh);
		const FBox Bounds = Mesh->GetBoundingBox();
		const FVector Size = Bounds.GetSize().ComponentMax(FVector(1.f));
		const float Scale = FMath::Min3(BoxSize.X / Size.X, BoxSize.Y / Size.Y, BoxSize.Z / Size.Z);
		Component->SetRelativeScale3D(FVector(Scale));
		if (!bIsRoot)
		{
			const FVector Center = Bounds.GetCenter();
			Component->SetRelativeLocation(FVector(-Center.X, -Center.Y, -Bounds.Min.Z) * Scale);
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

void UDarcAssetSettings::PlaySound(const UObject* WorldContextObject, FName Slot, const FVector& Location)
{
	if (USoundBase* Sound = FindSound(Slot))
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Sound, Location);
	}
}

UAudioComponent* UDarcAssetSettings::PlayLoopAttached(FName Slot, USceneComponent* AttachTo)
{
	USoundBase* Sound = FindSound(Slot);
	return (Sound && AttachTo) ? UGameplayStatics::SpawnSoundAttached(Sound, AttachTo) : nullptr;
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
