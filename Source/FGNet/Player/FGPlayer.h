// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FGPlayer.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UFGMovementComponent;
class UStaticMeshComponent;
class USphereComponent;
class UFGPlayerSettings;
class UFGNetDebugWidget;
class AFGRocket;
class AFGPickup;
class UMaterialInterface;

USTRUCT()
struct FGNetMovement
{
	GENERATED_USTRUCT_BODY()

public:
	FGNetMovement() = default;

	UPROPERTY()
	FVector_NetQuantize NetLocation;

	UPROPERTY()
	float NetForward;

	UPROPERTY()
	uint8 NetYaw;

	UPROPERTY()
	float NetTime;
};

UCLASS()
class FGNET_API AFGPlayer : public APawn
{
	GENERATED_BODY()

public:
	AFGPlayer();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditAnywhere, Category = Setting)
	UFGPlayerSettings* PlayerSettings = nullptr;

	UPROPERTY(EditAnywhere, Category = Movement, meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float BrakingFriction = 0.001f;

	UFUNCTION(BlueprintPure)
	bool IsBraking() const { return bBrake; }

	UFUNCTION(BlueprintPure)
	int32 GetPing() const;

	// calling in very frame is better to do unreliable
	UFUNCTION(Server, Unreliable)
	void Server_SendLocationAndRotation(const FVector& LocationToSend, const FRotator& RotationToSend, float DeltaTime);

	UFUNCTION(NetMulticast, Unreliable) // if choose Reliable -> very expensive in this function
	void Multicast_SendLocationAndRotation(const FVector& LocationToSend, const FRotator& RotationToSend, float DeltaTime);

#pragma region Week3 - Improve Movement / Prediction
	float ClientTimeBetweenUpdates;
	float LerpRatio;
	bool bNeedUpdate;

	UFUNCTION(Server, Unreliable)
	void Server_SendMovement(const FGNetMovement& MovementData);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SendMovement(const FGNetMovement& MovementData);
#pragma endregion

#pragma region Week2 - DebugMenu (for showing net conenctions options)
	UPROPERTY(editAnywhere, Category = Debug)
	TSubclassOf<UFGNetDebugWidget> DebugMenuClass;

	void ShowDebugMenu();
	void HideDebugMenu();
#pragma endregion

#pragma region Week2 - Replicate data example
	UFUNCTION(Server, Unreliable)
	void Server_SendYaw(float NewYaw);
#pragma endregion

#pragma region Week2 - Pickup and Rocket
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Health")
	float DefaultHealth = 100;

	UPROPERTY(ReplicatedUsing=OnHealthChanged, VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float Health;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	bool bIsDead = false;

	UFUNCTION(Server, Reliable)
	void Server_TakeDamage(float DamageAmout);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_TakeDamage(float DamageAmout);

	UFUNCTION(BlueprintImplementableEvent, Category = "Events")
	void OnHealthChanged(float CurrentHealth);

	float ServerHealth = 0.0f;


	void OnPickup(AFGPickup* Pickup);

	UFUNCTION(Server, Reliable)
	void Server_OnPickup(AFGPickup* Pickup);

	UFUNCTION(NetMulticast, Reliable)
	void Client_OnPickupRockets(int32 PickedUpRockets, AFGPickup* Pickup);

	UFUNCTION(BlueprintPure)
	int32 GetNumRockets() const { return NumRockets; }

	UFUNCTION(BlueprintImplementableEvent, Category = Player, meta = (DisplayName = "On Num Rockets Changed"))
	void BP_OnNumRocketsChanged(int32 NewNumrockets);

	int32 GetNumActiveRockets() const;

	void FireRocket();

	void SpawnRockets();
#pragma endregion
private:
	FGNetMovement MovementToUpdate;
	FGNetMovement CurrentMovement;
	const float SmoothTransitionSpeed = 2.5f;

#pragma region Week 2 - Pickup and Rocket
	int32 ServerNumRockets = 0;
	
	UPROPERTY(ReplicatedUsing=BP_OnNumRocketsChanged)
	int32 NumRockets = 0;

	FVector GetRocketStartLocation() const;

	AFGRocket* GetFreeRocket() const;

	UFUNCTION(Server, Reliable)
	void Server_FireRocket(AFGRocket* Newrocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FireRocket(AFGRocket* Newrocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation, int32 rocketFired);

	UFUNCTION(Client, Reliable)
	void Client_RemoveRocket(AFGRocket* RocketToRemove);

	UFUNCTION(BlueprintCallable)
	void Cheat_IncreaseRockets(int32 InNumRockets);

	UPROPERTY(Replicated, Transient)
	TArray<AFGRocket*> RocketInstances;

	UPROPERTY(EditAnywhere, Category = Weapon)
	TSubclassOf<AFGRocket> RocketClass;

	int32 MaxActiveRockets = 3;

	float FireCooldownElapsed = 0.0f;

	UPROPERTY(EditAnywhere, Category = Weapon)
	bool bUnlimitedRockets = false;

	void Handle_FirePressed();
#pragma endregion

#pragma region Week2 - DebugMenu
	void Handle_DebugMenuPressed();
	void CreateDebugWidget();

	UPROPERTY(Transient)
	UFGNetDebugWidget* DebugMenuInstance = nullptr;

	bool bShowDebugMenu = false;
#pragma endregion

#pragma region Week2 - Repicate data examples
	UPROPERTY(Replicated)// if value is changed on the server, it'll replicate to each client
	float ReplicatedYaw = 0.0f;

	UPROPERTY(Replicated)
	FVector ReplicatedLocation = FVector::ZeroVector;
#pragma endregion

	void Handle_Accelerate(float Value);
	void Handle_Turn(float Value);
	void Handle_BrakePressed();
	void Handle_BrakeReleased();

	bool bIsForward = true;
	float Forward = 0.0f;
	float Turn = 0.0f;

	float MovementVelocity = 0.0f;
	float Yaw = 0.0f;

	bool bBrake = false;

#pragma region Week3 - Improve movement
	void AddMovementVelocity(float DeltaTime);

	float ClientTimeStamp = 0.0f;
	float ServerTimeStamp = 0.0f;
	float LastCorrectionDelta = 0.0f;

	UPROPERTY(EditAnywhere, Category = Netork)
	bool bPerformNetWorkSmoothing = true;

	FVector OriginalMeshOffset = FVector::ZeroVector;
	FRotator OriginalMeshRotation = FRotator::ZeroRotator;
#pragma endregion

	UPROPERTY(VisibleDefaultsOnly, Category = Collision)
	USphereComponent* CollisionComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Camera)
	USpringArmComponent* SpringArmComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Camera)
	UCameraComponent* CameraComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Movement)
	UFGMovementComponent* MovementComponent;

	UFUNCTION(BlueprintCallable)
	void ChangeMaterialColors(FColor RandomColor);

};
