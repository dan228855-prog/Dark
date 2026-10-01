// DarcRareEventAnchor.h
// Точка в уровне, где может «сыграть» редкое событие: терминал, фотография,
// динамик, место для фантомной двери... Расставляется в уровне или генератором.
// Вся подача (текст из String Table, звук, свет, материал) — в Blueprint-наследнике.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarcRareEventTypes.h"
#include "DarcRareEventAnchor.generated.h"

UCLASS(Blueprintable)
class DARK_API ADarcRareEventAnchor : public AActor
{
	GENERATED_BODY()

public:
	ADarcRareEventAnchor();

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

	/**
	 * Сервер: событие выбрано и сейчас будет показано. Здесь — серверные изменения мира
	 * из Blueprint (например, заспавнить лишний предмет для «НЕПРАВИЛЬНОЕ КОЛИЧЕСТВО»).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "RareEvent")
	void OnEventFiredOnServer(const FDarcRareEventPayload& Payload);

	/**
	 * Каждый клиент, которому положено это увидеть/услышать (включая хоста).
	 * Здесь — только локальная подача: звук, текст на экране терминала, мигание.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "RareEvent")
	void PlayEventLocally(const FDarcRareEventPayload& Payload);
};
