// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FGRocket.generated.h"

class UStaticMeshComponent;
class UParticleSystem;
class UDamageType;

UCLASS()
class FGNET_API AFGRocket : public AActor
{
	GENERATED_BODY()
	
public:	
	AFGRocket();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	void StartMoving(const FVector& Forward, const FVector& InStartLocation);
	void ApplyCorrection(const FVector& Forward);

	FORCEINLINE bool IsFree() const { return bIsFree; }

	void Explode(FVector Location);
	void MakeFree();

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	TSubclassOf<UDamageType> DamageType;

	float ExplosionDamage = 20.0f;

	AController* ShootInstigator;
	FORCEINLINE void SetInstigator(AController* InstigatedController) { ShootInstigator = InstigatedController; }

	void ApplyDamage(AActor* HitActor);

private:
	void SetRocketVisibility(bool bVisible);

	FCollisionQueryParams CachedCollisionQueryParams;

	UPROPERTY(EditAnywhere, Category = VFX)
	UParticleSystem* Explosion = nullptr;

	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	UStaticMeshComponent* MeshComponent = nullptr;

	UPROPERTY(EditAnywhere, Category = Debug)
	bool bDebugDrawCorrection = true;

	FVector OriginalFacingDirection = FVector::ZeroVector;

	FVector FacingRotationStart = FVector::ZeroVector;
	FQuat FacingRotationCorrection = FQuat::Identity;

	FVector RocketStartLocation = FVector::ZeroVector;

	float LifeTime = 2.0f;
	float LifeTimeElapsed = 0.0f;

	float DistanceMoved = 0.0f;

	UPROPERTY(EditAnywhere)
	float MovementVelocity = 1300.0f;

	bool bIsFree = true;
};
