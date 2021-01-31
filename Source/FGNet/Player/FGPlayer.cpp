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
#include "FGPlayerSettings.h"
#include "../Debug/UI/FGNetDebugWidget.h"
#include "Net/UnrealNetwork.h"
#include "../FGRocket.h"
#include "../FGPickup.h"
#include "Kismet/GameplayStatics.h"
#include "Interfaces/IAnalyticsProvider.h"


const static float MaxMoveDeltaTime = 0.125f;

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

	Health = DefaultHealth;
	ServerHealth = Health;

	MovementComponent->SetUpdatedComponent(CollisionComponent);

	CreateDebugWidget();
	if (DebugMenuInstance != nullptr)
	{
		DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	}

	SpawnRockets();

	OriginalMeshOffset = MeshComponent->GetRelativeLocation();
	OriginalMeshRotation = MeshComponent->GetRelativeRotation();
}

void AFGPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FireCooldownElapsed -= DeltaTime;

	if (!ensure(PlayerSettings != nullptr))
		return;
	FFGFrameMovement FrameMovement = MovementComponent->CreateFrameMovement();

	if (IsLocallyControlled())
	{
		ClientTimeStamp += DeltaTime;

		const float MaxVelocity = PlayerSettings->MaxVelocity;
		const float Friction = IsBraking() ? PlayerSettings->BrakingFriction : PlayerSettings->Friction;
		const float Alpha = FMath::Clamp(FMath::Abs(MovementVelocity / (PlayerSettings->MaxVelocity * 0.75f)), 0.0f, 1.0f);
		const float TurnSpeed = FMath::InterpEaseOut(0.0f, PlayerSettings->TurnSpeedDefault, Alpha, 5.0f);
		const float TurnDirection = (MovementVelocity > 0.0f) ? Turn : -Turn;

		Yaw += (TurnDirection * TurnSpeed) * DeltaTime;
		FQuat WantedFacingDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		MovementComponent->SetFacingRotation(WantedFacingDirection, 10.5f);

		AddMovementVelocity(DeltaTime);
		MovementVelocity *= FMath::Pow(Friction, DeltaTime);

		MovementComponent->ApplyGravity();
		FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
		MovementComponent->Move(FrameMovement);

		// Compress the data before sending to the Server
		MovementToUpdate.NetLocation = GetActorLocation();
		MovementToUpdate.NetForward = Forward;
		MovementToUpdate.NetYaw = FMath::RoundToInt(GetActorRotation().Yaw * 256.0f / 360.0f) & 0xFF;// uint8
		MovementToUpdate.NetTime = ClientTimeStamp;

		Server_SendMovement(MovementToUpdate);

		/* week 1 - movment assignment
		Server_SendLocationAndRotation(StartLocation, StartRotation, DeltaTime);*/
		/* week2 replicate data example
		Server_SendYaw(MovementComponent->GetFacingRotation().Yaw);*/
	}
	else
	{
		const float Friction = IsBraking() ? PlayerSettings->BrakingFriction : PlayerSettings->Friction;
		MovementVelocity *= FMath::Pow(Friction, DeltaTime);
		FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
		MovementComponent->Move(FrameMovement);

		if (bPerformNetWorkSmoothing)
		{
			const FVector NewRelativeLocation = FMath::VInterpTo(MeshComponent->GetRelativeLocation(), OriginalMeshOffset, LastCorrectionDelta, 1.75f);
			MeshComponent->SetRelativeLocation(NewRelativeLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}

		/* teacher's week 2 implementation for using replicated life time props for replicating data*/
		//const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), ReplicatedLocation, DeltaTime, 10.0f);
		//SetActorLocation(NewLocation);
		//MovementComponent->SetFacingRotation(FRotator(0.0f, ReplicatedYaw, 0.0f), 7.0f);
		//SetActorRotation(MovementComponent->GetFacingRotation());
		// week 1 movement assignment
		/*SetActorLocation(FMath::VInterpTo(StartLocation, MovementToUpdate.Location, MovementToUpdate.Time, SmoothTransitionSpeed));
		SetActorRotation(FMath::RInterpTo(StartRotation, MovementToUpdate.Rotation, MovementToUpdate.Time, SmoothTransitionSpeed));*/
	}
}

void AFGPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (bIsDead)
	{
		return;
	}

	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Accelerate"), this, &AFGPlayer::Handle_Accelerate);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AFGPlayer::Handle_Turn);

	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Pressed, this, &AFGPlayer::Handle_BrakePressed);
	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Released, this, &AFGPlayer::Handle_BrakeReleased);

	PlayerInputComponent->BindAction(TEXT("DebugMenu"), IE_Pressed, this, &AFGPlayer::Handle_DebugMenuPressed);

	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AFGPlayer::Handle_FirePressed);
}

int32 AFGPlayer::GetPing() const
{
	if (GetPlayerState())
	{
		return static_cast<int32>(GetPlayerState()->GetPing());
	}
	return 0;
}

