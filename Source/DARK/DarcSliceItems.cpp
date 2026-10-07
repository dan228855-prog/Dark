// DarcSliceItems.cpp
#include "DarcSliceItems.h"
#include "DarcGameplayLibrary.h"
#include "DarcAssetSettings.h"
#include "Components/StaticMeshComponent.h"

UDarcCodeTextComponent::UDarcCodeTextComponent()
{
	// Раз в полсекунды хватает: код меняется только при старте выезда.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.5f;
	SetHorizontalAlignment(EHTA_Center);
	SetVerticalAlignment(EVRTA_TextCenter);
	SetWorldSize(6.f);
	SetTextRenderColor(FColor(30, 30, 30));
}

void UDarcCodeTextComponent::BeginPlay()
{
	Super::BeginPlay();
	// Цифры кода — неосвещаемым материалом: читаются и в тёмной кладовой, и в руке ночью
	// (обычный материал текста в темноте сливался с фоном).
	SetTextMaterial(UDarcAssetSettings::GetUnlitTextMaterial());
	// Без межбуквенного зазора цифры слипались («11» читалось как «Н»).
	SetHorizSpacingAdjust(0.12f * WorldSize);
}

void UDarcCodeTextComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	SetText(FText::FromString(UDarcGameplayLibrary::GetMissionCode(this, CodeKey, 4)));
}

ADarcKeycard::ADarcKeycard()
{
	BackMarking = CreateDefaultSubobject<UDarcCodeTextComponent>(TEXT("BackMarking"));
	BackMarking->SetupAttachment(Mesh);
	// Маркировка сверху карты (в серой версии): видна, когда карта лежит или в руках.
	// С настоящей моделью карты переложить на оборот.
	BackMarking->SetRelativeLocation(FVector(0.f, 0.f, 2.f));
	BackMarking->SetRelativeRotation(FRotator(90.f, 180.f, 0.f));
	// Корень (модель) масштабируется под размер карты — надпись не должна сплющиваться вместе с ним.
	BackMarking->SetUsingAbsoluteScale(true);

	// Карта в правой руке лицом к глазам — положение подобрано автором в игре (от кости ладони).
	bGripInBoneSpace = true;
	FirstPersonGripOffset = FVector(-8.f, 7.f, 6.f);
	FirstPersonGripRotation = FRotator(74.f, 119.f, -70.f);
}

void ADarcKeycard::BeginPlay()
{
	Super::BeginPlay(); // здесь применяется модель и масштаб корня

	// Корень карты масштабирован под её размер — смещение надписи пересчитываем,
	// чтобы цифры лежали на поверхности карты, а не внутри неё.
	const FVector Scale = Mesh->GetRelativeScale3D().ComponentMax(FVector(0.0001f));
	BackMarking->SetRelativeLocation(FVector(0.f, 0.f, 0.4f / Scale.Z));
	// Цифры — тёмно-синие, крупные (2.2 см на карте 5.4 см): мелкие 1.6 см в руке не читались.
	BackMarking->SetWorldSize(2.2f);
	BackMarking->SetHorizSpacingAdjust(0.3f);
	BackMarking->SetTextRenderColor(FColor(10, 20, 60));

	// Серая коробка вместо модели карты: светлая пластиковая лицевая сторона, чуть светится —
	// тёмные цифры на ней видны и ночью у КПП (на тёмно-сером кубе они пропадали).
	if (!UDarcAssetSettings::FindMesh(VisualSpec.Slot))
	{
		Mesh->SetMaterial(0, UDarcAssetSettings::MakeColorMaterial(this, FLinearColor(0.75f, 0.78f, 0.82f), 0.5f));
	}
}
