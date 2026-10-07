// DarcVitalsComponent.h
// Здоровье и выносливость игрока + статусы для HUD.
// Здоровье — на сервере: урон (ApplyDamage / падение с высоты), медленное восстановление
// после паузы, ноль — смерть через ADarcGameMode::KillPlayer. Выносливость — бег на Shift:
// тратится на бегу, восстанавливается шагом/стоя; пусто — бег недоступен, пока не наберётся порог.
// Статусы (страх, кровотечение, яд, инфекция) — только список для HUD: источников пока нет.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarcVitalsComponent.generated.h"

class ACharacter;

UCLASS(ClassGroup = (DARC), meta = (BlueprintSpawnableComponent))
class DARK_API UDarcVitalsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarcVitalsComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals")
	float MaxStamina = 100.f;

	/** Восстановление здоровья, ед/с, — через RegenDelay секунд после последнего урона. */
	UPROPERTY(EditAnywhere, Category = "Vitals")
	float HealthRegen = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Vitals")
	float RegenDelay = 10.f;

	/** Бег: множитель скорости, расход и восстановление выносливости, ед/с. */
	UPROPERTY(EditAnywhere, Category = "Vitals")
	float SprintSpeedFactor = 1.6f;

	UPROPERTY(EditAnywhere, Category = "Vitals")
	float StaminaDrain = 18.f;

	UPROPERTY(EditAnywhere, Category = "Vitals")
	float StaminaRegen = 12.f;

	/** После полного выдоха бег снова доступен с этого уровня выносливости. */
	UPROPERTY(EditAnywhere, Category = "Vitals")
	float SprintResumeStamina = 30.f;

	/** Падение: урон начинается со скорости приземления (см/с), ед. урона на каждые 100 см/с сверх. */
	UPROPERTY(EditAnywhere, Category = "Vitals")
	float FallDamageSpeed = 1100.f;

	UPROPERTY(EditAnywhere, Category = "Vitals")
	float FallDamagePer100 = 9.f;

	UFUNCTION(BlueprintPure, Category = "Vitals")
	float GetHealth01() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Vitals")
	float GetStamina01() const { return MaxStamina > 0.f ? Stamina / MaxStamina : 0.f; }

	/** Сколько секунд назад был урон (для вспышки по краям экрана). */
	float GetSecondsSinceDamage() const;

	const TArray<FName>& GetStatuses() const { return Statuses; }

	/** Сервер: включить/выключить статус (Fear, Bleeding, Poison, Infection). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Vitals")
	void SetStatus(FName Status, bool bActive);

	/** Своя машина: зажат ли Shift. Отправляет на сервер. */
	void SetSprintPressed(bool bPressed);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	float Health = 100.f;

	UPROPERTY(Replicated)
	float Stamina = 100.f;

	UPROPERTY(Replicated)
	bool bSprintPressed = false;

	UPROPERTY(Replicated)
	bool bExhausted = false;

	/** Серверное время последнего урона (реплицируется — для вспышки у владельца). */
	UPROPERTY(Replicated)
	float LastDamageTime = -100.f;

	UPROPERTY(Replicated)
	TArray<FName> Statuses;

	UFUNCTION(Server, Reliable)
	void Server_SetSprint(bool bPressed);

	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	void ApplyHealthDamage(float Amount);
	bool IsSprinting() const;

	float BaseWalkSpeed = -1.f;
	bool bDead = false;
};
