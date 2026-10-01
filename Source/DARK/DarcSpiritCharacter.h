// DarcSpiritCharacter.h
// «Дух» погибшего игрока (решение автора: смерть настоящая, погибший — дух-спектатор).
//
// Что умеет (и только это):
// - мигать светом: моргнуть устройством, у которого есть питание (через электросистему);
// - щёлкнуть рубильником щитка (включить/выключить приборы через электросистему);
// - толкнуть мелкий физический предмет (полтергейст).
// Может и помогать, и мешать. Особого зрения нет. Живые игроки духа не видят.
//
// Эскалация: чем больше смертей за кампанию и выше уровень, тем чаще и сильнее
// он может действовать (GetIntensity) — к уровням 6–7 дух становится настойчивым.
//
// Сеть: это обычный Character в режиме полёта — движение предсказывается клиентом и
// проверяется сервером, поэтому сервер знает, где дух на самом деле. Реплицируется только
// владельцу (другим его не видно и не нужно). Действия — Server RPC с проверкой
// дистанции, перезарядки и типа цели на сервере.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/NetSerialization.h"
#include "DarcSpiritCharacter.generated.h"

class UCameraComponent;
class ADarcFuseBox;

UCLASS()
class DARK_API ADarcSpiritCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADarcSpiritCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spirit")
	TObjectPtr<UCameraComponent> Camera;

	/** Дальность воздействия, см. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spirit", meta = (ClampMin = "100"))
	float AbilityRange = 1200.f;

	/** Перезарядка при минимальной силе, сек. С ростом силы — короче. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spirit", meta = (ClampMin = "0.5"))
	float BaseCooldown = 12.f;

	/** Импульс толчка при силе 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spirit", meta = (ClampMin = "0"))
	float BasePushImpulse = 500.f;

	/** Тяжелее этого — не сдвинуть (только мелочь), кг. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spirit", meta = (ClampMin = "0.1"))
	float MaxPushMass = 15.f;

	/** Время сервера, когда снова можно действовать (для индикатора в UI духа). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spirit")
	float CooldownEndTime = 0.f;

	/** Сила духа 0.5–2: растёт со смертями за кампанию и уровнем. Считается на сервере. */
	UFUNCTION(BlueprintPure, Category = "Spirit")
	float GetIntensity() const;

	// --- Действия: вызывать из ввода (Blueprint/Enhanced Input) на клиенте духа ---

	UFUNCTION(BlueprintCallable, Category = "Spirit")
	void TryFlicker();

	UFUNCTION(BlueprintCallable, Category = "Spirit")
	void TryToggleBreaker();

	UFUNCTION(BlueprintCallable, Category = "Spirit")
	void TryPush();

	/** Что сейчас под прицелом духа (для подсветки цели в UI духа). */
	UFUNCTION(BlueprintPure, Category = "Spirit")
	AActor* TraceTarget() const;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void Server_Flicker(AActor* Target);

	UFUNCTION(Server, Reliable)
	void Server_ToggleBreaker(ADarcFuseBox* FuseBox);

	UFUNCTION(Server, Reliable)
	void Server_Push(AActor* Target, FVector_NetQuantizeNormal Direction);

	/** Сервер: можно ли действовать по этой цели сейчас. */
	bool ValidateAction(const AActor* Target) const;
	void StartCooldown();

	/** Отклик у владельца (звук «у духа получилось», UI). */
	UFUNCTION(Client, Unreliable)
	void Client_ActionResult(bool bSuccess);

	UFUNCTION(BlueprintImplementableEvent, Category = "Spirit")
	void OnActionResult(bool bSuccess);
};
