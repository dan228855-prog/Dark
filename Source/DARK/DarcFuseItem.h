// DarcFuseItem.h
// Предохранитель — обычный переносимый предмет. Отдельный класс нужен только для того,
// чтобы щиток понимал, что у игрока в руках именно предохранитель.
#pragma once

#include "CoreMinimal.h"
#include "CarryableItem.h"
#include "DarcFuseItem.generated.h"

UCLASS()
class DARK_API ADarcFuseItem : public ACarryableItem
{
	GENERATED_BODY()
};