#pragma region Week3 - Improve Movement
void AFGPlayer::Server_SendMovement_Implementation(const FGNetMovement& MovementData)
{
	Multicast_SendMovement(MovementData);
}

void AFGPlayer::Multicast_SendMovement_Implementation(const FGNetMovement& MovementData)
{
	if (!IsLocallyControlled())
	{
		Forward = MovementData.NetForward;
		const float DeltaTime = FMath::Min(MovementData.NetTime - ClientTimeStamp, MaxMoveDeltaTime);
		ClientTimeStamp = MovementData.NetTime;
		AddMovementVelocity(DeltaTime);
		////Decompress
		FRotator DecompressdRotation = FRotator(0.0f, (float)MovementData.NetYaw * 360.0f / 256.0f, 0.0f);
		MovementComponent->SetFacingRotation(DecompressdRotation);

		const FVector DeltaDiff = MovementData.NetLocation - GetActorLocation();
		if (DeltaDiff.SizeSquared() > FMath::Square(40.0f))
		{
			if (bPerformNetWorkSmoothing)
			{
				const FScopedPreventAttachedComponentMove PreventMeshMove(MeshComponent);
				MovementComponent->UpdatedComponent->SetWorldLocation(MovementData.NetLocation, false, nullptr, ETeleportType::TeleportPhysics);
				LastCorrectionDelta = DeltaTime;
			}
			else
			{
				SetActorLocation(MovementData.NetLocation);
			}
		}
	}
}

void AFGPlayer::AddMovementVelocity(float DeltaTime)
{
	if (!ensure(PlayerSettings != nullptr))
	{
		return;
	}

	const float MaxVelocity = PlayerSettings->MaxVelocity;
	const float Acceleration = PlayerSettings->Acceleration;
	MovementVelocity += Forward * Acceleration * DeltaTime;
	MovementVelocity = FMath::Clamp(MovementVelocity, -MaxVelocity, MaxVelocity);
}
#pragma endregion

void AFGPlayer::Server_SendLocationAndRotation_Implementation(const FVector& LocationToSend, const FRotator& RotationToSend, float DeltaTime)
{
	Multicast_SendLocationAndRotation(LocationToSend, RotationToSend, DeltaTime);
	//ReplicatedLocation = LocationToSend;
	//ReplicatedYaw = RotationToSend.Yaw;
}

void AFGPlayer::Multicast_SendLocationAndRotation_Implementation(const FVector& LocationToSend, const FRotator& RotationToSend, float DeltaTime)
{
	if (!IsLocallyControlled())
	{

	}
}

// Week2 - Replicate Data example
void AFGPlayer::Server_SendYaw_Implementation(float NewYaw)
{
	ReplicatedYaw = NewYaw;
}

#pragma region Week2 - Pickup and Rocket

void AFGPlayer::Server_TakeDamage_Implementation(float DamageAmout)
{
	if (bIsDead)
	{
		return;
	}
	ServerHealth -= DamageAmout;
	if (ServerHealth < Health)
		Multicast_TakeDamage(DamageAmout);
}

void AFGPlayer::Multicast_TakeDamage_Implementation(float DamageAmout)
{
	if ((Health -= DamageAmout) <= 0.0f)
	{
		bIsDead = true;
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		DetachFromControllerPendingDestroy();
		SetLifeSpan(1.0f);
	}

	OnHealthChanged(Health);
}

void AFGPlayer::OnPickup(AFGPickup* Pickup)
{
	if (IsLocallyControlled())
	{
		Server_OnPickup(Pickup);
	}
}

void AFGPlayer::Client_OnPickupRockets_Implementation(int32 PickedUpRockets, AFGPickup* Pickup)
{
	Pickup->ObjectHasBeenPickedUp();
	NumRockets += PickedUpRockets;
	BP_OnNumRocketsChanged(NumRockets);
}

void AFGPlayer::Server_OnPickup_Implementation(AFGPickup* Pickup)
{
	ServerNumRockets += Pickup->NumRockets;
	Client_OnPickupRockets(Pickup->NumRockets, Pickup);
}

int32 AFGPlayer::GetNumActiveRockets() const
{
	int32 NumActive = 0;
	for (AFGRocket* Rocket : RocketInstances)
	{
		if (!Rocket->IsFree())
			NumActive++;
	}
	return NumActive;
}

void AFGPlayer::Handle_FirePressed()
{
	FireRocket();
}

void AFGPlayer::FireRocket()
{
	if (FireCooldownElapsed > 0.0f)
		return;

	if (NumRockets <= 0 && !bUnlimitedRockets)
		return;

	if (GetNumActiveRockets() >= MaxActiveRockets)
		return;

	AFGRocket* NewRocket = GetFreeRocket();

	if (!ensure(NewRocket != nullptr))
		return;

	FireCooldownElapsed = PlayerSettings->FireCooldown;

	if (GetLocalRole() >= ROLE_AutonomousProxy)
	{
		if (HasAuthority())
		{
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
		}
		else // if we are local but not the host
		{
			NumRockets--;
			NewRocket->StartMoving(GetActorForwardVector(), GetRocketStartLocation());
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
		}
	}
}

