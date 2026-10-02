// DarcGameplayLibrary.cpp
#include "DarcGameplayLibrary.h"
#include "TaskManagerComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
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

void UDarcGameplayLibrary::GetAimViewPoint(const APawn* Pawn, FVector& OutLocation, FRotator& OutRotation)
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;
	if (!Pawn)
	{
		return;
	}
	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (PC && PC->IsLocalController())
	{
		PC->GetPlayerViewPoint(OutLocation, OutRotation); // камера — ровно то, что видно на экране
		return;
	}
	Pawn->GetActorEyesViewPoint(OutLocation, OutRotation);
}

bool UDarcGameplayLibrary::TraceAim(const APawn* Pawn, float Range, FHitResult& OutHit)
{
	if (!Pawn || !Pawn->GetWorld())
	{
		return false;
	}
	FVector Start;
	FRotator Rotation;
	GetAimViewPoint(Pawn, Start, Rotation);
	const FVector End = Start + Rotation.Vector() * Range;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DarcAim), false, Pawn);
	if (Pawn->GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params))
	{
		return true;
	}
	return Pawn->GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(5.f), Params);
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
