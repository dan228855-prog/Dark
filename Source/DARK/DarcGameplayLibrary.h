// DarcGameplayLibrary.h
// Общие помощники для Blueprint и C++.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DarcGameplayLibrary.generated.h"

UCLASS()
class DARK_API UDarcGameplayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Код выезда (например, 4 цифры на обороте карты доступа). Выводится из сида выезда
	 * и ключа, поэтому одинаков на сервере и у всех клиентов и меняется от выезда к выезду.
	 * Пока выезд не начат (сид 0), возвращает стабильный код по одному ключу — удобно для тестов.
	 */
	UFUNCTION(BlueprintPure, Category = "DARC", meta = (WorldContext = "WorldContextObject"))
	static FString GetMissionCode(const UObject* WorldContextObject, FName Key, int32 Digits = 4);

	/** Текст для игрока из таблицы строк ST_UI по ключу (единственный путь текста из C++). */
	UFUNCTION(BlueprintPure, Category = "DARC")
	static FText UIText(FName Key);

	/** Строка → двоичный код UTF-8: байты по 8 бит через пробел, BytesPerLine байт в строке. */
	UFUNCTION(BlueprintPure, Category = "DARC")
	static FString EncodeBinary(const FString& Text, int32 BytesPerLine = 6);
};
