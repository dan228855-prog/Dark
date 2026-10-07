// DarcVoiceLog.cpp
#include "DarcVoiceLog.h"
#include "DarcGameState.h"
#include "DarcGameplayLibrary.h"
#include "DarcNoiseSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADarcVoiceLog::ADarcVoiceLog()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ADarcVoiceLog::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADarcVoiceLog, VisualSpec, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ADarcVoiceLog, LogId, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ADarcVoiceLog, NumLines, COND_InitialOnly);
	DOREPLIFETIME(ADarcVoiceLog, bPlaying);
}

void ADarcVoiceLog::BeginPlay()
{
	Super::BeginPlay();
	VisualSpec.ApplyTo(Visual);
}

bool ADarcVoiceLog::CanInteract_Implementation(AActor* Interactor) const
{
	return !bPlaying && !LogId.IsNone();
}

void ADarcVoiceLog::OnInteract_Implementation(AActor* Interactor)
{
	ServerPlay();
}

FText ADarcVoiceLog::GetInteractionPrompt_Implementation() const
{
	return UDarcGameplayLibrary::UIText(TEXT("VoiceLog_Play"));
}

FText ADarcVoiceLog::LineText(int32 Index) const
{
	return UDarcGameplayLibrary::UIText(*FString::Printf(TEXT("VoiceLog_%s_%d"), *LogId.ToString(), Index + 1));
}

void ADarcVoiceLog::ServerPlay()
{
	if (!HasAuthority() || bPlaying || LogId.IsNone())
	{
		return;
	}
	bPlaying = true;
	NextLine = 0;
	Multicast_PlaySound();
	// Первая реплика — после щелчка и шипения плёнки.
	GetWorldTimerManager().SetTimer(LineTimer, this, &ADarcVoiceLog::SayNextLine, 0.8f, false);
}

void ADarcVoiceLog::Multicast_PlaySound_Implementation()
{
	UDarcAssetSettings::PlaySound(this, TEXT("RadioClick"), GetActorLocation(), 0.6f);
	const FName Slot(*FString::Printf(TEXT("VoiceLog_%s"), *LogId.ToString()));
	USoundBase* Sound = UDarcAssetSettings::FindSound(UDarcAssetSettings::HasSound(Slot) ? Slot : FName(TEXT("VoiceLogPlaceholder")));
	if (Playback)
	{
		Playback->Stop();
	}
	// Заглушка (помехи) — тише: это фон под субтитры, пока голос не записан.
	const float Volume = UDarcAssetSettings::HasSound(Slot) ? 1.f : 0.25f;
	Playback = Sound ? UGameplayStatics::SpawnSoundAtLocation(this, Sound, GetActorLocation(), FRotator::ZeroRotator, Volume, 1.f, 0.f,
		UDarcAssetSettings::GetDefaultAttenuation(Sound)) : nullptr;
	UDarcNoiseSubsystem::Report(this, GetActorLocation(), 0.3f);
}

void ADarcVoiceLog::SayNextLine()
{
	ADarcGameState* GS = GetWorld()->GetGameState<ADarcGameState>();
	if (!GS || NextLine >= NumLines)
	{
		bPlaying = false;
		return;
	}
	const FText Line = LineText(NextLine);
	const FText Speaker = UDarcGameplayLibrary::UIText(*FString::Printf(TEXT("VoiceLog_%s_Speaker"), *LogId.ToString()));
	// Длительность — по длине реплики: успеть прочитать.
	const float Duration = FMath::Clamp(Line.ToString().Len() * 0.065f, 2.5f, 9.f);
	GS->Say(Speaker, Line, Duration, GetActorLocation(), HearRadius);
	++NextLine;
	GetWorldTimerManager().SetTimer(LineTimer, this, &ADarcVoiceLog::SayNextLine, Duration + 0.3f, false);
}
