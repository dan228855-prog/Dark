// DarcTool.cpp
#include "DarcTool.h"
#include "DarcInventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ADarcTool::ADarcTool()
{
}

void ADarcTool::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcTool, ToolType, COND_InitialOnly);
}

EDarcToolType ADarcTool::GetHeldTool(const AActor* Holder)
{
	const ADarcTool* Tool = Cast<ADarcTool>(ACarryableItem::FindItemHeldBy(Holder));
	return Tool ? Tool->ToolType : EDarcToolType::None;
}

bool ADarcTool::HasTool(const AActor* Holder, EDarcToolType Type)
{
	if (GetHeldTool(Holder) == Type)
	{
		return true;
	}
	const UDarcInventoryComponent* Inventory = Holder ? Holder->FindComponentByClass<UDarcInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return false;
	}
	for (int32 i = 0; i < UDarcInventoryComponent::NumSlots; ++i)
	{
		const ADarcTool* Tool = Cast<ADarcTool>(Inventory->GetSlotItem(i));
		if (Tool && Tool->ToolType == Type)
		{
			return true;
		}
	}
	return false;
}

EDarcToolType ADarcTool::ParseType(const FString& Name)
{
	if (Name.Equals(TEXT("Wrench"), ESearchCase::IgnoreCase))
	{
		return EDarcToolType::Wrench;
	}
	if (Name.Equals(TEXT("Tester"), ESearchCase::IgnoreCase))
	{
		return EDarcToolType::Tester;
	}
	if (Name.Equals(TEXT("Radio"), ESearchCase::IgnoreCase))
	{
		return EDarcToolType::Radio;
	}
	return EDarcToolType::Crowbar;
}

const TCHAR* ADarcTool::NameKey(EDarcToolType Type)
{
	switch (Type)
	{
	case EDarcToolType::Crowbar: return TEXT("Name_Crowbar");
	case EDarcToolType::Wrench:  return TEXT("Name_Wrench");
	case EDarcToolType::Tester:  return TEXT("Name_Tester");
	case EDarcToolType::Radio:   return TEXT("Name_Radio");
	default:                     return TEXT("");
	}
}

bool ADarcTool::LocalPlayerHolds(const UObject* WorldContextObject, EDarcToolType Type)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC && PC->GetPawn() && GetHeldTool(PC->GetPawn()) == Type;
}