void AFGPlayer::Server_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation)
{
	if ((ServerNumRockets - 1) < 0 && !bUnlimitedRockets)
	{
		Client_RemoveRocket(NewRocket);
	}
	else
	{
		//Rocket shooting direction based on player's facing direciton
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(RocketFacingRotation.Yaw, GetActorForwardVector().Rotation().Yaw) * 0.5f; // 0.5f is small offset
		const FRotator NewFacingRotation = RocketFacingRotation + FRotator(0.0f, DeltaYaw, 0.0f);
		ServerNumRockets--;
		Multicast_FireRocket(NewRocket, RocketStartLocation, NewFacingRotation);
	}
}

void AFGPlayer::Multicast_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation)
{
	if (!ensure(NewRocket != nullptr))
		return;

	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		NewRocket->ApplyCorrection(RocketFacingRotation.Vector());
	}
	else
	{
		NumRockets--;
		NewRocket->StartMoving(RocketFacingRotation.Vector(), RocketStartLocation);
	}
	BP_OnNumRocketsChanged(NumRockets);
}

void AFGPlayer::Client_RemoveRocket_Implementation(AFGRocket* RocketToRemove)
{
	RocketToRemove->MakeFree();
}

void AFGPlayer::Cheat_IncreaseRockets(int32 InNumRockets)
{
	if (IsLocallyControlled())
		NumRockets += InNumRockets;
}

FVector AFGPlayer::GetRocketStartLocation() const
{
	const FVector StartLoc = GetActorLocation() + GetActorForwardVector() * 100.0f;
	return StartLoc;
}

AFGRocket* AFGPlayer::GetFreeRocket() const
{
	for (AFGRocket* Rocket : RocketInstances)
	{
		if (Rocket == nullptr)
			continue;

		if (Rocket->IsFree())
			return Rocket;
	}
	return nullptr;
}

void AFGPlayer::SpawnRockets()
{
	if (HasAuthority() && RocketClass != nullptr)
	{
		const int32 RocketCache = 8;

		for (int32 Index = 0; Index < RocketCache; ++Index)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SpawnParams.ObjectFlags = RF_Transient; // won't save in the level
			SpawnParams.Instigator = this;
			SpawnParams.Owner = this;
			AFGRocket* NewRocketInstance = GetWorld()->SpawnActor<AFGRocket>(RocketClass, GetActorLocation(), GetActorRotation(), SpawnParams);
			NewRocketInstance->SetInstigator(GetController());
			RocketInstances.Add(NewRocketInstance);
		}
	}
}
#pragma endregion

#pragma region Week2 - DebugMenu
void AFGPlayer::ShowDebugMenu()
{
	CreateDebugWidget();

	if (DebugMenuInstance == nullptr)
		return;

	DebugMenuInstance->SetVisibility(ESlateVisibility::Visible);
	DebugMenuInstance->BP_OnShowWidiget();
}

void AFGPlayer::HideDebugMenu()
{
	if (DebugMenuInstance == nullptr)
		return;

	DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	DebugMenuInstance->BP_OnHideWidget();
}

void AFGPlayer::Handle_DebugMenuPressed()
{
	bShowDebugMenu = !bShowDebugMenu;

	if (bShowDebugMenu)
		ShowDebugMenu();
	else
		HideDebugMenu();
}

void AFGPlayer::CreateDebugWidget()
{
	if (DebugMenuClass == nullptr)
		return;

	if (!IsLocallyControlled())
		return;

	if (DebugMenuInstance == nullptr)
	{
		DebugMenuInstance = CreateWidget<UFGNetDebugWidget>(GetWorld(), DebugMenuClass);
		DebugMenuInstance->AddToViewport();
	}
}
#pragma endregion

void AFGPlayer::Handle_Accelerate(float Value)
{
	if (Value > 0)
		bIsForward = true;
	else if (Value < 0)
		bIsForward = false;

	Forward = Value;
}

void AFGPlayer::Handle_Turn(float Value)
{
	if (!bIsForward)
		Value *= -1;
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

// replicate data example - must have
void AFGPlayer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFGPlayer, ReplicatedYaw);
	DOREPLIFETIME(AFGPlayer, ReplicatedLocation);
	DOREPLIFETIME(AFGPlayer, RocketInstances);
	DOREPLIFETIME(AFGPlayer, NumRockets);
	DOREPLIFETIME(AFGPlayer, Health);
	DOREPLIFETIME(AFGPlayer, bIsDead);

}

void AFGPlayer::ChangeMaterialColors(FColor RandomColor)
{
	UMaterialInterface* MeshCompMaterial = MeshComponent->GetMaterial(0);
	MeshComponent->CreateDynamicMaterialInstance(0, MeshCompMaterial)->SetVectorParameterValue("Color", RandomColor);
}






