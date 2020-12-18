// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FGPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EFGPickupTypes : uint8
{
	Rocket,
	Health
};

UCLASS()
class FGNET_API AFGPickup : public AActor
{
	GENERATED_BODY()
	
public:	
	AFGPickup();

protected:	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleDefaultsOnly, Category = Collision)
	USphereComponent* SphereComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(EditAnywhere)
	EFGPickupTypes PickupType = EFGPickupTypes::Rocket;

	UPROPERTY(EditAnywhere)
	int32 NumRockets = 5;

	UPROPERTY(EditAnywhere)
	float ReActivateTime = 5.0f;

	void ObjectHasBeenPickedUp();
private:
	FVector CachedMeshRelativeLocation = FVector::ZeroVector;
	FTimerHandle ReActivateHandle;

	UFUNCTION()
	void ReActivatePickup();

	UFUNCTION()
	void OverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 otherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	bool bPickedUp = false;
};
