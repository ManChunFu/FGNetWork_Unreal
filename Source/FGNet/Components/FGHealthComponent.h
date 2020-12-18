// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FGHealthComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FGNET_API UFGHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UFGHealthComponent();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "HealthComponent")
	class USphereComponent* SphereComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HealthComponent")
	float DefaultHealth;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "HealthComponent")
	float Health;

	virtual void BeginPlay() override;


};
