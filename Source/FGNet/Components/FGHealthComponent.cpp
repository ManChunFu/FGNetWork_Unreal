// Fill out your copyright notice in the Description page of Project Settings.


#include "FGHealthComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UFGHealthComponent::UFGHealthComponent()
{
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));

	DefaultHealth = 100.0f;

	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UFGHealthComponent::BeginPlay()
{
	Super::BeginPlay();


	Health = DefaultHealth;
}


void UFGHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UFGHealthComponent, Health, COND_OwnerOnly);
}




