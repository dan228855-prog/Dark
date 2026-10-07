// DarcGrassScatter.cpp
#include "DarcGrassScatter.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

ADarcGrassScatter::ADarcGrassScatter()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ADarcGrassScatter::BeginPlay()
{
	Super::BeginPlay();
	Regenerate();
}

void ADarcGrassScatter::Clear()
{
	for (UHierarchicalInstancedStaticMeshComponent* Layer : GrassLayers)
	{
		if (Layer)
		{
			Layer->DestroyComponent();
		}
	}
	GrassLayers.Empty();
}

float ADarcGrassScatter::DistanceToPath(const FVector2D& P) const
{
	float Best = TNumericLimits<float>::Max();
	for (int32 i = 0; i + 1 < PathPoints.Num(); ++i)
	{
		const FVector2D A(PathPoints[i]), B(PathPoints[i + 1]);
		const FVector2D AB = B - A;
		const float T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1.f), 0.f, 1.f);
		Best = FMath::Min(Best, FVector2D::DistSquared(P, A + AB * T));
	}
	return FMath::Sqrt(Best);
}

void ADarcGrassScatter::Regenerate()
{
	Clear();
	UWorld* World = GetWorld();
	if (!World || Varieties.Num() == 0)
	{
		return;
	}

	// По слою на модель: без теней и коллизии, гаснет вдали.
	float TotalWeight = 0.f;
	for (const FDarcGrassVariety& Variety : Varieties)
	{
		UHierarchicalInstancedStaticMeshComponent* Layer = nullptr;
		if (Variety.Mesh && Variety.Weight > 0.f)
		{
			Layer = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
			Layer->SetupAttachment(RootComponent);
			Layer->SetStaticMesh(Variety.Mesh);
			Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Layer->SetCastShadow(false);
			Layer->SetReceivesDecals(false);
			Layer->bAffectDistanceFieldLighting = false;
			Layer->SetVisibleInRayTracing(false);
			Layer->SetCullDistances(CullStart, CullEnd);
			Layer->RegisterComponent();
			TotalWeight += Variety.Weight;
		}
		GrassLayers.Add(Layer);
	}
	if (TotalWeight <= 0.f)
	{
		return;
	}

	// Область перебора: дорога с полосой травы + дополнительные коробки.
	FBox Bounds(ForceInit);
	for (const FVector& P : PathPoints)
	{
		Bounds += P;
	}
	Bounds = Bounds.ExpandBy(RoadHalfWidth + BandWidth);
	for (const FBox& Area : Areas)
	{
		// У областей берём только план: высоту луча задаёт дорога (иначе луч стартует
		// из-под неба и упирается во что попало над картой).
		Bounds += FVector(Area.Min.X, Area.Min.Y, Bounds.IsValid ? Bounds.GetCenter().Z : GetActorLocation().Z);
		Bounds += FVector(Area.Max.X, Area.Max.Y, Bounds.GetCenter().Z);
	}
	if (!Bounds.IsValid)
	{
		return;
	}

	FRandomStream Random(Seed);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcGrass), false, this);
	TArray<TArray<FTransform>> Instances;
	Instances.SetNum(Varieties.Num());
	const FTransform ToLocal = GetActorTransform().Inverse();
	for (float X = Bounds.Min.X; X < Bounds.Max.X; X += Spacing)
	{
		for (float Y = Bounds.Min.Y; Y < Bounds.Max.Y; Y += Spacing)
		{
			// Случайные числа берём всегда, чтобы раскладка не зависела от того, что отсеялось.
			const FVector2D P(X + Random.FRandRange(0.f, Spacing), Y + Random.FRandRange(0.f, Spacing));
			const float Keep = Random.FRand();
			const float Pick = Random.FRand() * TotalWeight;
			const float Yaw = Random.FRandRange(0.f, 360.f);
			const float ScaleT = Random.FRand();

			bool bInArea = false;
			for (const FBox& Area : Areas)
			{
				bInArea |= P.X >= Area.Min.X && P.X <= Area.Max.X && P.Y >= Area.Min.Y && P.Y <= Area.Max.Y;
			}
			float Chance = bInArea ? 0.85f : 0.f;
			if (PathPoints.Num() >= 2)
			{
				const float D = DistanceToPath(P);
				if (D < RoadHalfWidth)
				{
					continue; // дорога
				}
				if (D < RoadHalfWidth + BandWidth)
				{
					// Густо у обочины, реже вглубь.
					Chance = FMath::Max(Chance, FMath::Lerp(1.f, 0.25f, (D - RoadHalfWidth) / BandWidth));
				}
			}
			if (Keep >= Chance)
			{
				continue;
			}
			bool bExcluded = false;
			for (const FBox& Box : Exclusions)
			{
				bExcluded |= P.X >= Box.Min.X && P.X <= Box.Max.X && P.Y >= Box.Min.Y && P.Y <= Box.Max.Y;
			}
			if (bExcluded)
			{
				continue;
			}

			// Только по земле (ландшафт/статика), не на скалах.
			FHitResult Hit;
			if (!World->LineTraceSingleByObjectType(Hit, FVector(P, Bounds.Max.Z + 1500.f), FVector(P, Bounds.Min.Z - 1500.f),
				FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				continue;
			}
			// Только ландшафт: на камнях, будке, заборе травы нет.
			if (Hit.ImpactNormal.Z < MaxSlopeNormalZ || Hit.ImpactPoint.Z < MinGroundZ || !Hit.GetActor() || !Hit.GetActor()->GetClass()->GetName().Contains(TEXT("Landscape")))
			{
				continue;
			}

			int32 Index = 0;
			float Acc = 0.f;
			for (; Index < Varieties.Num(); ++Index)
			{
				Acc += GrassLayers[Index] ? Varieties[Index].Weight : 0.f;
				if (Pick < Acc)
				{
					break;
				}
			}
			Index = FMath::Min(Index, Varieties.Num() - 1);
			const FDarcGrassVariety& Variety = Varieties[Index];
			const FVector Up = FMath::Lerp(FVector::UpVector, Hit.ImpactNormal, Variety.AlignToGround).GetSafeNormal();
			const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, Up);
			const FQuat Rotation = Tilt * FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
			const float Scale = FMath::Lerp(Variety.ScaleRange.X, Variety.ScaleRange.Y, ScaleT);
			const FTransform Placed(Rotation, Hit.ImpactPoint - Up * 3.f, FVector(Scale));
			Instances[Index].Add(Placed * ToLocal);
		}
	}
	for (int32 i = 0; i < GrassLayers.Num(); ++i)
	{
		if (GrassLayers[i] && Instances[i].Num() > 0)
		{
			GrassLayers[i]->AddInstances(Instances[i], false, false);
		}
	}
}
