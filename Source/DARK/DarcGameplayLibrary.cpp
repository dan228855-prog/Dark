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

FString UDarcGameplayLibrary::EncodeBinary(const FString& Text, int32 BytesPerLine)
{
	const FTCHARToUTF8 Utf8(*Text);
	FString Out;
	for (int32 i = 0; i < Utf8.Length(); ++i)
	{
		const uint8 Byte = static_cast<uint8>(Utf8.Get()[i]);
		for (int32 Bit = 7; Bit >= 0; --Bit)
		{
			Out.AppendChar((Byte >> Bit) & 1 ? TEXT('1') : TEXT('0'));
		}
		const bool bLineEnd = BytesPerLine > 0 && (i + 1) % BytesPerLine == 0;
		Out.AppendChar(bLineEnd ? TEXT('\n') : TEXT(' '));
	}
	return Out.TrimEnd();
}
