// DarcTool.h
// Инструмент игрока — обычный переносимый предмет с типом. Каждый инструмент работает в
// нескольких механиках:
//   Лом     — взломать запертую дверь (громко, ломает замок, может порвать кабель за дверью),
//             сорвать решётку/панель (громко, сразу).
//   Ключ    — починить повреждённую линию, тихо открутить решётку/панель (по винту за нажатие).
//   Тестер  — показать состояние контура на щитке/линии (есть ли напряжение, где обрыв).
//   Рация   — в руке или в кармане: голос напарника с рацией слышен по радио, а не только рядом.
// Устройства сами смотрят, что у игрока в руках (GetHeldTool), — отдельной клавиши нет.
#pragma once

#include "CoreMinimal.h"
#include "CarryableItem.h"
#include "DarcTool.generated.h"

UENUM(BlueprintType)
enum class EDarcToolType : uint8
{
	None,
	Crowbar,
	Wrench,
	Tester,
	Radio
};

UCLASS()
class DARK_API ADarcTool : public ACarryableItem
{
	GENERATED_BODY()

public:
	ADarcTool();

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Tool")
	EDarcToolType ToolType = EDarcToolType::Crowbar;

	/** Какой инструмент у игрока в руке (None — пустая рука или не инструмент). */
	static EDarcToolType GetHeldTool(const AActor* Holder);

	/** Есть ли у игрока такой инструмент — в руке или в кармане. */
	static bool HasTool(const AActor* Holder, EDarcToolType Type);

	/** Тип по короткому имени из разметки (Crowbar / Wrench / Tester / Radio). */
	static EDarcToolType ParseType(const FString& Name);

	/** Ключ String Table с названием инструмента (Name_Crowbar …). */
	static const TCHAR* NameKey(EDarcToolType Type);

	/** Локальная пешка этой машины держит такой инструмент (для подсказок в HUD). */
	static bool LocalPlayerHolds(const UObject* WorldContextObject, EDarcToolType Type);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
