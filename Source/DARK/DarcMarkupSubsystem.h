// DarcMarkupSubsystem.h
// Разметка карты тегами: автор строит уровень в редакторе из обычных моделей, а игровые
// объекты отмечает тегами актора (Details → Actor → Tags). При старте игры каждый
// помеченный актор превращается в настоящий игровой объект с той же моделью и на том же месте.
//
// Формат тегов (полная памятка — docs/MAP_MARKUP.md):
//   DARC.<Тип>[.<Флаг>...]   — что это, например DARC.Door.Locked, DARC.FuseBox.NoFuse
//   <Ключ>=<Значение>        — параметры, например Id=ServerDoor, Circuit=Server, Door=ServerDoor
// Списки — через запятую: Lines=File_VerifyNote,File_MainStorage.
// Тексты — ключами String Table ST_UI (Title=Screen_CatalogTitle), не строками.
//
// Сеть: игровые объекты создаёт только сервер (они реплицируются). Модель берётся у исходного
// актора карты (FDarcVisualSpec::CopyFrom) — он есть на карте у каждого игрока, поэтому
// по сети передаётся только ссылка; исходный актор у каждого скрывается.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DarcMarkupSubsystem.generated.h"

/** Разобранная разметка одного актора карты. */
struct FDarcMarkup
{
	TWeakObjectPtr<AActor> Source;
	FName Type;
	TSet<FString> Flags;          // в нижнем регистре
	TMap<FString, FString> Params; // ключ в нижнем регистре

	bool Has(const TCHAR* Flag) const { return Flags.Contains(FString(Flag).ToLower()); }
	FString Get(const TCHAR* Key, const FString& Default = FString()) const;
	FName GetName(const TCHAR* Key, FName Default = NAME_None) const;
	float GetFloat(const TCHAR* Key, float Default) const;
	TArray<FString> GetList(const TCHAR* Key) const;
};

UCLASS()
class DARK_API UDarcMarkupSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Есть ли на карте разметка DARC.* (тогда серый срез кодом не строится). */
	static bool HasMarkup(const UWorld* World);

	/** Разобрать теги актора. false — актор не размечен. */
	static bool ParseMarkup(const AActor* Actor, FDarcMarkup& Out);

	/** Игровой объект, созданный из разметки с этим Id (сервер). */
	UFUNCTION(BlueprintPure, Category = "DARC|Markup")
	AActor* FindById(FName Id) const;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	/** У каждой машины: скрыть исходные акторы, повесить надписи с кодом. */
	void ApplyLocalMarkup();

	/** Сервер: создать игровые объекты по разметке. */
	void ConvertMarkup();

	AActor* SpawnFor(const FDarcMarkup& Markup, FTransform& OutTransform);
	void Configure(AActor* Actor, const FDarcMarkup& Markup);
	void Warn(const FDarcMarkup& Markup, const FString& Message) const;

	template <class T>
	T* Ref(const FDarcMarkup& Markup, const TCHAR* Key) const;

	UPROPERTY()
	TMap<FName, TObjectPtr<AActor>> ById;

	bool bConverted = false;
};
