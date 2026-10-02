// DarcHintComponent.cpp
#include "DarcHintComponent.h"
#include "DarcGameplayLibrary.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UDarcHintComponent::UDarcHintComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDarcHintComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDarcHintComponent, NameKey);
	DOREPLIFETIME(UDarcHintComponent, HintKey);
}

FText UDarcHintComponent::GetDisplayText() const
{
	if (NameKey.IsNone())
	{
		return FText::GetEmpty();
	}
	const FText Name = UDarcGameplayLibrary::UIText(*NameKey.ToString());
	if (HintKey.IsNone())
	{
		return Name;
	}
	return FText::Format(UDarcGameplayLibrary::UIText(TEXT("HUD_HintFormat")), Name, UDarcGameplayLibrary::UIText(*HintKey.ToString()));
}

UDarcHintComponent* UDarcHintComponent::AddHint(AActor* Actor, FName NameKey, FName HintKey)
{
	if (!Actor || !Actor->HasAuthority() || NameKey.IsNone())
	{
		return nullptr;
	}
	UDarcHintComponent* Hint = Actor->FindComponentByClass<UDarcHintComponent>();
	if (!Hint)
	{
		Hint = NewObject<UDarcHintComponent>(Actor, TEXT("Hint"));
		Hint->SetIsReplicated(true);
		Hint->RegisterComponent();
		Actor->AddInstanceComponent(Hint);
	}
	Hint->NameKey = NameKey;
	Hint->HintKey = HintKey;
	return Hint;
}
