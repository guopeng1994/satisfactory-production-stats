// SPDX-License-Identifier: 0BSD
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProductionStatsPlayerComponent.generated.h"
class AFGCharacterPlayer;
class UEnhancedInputComponent;
class UInputComponent;
class APawn;
class UFGHealthComponent;
class UInputAction;
class UProductionStatsWidget;

UCLASS(NotBlueprintable, Transient)
class FACTORYPRODUCTIONSTATS_API UProductionStatsPlayerComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    static UProductionStatsPlayerComponent* Attach(class AFGPlayerController* Player);
    void BindInput(AFGCharacterPlayer* Character, UInputComponent* Input);
    void Toggle();
protected:
    void OnRegister() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void Close();
    UFUNCTION() void OnDeath(AActor* Actor);
    UFUNCTION() void OnPawnChanged(APawn* Previous, APawn* Next);
    UPROPERTY(Transient) TObjectPtr<UProductionStatsWidget> Window;
    UPROPERTY(Transient) TObjectPtr<UInputAction> ToggleAction;
    TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
    TWeakObjectPtr<UFGHealthComponent> BoundHealth;
};
