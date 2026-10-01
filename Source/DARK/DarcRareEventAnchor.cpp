// DarcRareEventAnchor.cpp
#include "DarcRareEventAnchor.h"

ADarcRareEventAnchor::ADarcRareEventAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Реплицируется, чтобы ссылку на якорь можно было передать клиентам в RPC.
	// Состояния у него нет, поэтому трафика почти нет.
	bReplicates = true;
	SetReplicateMovement(false);
	// Всегда релевантен: иначе у далёкого клиента ссылка на якорь в RPC придёт пустой
	// (например, «шаги» в комнате, из которой игрок уже ушёл далеко).
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}
