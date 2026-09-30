// SPDX-License-Identifier: 0BSD
#include "ProductionStatsPowerReader.h"
#include "FGCircuitSubsystem.h"
#include "FGPowerCircuit.h"
#include "FGPowerInfoComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

using namespace FactoryProductionStats;
PowerSnapshot FProductionStatsPowerReader::Capture(UWorld* World, double Time)
{
    check(IsInGameThread());
    auto* Subsystem = AFGCircuitSubsystem::Get(World);
    if (!IsValid(Subsystem) || !Subsystem->HasAuthority()) return {Time, {}, std::nullopt, std::nullopt};
    std::vector<CircuitSample> Circuits;
    std::vector<DeviceSample> Devices;
    // Read at PreFactoryTick; retain no circuit/component pointer beyond this call.
    // These are local live fields, not GetStats' once-per-second graph copies.
    for (const auto& Pair : Subsystem->mCircuits)
    {
        auto* Circuit = Cast<UFGPowerCircuit>(Pair.Value.Get());
        if (!IsValid(Circuit)) continue;
        const int32 Group = Circuit->GetCircuitGroupID();
        // Tag standalone IDs separately: temporary invalid group IDs never collapse
        // every disconnected circuit into a single network.
        const std::int64_t Network = Group >= 0 ? Group : -1 - static_cast<std::int64_t>(Circuit->GetCircuitID());
        Circuits.push_back({reinterpret_cast<std::uint64_t>(Circuit), Network, Circuit->IsFuseTriggered(),
            {Circuit->mPowerConsumed, Circuit->mPowerProduced, Circuit->mPowerProductionCapacity, Circuit->mMaximumPowerConsumption}});
        for (auto InfoPtr : Circuit->mPowerInfos)
        {
            auto* Info = InfoPtr.Get();
            if (!IsValid(Info) || !Info->IsConnected()) continue;
            auto* Owner = Info->GetOwner();
            DeviceSample Device;
            Device.token = reinterpret_cast<std::uint64_t>(Info);
            Device.owner = reinterpret_cast<std::uint64_t>(Owner);
            if (IsValid(Owner)) Device.buildingClass = TCHAR_TO_UTF8(*Owner->GetClass()->GetPathName());
            Device.consumptionMW = Info->GetActualConsumption();
            Device.boostMW = Info->GetActualProductionBoost();
            Device.productionMW = static_cast<double>(Info->GetBaseProduction()) + Info->GetRegulatedDynamicProduction() + Device.boostMW;
            Device.consumer = Info->GetMaximumTargetConsumption() > 0 || Info->GetTargetConsumption() > 0 || Device.consumptionMW != 0;
            Device.producer = Info->GetBaseProduction() > 0 || Info->GetDynamicProductionCapacity() > 0 || Info->GetProductionBoostFactor() > 0 || Device.productionMW != 0;
            if (auto* Battery = Info->GetBatteryInfo(); IsValid(Battery))
            {
                Device.producer = false; // Inactive storage is not a generator either.
                if (Battery->IsActive()) Device.battery = BatterySample{reinterpret_cast<std::uint64_t>(Battery), Battery->GetPowerInput(), Battery->GetPowerStore(), Battery->GetPowerStoreCapacity()};
            }
            Devices.push_back(std::move(Device));
        }
    }
    // T01 still has to establish circuit-local vs group assignment, boost ownership,
    // sampling phase and special-load coverage against the running game.
    return AggregatePower(Time, Circuits, Devices, false);
}
