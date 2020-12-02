// Fill out your copyright notice in the Description page of Project Settings.


#include "FGMovementStatics.h"
#include "Components/SceneComponent.h"

FFGFrameMovement::FFGFrameMovement(AActor* InActor)
{
	StartLocation = InActor->GetActorLocation();
}

FFGFrameMovement::FFGFrameMovement(USceneComponent* InSceneComponent)
{
	StartLocation = InSceneComponent->GetComponentLocation();
}

void FFGFrameMovement::AddDelta(const FVector& InDelta)
{
	MovementDelta += InDelta;
}