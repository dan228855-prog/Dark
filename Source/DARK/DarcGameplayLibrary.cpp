// DarcGameplayLibrary.cpp
#include "DarcGameplayLibrary.h"
#include "TaskManagerComponent.h"
#include "Math/RandomStream.h"
#include "Misc/Crc.h"

FString UDarcGameplayLibrary::GetMissionCode(const UObject* WorldContextObject, FName Key, int32 Digits)
{
	const UTaskManagerComponent* Tasks = UTaskManagerComponent::GetTaskManager(WorldContextObject);
	const int32 Seed = Tasks ? Tasks->GetMissionSeed() : 0;

	// Свой поток случайности на каждый ключ: коды разных замков не связаны между собой.
	const uint32 KeyHash = FCrc::StrCrc32(*Key.ToString());
	FRandomStream Stream(static_cast<int32>(KeyHash ^ static_cast<uint32>(Seed)));

	FString Code;
	for (int32 i = 0; i < FMath::Clamp(Digits, 1, 12); ++i)
	{
		Code.AppendChar(static_cast<TCHAR>(TEXT('0') + Stream.RandRange(0, 9)));
	}
	return Code;
}

FText UDarcGameplayLibrary::UIText(FName Key)
{
	return FText::FromStringTable(TEXT("ST_UI"), Key.ToString());
}
