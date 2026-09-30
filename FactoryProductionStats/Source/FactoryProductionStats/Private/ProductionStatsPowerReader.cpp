// SPDX-License-Identifier: 0BSD
#include "ProductionStatsPowerReader.h"
#include "FGCircuitSubsystem.h"
#include "FGPowerCircuit.h"
#include "FGPowerInfoComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

using namespace FactoryProductionStats;
DEFINE_LOG_CATEGORY_STATIC(LogProductionStatsPowerEvidence, Log, All);
PowerSnapshot FProductionStatsPowerReader::Capture(UWorld* World, double Time, bool LogEvidence)
{
    check(IsInGameThread());
    auto* Subsystem = AFGCircuitSubsystem::Get(World);
    if (!IsValid(Subsystem) || !Subsystem->HasAuthority())
    {
        if (LogEvidence) UE_LOG(LogProductionStatsPowerEvidence, Warning, TEXT("T01 native circuit registry unavailable; no known zero"));
        return {Time, {}, std::nullopt, std::nullopt};
    }
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
        if (LogEvidence)
        {
            FPowerCircuitStats Stats;
            Circuit->GetStats(Stats);
            UE_LOG(LogProductionStatsPowerEvidence, Display, TEXT("T01 circuit=%d group=%d fuse=%d infos=%d localConsumed=%.9g localProduced=%.9g localCapacity=%.9g localDemand=%.9g localBoost=%.9g statsConsumed=%.9g statsProduced=%.9g statsCapacity=%.9g statsDemand=%.9g statsBoost=%.9g statsLastTime=%.9g statsInterval=%.9g sampleTime=%.17g (raw native values; field scope pending validation)"),
                Circuit->GetCircuitID(), Group, Circuit->IsFuseTriggered(), Circuit->mPowerInfos.Num(),
                Circuit->mPowerConsumed, Circuit->mPowerProduced, Circuit->mPowerProductionCapacity, Circuit->mMaximumPowerConsumption, Circuit->mBoostProduced,
                Stats.PowerConsumed, Stats.PowerProduced, Stats.PowerProductionCapacity, Stats.MaximumPowerConsumption, Stats.BoostProduced, Stats.LastStatTime, Stats.StatIntervalTime, Time);
            UE_LOG(LogProductionStatsPowerEvidence, Display, TEXT("T01 circuit=%d group=%d batteryNet=%.9g statsBatteryNet=%.9g MW circuitStored=%.9g circuitStorageCapacity=%.9g MWh (raw circuit copies; compare with unique battery entries)"),
                Circuit->GetCircuitID(), Group, Circuit->mBatterySumPowerInput, Stats.BatteryPowerInput, Circuit->GetBatterySumPowerStore(), Circuit->GetBatterySumPowerStoreCapacity());
        }
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
            const double Base = Info->GetBaseProduction();
            const double Dynamic = Info->GetRegulatedDynamicProduction();
            Device.productionMW = Base + Dynamic + Device.boostMW;
            Device.consumer = Info->GetMaximumTargetConsumption() > 0 || Info->GetTargetConsumption() > 0 || Device.consumptionMW != 0;
            Device.producer = Base > 0 || Info->GetDynamicProductionCapacity() > 0 || Info->GetProductionBoostFactor() > 0 || Device.productionMW != 0;
            if (LogEvidence)
                UE_LOG(LogProductionStatsPowerEvidence, Display, TEXT("T01 circuit=%d info=%s ownerClass=%s actualConsumption=%.9g targetConsumption=%.9g baseProduction=%.9g regulatedDynamicProduction=%.9g boostProduction=%.9g candidateProduction=%.17g (MW; ownership/sampling pending validation)"),
                    Circuit->GetCircuitID(), *Info->GetPathName(), *FString(UTF8_TO_TCHAR(Device.buildingClass.c_str())),
                    Device.consumptionMW, Info->GetTargetConsumption(), Base, Dynamic, Device.boostMW, Device.productionMW);
            if (auto* Battery = Info->GetBatteryInfo(); IsValid(Battery))
            {
                Device.producer = false; // Inactive storage is not a generator either.
                if (LogEvidence)
                    UE_LOG(LogProductionStatsPowerEvidence, Display, TEXT("T01 circuit=%d info=%s battery=%s active=%d signedInput=%.9g MW stored=%.9g MWh capacity=%.9g MWh (native units pending game UI comparison)"),
                        Circuit->GetCircuitID(), *Info->GetPathName(), *Battery->GetPathName(), Battery->IsActive(), Battery->GetPowerInput(), Battery->GetPowerStore(), Battery->GetPowerStoreCapacity());
                if (Battery->IsActive()) Device.battery = BatterySample{reinterpret_cast<std::uint64_t>(Battery), Battery->GetPowerInput(), Battery->GetPowerStore(), Battery->GetPowerStoreCapacity()};
            }
            Devices.push_back(std::move(Device));
        }
    }
    if (LogEvidence)
        UE_LOG(LogProductionStatsPowerEvidence, Display, TEXT("T01 native registry powerCircuits=%llu participantEntries=%llu sampleTime=%.17g; no inference of complete coverage"),
            static_cast<unsigned long long>(Circuits.size()), static_cast<unsigned long long>(Devices.size()), Time);
    // T01 still has to establish circuit-local vs group assignment, boost ownership,
    // sampling phase and special-load coverage against the running game.
    return AggregatePower(Time, Circuits, Devices, false);
}
