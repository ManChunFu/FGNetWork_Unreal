// Fill out your copyright notice in the Description page of Project Settings.


#include "FGPlayer.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerState.h"
#include "../Components/FGMovementComponent.h"
#include "../FGMovementStatics.h"
#include "Components/SceneComponent.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
AFGPlayer::AFGPlayer()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	RootComponent = CollisionComponent;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->bInheritYaw = false;
	SpringArmComponent->SetupAttachment(CollisionComponent);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent);

	MovementComponent = CreateDefaultSubobject<UFGMovementComponent>(TEXT("MovementComponent"));

	SetReplicateMovement(false);

}

void AFGPlayer::BeginPlay()
{
	Super::BeginPlay();

	MovementComponent->SetUpdatedComponent(CollisionComponent);

	CurrentMovement.Location = GetActorLocation();
	CurrentMovement.Rotation = GetActorRotation();
	MovementToUpdate.Location = CurrentMovement.Location;
	MovementToUpdate.Rotation = CurrentMovement.Rotation;
}

void AFGPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FRotator StartRotation = GetActorRotation();
	FVector StartLocation = GetActorLocation();

	if (!IsLocallyControlled())
	{
		SetActorLocation(FMath::VInterpTo(StartLocation, MovementToUpdate.Location, MovementToUpdate.Time, SmoothTransitionSpeed));
		SetActorRotation(FMath::RInterpTo(StartRotation, MovementToUpdate.Rotation, MovementToUpdate.Time, SmoothTransitionSpeed));
	}
	else
	{
		const float Friction = IsBraking() ? BrakingFriction : DefaultFriction;
		const float Alpha = FMath::Clamp(FMath::Abs(MovementVelocity / (MaxVelocity * 0.75f)), 0.0f, 1.0f);
		const float TurnSpeed = FMath::InterpEaseOut(0.0f, TurnSpeedDefault, Alpha, 5.0f);
		const float MovementDirection = MovementVelocity > 0.0f ? Turn : -Turn;

		Yaw += (MovementDirection * TurnSpeed) * DeltaTime;
		FQuat WantedFacingDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		MovementComponent->SetFacingRotation(WantedFacingDirection);

		FFGFrameMovement FrameMovement = MovementComponent->CreateFrameMovement();

		MovementVelocity += Forward * Acceleration * DeltaTime;
		MovementVelocity = FMath::Clamp(MovementVelocity, -MaxVelocity, MaxVelocity);
		MovementVelocity *= FMath::Pow(Friction, DeltaTime);

		MovementComponent->ApplyGravity();
		FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
		MovementComponent->Move(FrameMovement);

		if (CurrentMovement.Location != StartLocation || CurrentMovement.Rotation != StartRotation)
		{
			CurrentMovement.Location = StartLocation;
			CurrentMovement.Rotation = StartRotation;
			Server_SendLocationAndRotation(StartLocation, StartRotation, DeltaTime);
		}
	}
}

void AFGPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Accelerate"), this, &AFGPlayer::Handle_Accelerate);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AFGPlayer::Handle_Turn);

	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Pressed, this, &AFGPlayer::Handle_BrakePressed);
	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Released, this, &AFGPlayer::Handle_BrakeReleased);
}

int32 AFGPlayer::GetPing() const
{
	if (GetPlayerState())
	{
		return static_cast<int32>(GetPlayerState()->GetPing());
	}

	return 0;
}


void AFGPlayer::Server_SendLocationAndRotation_Implementation(const FVector& LocationToSend, const FRotator& RotationToSend, float DeltaTime)
{
	Multicast_SendLocationAndRotation(LocationToSend, RotationToSend, DeltaTime);
}

void AFGPlayer::Multicast_SendLocationAndRotation_Implementation(const FVector& LocationToSend, const FRotator& RotationToSend, float DeltaTime)
{
	if (!IsLocallyControlled())
	{
		MovementToUpdate.Location = LocationToSend;
		MovementToUpdate.Rotation = RotationToSend;
		MovementToUpdate.Time = DeltaTime;
	}
}

void AFGPlayer::Handle_Accelerate(float Value)
{
	Forward = Value;
}

void AFGPlayer::Handle_Turn(float Value)
{
	Turn = Value;
}

void AFGPlayer::Handle_BrakePressed()
{
	bBrake = true;
}

void AFGPlayer::Handle_BrakeReleased()
{
	bBrake = false;
}






