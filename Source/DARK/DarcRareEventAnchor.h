// DarcRareEventAnchor.h
// Точка в уровне, где может «сыграть» редкое событие: терминал, фотография,
// динамик, место для фантомной двери... Расставляется в уровне или генератором.
// Вся подача (текст из String Table, звук, свет, материал) — в Blueprint-наследнике.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarcRareEventTypes.h"
#include "DarcAssetSettings.h"
#include "DarcRareEventAnchor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class AInteractableDoor;

/** Встроенное поведение якоря (работает без Blueprint; Blueprint может заменить). */
UENUM(BlueprintType)
enum class EDarcAnchorBehavior : uint8
{
	SoundOnly     UMETA(DisplayName = "Только звук события"),
	LightOn       UMETA(DisplayName = "Зажечь свет (окно)"),
	Levitate      UMETA(DisplayName = "Приподнять предмет (левитация)"),
	VanishingRoom UMETA(DisplayName = "Комната исчезает (дверь запирается, предмет смещается)")
};

UCLASS(Blueprintable)
class DARK_API ADarcRareEventAnchor : public AActor
{
	GENERATED_BODY()

public:
	ADarcRareEventAnchor();

	/** Какая модель у объекта (реплицируется при появлении, применяется у каждого игрока). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Visual")
	FDarcVisualSpec VisualSpec;

	/** Видимая модель (по умолчанию серая коробка, см. DarcAssetSettings). Нужна и для трейса взгляда. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Какие типы событий умеет показывать этот якорь (совпадает с EventType в таблице). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	TArray<FName> SupportedEventTypes;

	/** В какой комнате стоит якорь (RoomId из ADarcRoomVolume). Нужно для событий «по комнате». */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent")
	FName RoomId;

	/** Выключенный якорь сервер не выбирает (например, терминал разбит или занят сюжетной сценой). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RareEvent")
	bool bEnabled = true;

	bool SupportsType(FName EventType) const { return bEnabled && SupportedEventTypes.Contains(EventType); }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent|Native")
	EDarcAnchorBehavior Behavior = EDarcAnchorBehavior::SoundOnly;

	/** Свет для LightOn (выключен до события). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RareEvent|Native")
	TObjectPtr<UPointLightComponent> EventLight;

	/** Предмет для Levitate / VanishingRoom (кружка, шкаф). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent|Native")
	TObjectPtr<AActor> TargetActor;

	/** Если TargetActor не задан (например, локальная мелочь) — искать ближайший актор с этим тегом. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent|Native")
	FName TargetTag;

	/** Дверь комнаты для VanishingRoom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent|Native")
	TObjectPtr<AInteractableDoor> RoomDoor;

	/** Сколько длится эффект (левитация, запертая комната), сек. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RareEvent|Native", meta = (ClampMin = "1"))
	float EffectDuration = 40.f;

	/**
	 * Сервер: событие выбрано и сейчас будет показано. Здесь — серверные изменения мира
	 * из Blueprint (например, заспавнить лишний предмет для «НЕПРАВИЛЬНОЕ КОЛИЧЕСТВО»).
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "RareEvent")
	void OnEventFiredOnServer(const FDarcRareEventPayload& Payload);

	/**
	 * Каждый клиент, которому положено это увидеть/услышать (включая хоста).
	 * Здесь — только локальная подача: звук, текст на экране терминала, мигание.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "RareEvent")
	void PlayEventLocally(const FDarcRareEventPayload& Payload);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void EndServerEffect();
	void EndLocalEffect();
	AActor* ResolveTarget();

	FTimerHandle ServerEffectTimer;
	FTimerHandle LocalEffectTimer;
	FVector TargetOriginalLocation = FVector::ZeroVector;
};
