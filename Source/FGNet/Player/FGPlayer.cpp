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
}

// Called every frame
void AFGPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsLocallyControlled())
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

		Server_SendLocationAndRotation(GetActorLocation(), GetActorRotation(), DeltaTime);
	}
	else
	{
		float TimeDiff = 0;
		while (!MovementsQueue.IsEmpty())
		{
			FGNetMovement NewNetMove;
			MovementsQueue.Dequeue(NewNetMove);

			TimeDiff = DeltaTime - NewNetMove.Time;
			if (DeltaTime - NewNetMove.Time > TimeDiff)
			{
				UE_LOG(LogTemp, Warning, TEXT("Skipping"), DeltaTime);
				continue;
			}

			UE_LOG(LogTemp, Warning, TEXT("%f"), TimeDiff);

			SetActorLocation(FMath::Lerp(GetActorLocation(), NewNetMove.Location, DeltaTime * 2.0f));
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), NewNetMove.Rotation, DeltaTime * 2.0f, Yaw));

			//SetActorLocationAndRotation(NewNetMove.Location, NewNetMove.Rotation);
		}
	}
}

// Called to bind functionality to input
void AFGPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Accelerate"), this, &AFGPlayer::Handle_Accelerate);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AFGPlayer::Handle_Turn);

	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Pressed, this, &AFGPlayer::Handle_BrakePressed);
	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Released, this, &AFGPlayer::Handle_BrakeReleased);
}

int32 AFGPlayer::GetPint() const
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
		MovementsQueue.Enqueue({ LocationToSend, RotationToSend, DeltaTime});
		//SetActorLocation(FMath::VInterpTo(GetActorLocation(), LocationToSend, DeltaTime, MovementVelocity));
		//SetActorRotation(FMath::RInterpTo(GetActorRotation(), RotationToSend, DeltaTime, Yaw));
		//SetActorLocationAndRotation(LocationToSend, RotationToSend);
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





