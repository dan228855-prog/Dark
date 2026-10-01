// DarcRoomVolume.h
// Помечает помещение для WorldMemory. Ставится в уровень (или процедурным
// генератором) по одному на комнату. Сервер по перекрытию отмечает, какой
// игрок вошёл/вышел. Клиенты ничего не считают.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarcRoomVolume.generated.h"

class UBoxComponent;

UCLASS()
class DARK_API ADarcRoomVolume : public AActor
{
	GENERATED_BODY()

public:
	ADarcRoomVolume();

	/** Уникальный ID комнаты в рамках выезда, например "ServerRoom". Используется в памяти и в условиях редких событий. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	FName RoomId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UBoxComponent> Bounds;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);
};
