// DarcSliceItems.h
// Предметы среза. Отдельные классы нужны, чтобы считыватель/охранник/слоты понимали,
// что именно у игрока в руках. Модель — слот DarcAssetSettings с тем же именем.
#pragma once

#include "CoreMinimal.h"
#include "CarryableItem.h"
#include "Components/TextRenderComponent.h"
#include "DarcSliceItems.generated.h"

/**
 * Надпись с кодом выезда (цифры, не слова — String Table не нужна). Обновляется сама,
 * когда с сервера приходит сид выезда, поэтому у всех игроков одинаковая.
 */
UCLASS(ClassGroup = (DARC), meta = (BlueprintSpawnableComponent))
class DARK_API UDarcCodeTextComponent : public UTextRenderComponent
{
	GENERATED_BODY()

public:
	UDarcCodeTextComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Code")
	FName CodeKey = TEXT("CatalogCode");

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};

/** Карта доступа: на обороте — служебная маркировка с 4 цифрами кода каталога. */
UCLASS()
class DARK_API ADarcKeycard : public ACarryableItem
{
	GENERATED_BODY()

public:
	ADarcKeycard();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Keycard")
	TObjectPtr<UDarcCodeTextComponent> BackMarking;

protected:
	virtual void BeginPlay() override;
};

/** Носитель с архивом — ключевой предмет задания. */
UCLASS()
class DARK_API ADarcDataDrive : public ACarryableItem
{
	GENERATED_BODY()
};

/** Интерфейсный модуль — без него «EXTERNAL STORAGE NOT READY». */
UCLASS()
class DARK_API ADarcInterfaceModule : public ACarryableItem
{
	GENERATED_BODY()
};
