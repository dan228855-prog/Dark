// DarcVoiceLog.h
// Голосовая запись (диктофон, автоответчик, кассета): E — прослушать. Звук идёт из самой
// записи, реплики — субтитрами у тех, кто рядом. Записи — это голоса людей, которые работали
// в здании до нас: никакой прямой разгадки, только обрывки.
// Разметка: DARC.VoiceLog, Log=<Id>, Lines=<сколько реплик>. Тексты — ST_UI:
//   VoiceLog_<Id>_Speaker — кто говорит, VoiceLog_<Id>_1..N — реплики.
// Звук — слот VoiceLog_<Id> в DarcAssetSettings (пока не записан — помехи VoiceLogPlaceholder).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DarcAssetSettings.h"
#include "DarcVoiceLog.generated.h"

class UStaticMeshComponent;
class UAudioComponent;

UCLASS()
class DARK_API ADarcVoiceLog : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADarcVoiceLog();

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
	FDarcVisualSpec VisualSpec;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Id записи: ключи текста VoiceLog_<Id>_..., слот звука VoiceLog_<Id>. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "VoiceLog")
	FName LogId;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "VoiceLog")
	int32 NumLines = 1;

	/** Кто рядом (см) — слышит запись и видит субтитры. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VoiceLog")
	float HearRadius = 1500.f;

	/** Сервер: запустить (по E или командой консоли). */
	void ServerPlay();

	// --- IInteractable ---
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	bool bPlaying = false;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlaySound();

	/** Сервер: следующая реплика субтитром, по окончании всех — запись снова можно включить. */
	void SayNextLine();

	FText LineText(int32 Index) const;

	int32 NextLine = 0;
	FTimerHandle LineTimer;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Playback;
};
