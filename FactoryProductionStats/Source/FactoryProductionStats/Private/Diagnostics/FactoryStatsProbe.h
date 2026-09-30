// SPDX-License-Identifier: 0BSD
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FGBuildableSubsystem.h"
#include "FactoryStatsProbe.generated.h"

// One-shot diagnostic. Native Actor requires no Blueprint asset and never saves.
UCLASS(NotBlueprintable, Transient)
class AFactoryStatsProbe final : public AActor, public IFGFactoryTickHandlerInterface
{
    GENERATED_BODY()
public:
    AFactoryStatsProbe();
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
    void PreFactoryTick(AFGBuildableSubsystem* Subsystem, float DeltaTime) override;
private:
    TWeakObjectPtr<AFGBuildableSubsystem> RegisteredSubsystem;
    bool Captured = false;
};
