// DarcSliceItems.cpp
#include "DarcSliceItems.h"
#include "DarcGameplayLibrary.h"
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
}

void ADarcKeycard::BeginPlay()
{
	Super::BeginPlay(); // здесь применяется модель и масштаб корня

	// Корень карты масштабирован под её размер — смещение надписи пересчитываем,
	// чтобы цифры лежали на поверхности карты, а не внутри неё.
	const FVector Scale = Mesh->GetRelativeScale3D().ComponentMax(FVector(0.0001f));
	BackMarking->SetRelativeLocation(FVector(0.f, 0.f, 0.6f / Scale.Z));
}
