// SPDX-License-Identifier: 0BSD
#include "ProductionStatsHooks.h"
#include "Patching/NativeHookManager.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableManufacturerVariablePower.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Buildables/FGBuildableGeneratorFuel.h"
#include "Buildables/FGBuildableGeneratorNuclear.h"
#include "Buildables/FGBuildableResourceSink.h"
#include "Buildables/FGBuildableFactorySimpleProducer.h"
#include "FGBuildablePowerBooster.h"
#include "FGPortableMiner.h"
#include "FGResourceSinkSubsystem.h"
#include "FGInventoryComponent.h"
#include "FGInventoryLibrary.h"
#include "Resources/FGItemDescriptor.h"
#include "Misc/ScopeLock.h"
#include "UObject/UObjectIterator.h"
#include "UObject/StrongObjectPtr.h"

using namespace FactoryProductionStats;
namespace
{
struct Binding
{
    std::shared_ptr<QuantityInbox> inbox;
    std::string source;
    UFGInventoryComponent* input = nullptr;
    UFGInventoryComponent* output = nullptr;
    bool sink = false, simple = false;
};
struct Descriptor
{
    std::string path;
    Category category;
    double scale;
};
// ponytail: one registry lock across worlds; split immutable frame snapshots
// only if T09 measures contention. No lock is held while forwarding game calls.
FCriticalSection RegistryMutex;
TMap<AActor*, Binding> Registry;
TMap<UClass*, TSharedPtr<Descriptor, ESPMode::ThreadSafe>> Descriptors;
TArray<TStrongObjectPtr<UClass>> RetainedClasses; // Created/destroyed only on the game thread.
struct ProductionScope
{
    Binding binding;
    bool inputs, outputs;
    ProductionScope* previous;
    static thread_local ProductionScope* current;
    ProductionScope(AActor* actor, bool inputs, bool outputs) : inputs(inputs), outputs(outputs), previous(current)
    {
        FScopeLock lock(&RegistryMutex);
        if (const auto* found = Registry.Find(actor)) binding = *found;
        current = this;
    }
    ~ProductionScope() { current = previous; }
};
thread_local ProductionScope* ProductionScope::current = nullptr;
void Emit(UClass* resource, int32 count, Direction direction)
{
    auto* scope = ProductionScope::current;
    if (!scope || !scope->binding.inbox || count <= 0) return;
    TSharedPtr<Descriptor, ESPMode::ThreadSafe> descriptor;
    {
        FScopeLock lock(&RegistryMutex);
        if (const auto* found = Descriptors.Find(resource)) descriptor = *found;
    }
    if (!descriptor) { scope->binding.inbox->MarkGap(); return; }
    QuantityEvent event{{descriptor->category, Metric::Quantity, direction, Scope::World, descriptor->path}, 0, std::int64_t{0}, scope->binding.source};
    if (NativeQuantity(event, count, descriptor->scale) != Error::None) scope->binding.inbox->MarkGap();
    else scope->binding.inbox->Submit(std::move(event));
}
}
TArray<TFunction<void()>> FProductionStatsHooks::Unsubscribers;
void FProductionStatsHooks::Install()
{
    if (WITH_EDITOR || !Unsubscribers.IsEmpty()) return;
    // Each hook forwards exactly once. A void production call is only a causal
    // scope: actual OnItemsAdded/Removed counts are the facts, not the call itself.
#define FPS_HOOK(Class, Method, ...) \
    { const auto Handle = SUBSCRIBE_UOBJECT_METHOD(Class, Method, __VA_ARGS__); \
      Unsubscribers.Add([Handle] { UNSUBSCRIBE_UOBJECT_METHOD(Class, Method, Handle); }); }
    FPS_HOOK(AFGBuildableManufacturer, Factory_TickProducing, [](auto& Next, auto* Self, float Dt) { ProductionScope Scope(Self, false, true); Next(Self, Dt); });
    FPS_HOOK(AFGBuildableManufacturerVariablePower, Factory_TickProducing, [](auto& Next, auto* Self, float Dt) { ProductionScope Scope(Self, false, true); Next(Self, Dt); });
    FPS_HOOK(AFGBuildableManufacturer, Factory_ConsumeIngredients, [](auto& Next, auto* Self) { ProductionScope Scope(Self, true, false); Next(Self); });
    FPS_HOOK(AFGBuildableResourceExtractor, Factory_TickProducing, [](auto& Next, auto* Self, float Dt) { ProductionScope Scope(Self, false, true); Next(Self, Dt); });
    FPS_HOOK(AFGPortableMiner, TickProducing, [](auto& Next, auto* Self, float Dt) { ProductionScope Scope(Self, false, true); Next(Self, Dt); });
    FPS_HOOK(AFGBuildableGeneratorFuel, LoadFuel, [](auto& Next, auto* Self) { ProductionScope Scope(Self, true, false); Next(Self); });
    FPS_HOOK(AFGBuildableGeneratorFuel, LoadSupplemental, [](auto& Next, auto* Self) { ProductionScope Scope(Self, true, false); Next(Self); });
    FPS_HOOK(AFGBuildableGeneratorNuclear, LoadFuel, [](auto& Next, auto* Self) { ProductionScope Scope(Self, true, false); Next(Self); });
    FPS_HOOK(AFGBuildableGeneratorNuclear, TryProduceWaste, [](auto& Next, auto* Self) { ProductionScope Scope(Self, false, true); Next(Self); });
    FPS_HOOK(AFGBuildablePowerBooster, LoadFuel, [](auto& Next, auto* Self) { ProductionScope Scope(Self, true, false); Next(Self); });
    FPS_HOOK(AFGBuildableResourceSink, Factory_CollectInput_Implementation, [](auto& Next, auto* Self) { ProductionScope Scope(Self, true, false); Next(Self); });
    FPS_HOOK(AFGResourceSinkSubsystem, AddPoints_ThreadSafe, [](auto& Next, auto* Self, TSubclassOf<UFGItemDescriptor> Item)
    {
        const bool Accepted = Next(Self, Item);
        if (Accepted && ProductionScope::current && ProductionScope::current->binding.sink) Emit(Item.Get(), 1, Direction::Consumed);
    });
    FPS_HOOK(AFGBuildableFactorySimpleProducer, Factory_GrabOutput_Implementation,
        [](auto& Next, auto* Self, UFGFactoryConnectionComponent* Connection, FInventoryItem& Item, float& Offset, TSubclassOf<UFGItemDescriptor> Type)
    {
        ProductionScope Scope(Self, false, true);
        const bool Produced = Next(Self, Connection, Item, Offset, Type);
        // This class generates one item on a successful grab; it has no output inventory.
        if (Produced && Scope.binding.simple) Emit(Item.GetItemClass().Get(), 1, Direction::Produced);
    });
    FPS_HOOK(UFGInventoryComponent, OnItemsAdded,
        [](auto& Next, UFGInventoryComponent* Self, const int32 Index, const int32 Count, UFGInventoryComponent* Source)
    {
        auto* Scope = ProductionScope::current;
        // Read the slot only within its owning mutation, before external delegates
        // can remove the item. Worker metadata is resolved from the immutable cache.
        if (!Source && Scope && Scope->outputs && Scope->binding.output == Self)
            Emit(Self->GetItemClassAtIndex(Index).Get(), Count, Direction::Produced);
        Next(Self, Index, Count, Source);
    });
    FPS_HOOK(UFGInventoryComponent, OnItemsRemoved,
        [](auto& Next, UFGInventoryComponent* Self, int32 Index, int32 Count, const FInventoryItem& Item, UFGInventoryComponent* Target)
    {
        auto* Scope = ProductionScope::current;
        if (!Target && Scope && Scope->inputs && Scope->binding.input == Self) Emit(Item.GetItemClass().Get(), Count, Direction::Consumed);
        Next(Self, Index, Count, Item, Target);
    });
#undef FPS_HOOK
}
void FProductionStatsHooks::Remove()
{
    { FScopeLock lock(&RegistryMutex); Registry.Empty(); Descriptors.Empty(); RetainedClasses.Empty(); }
    for (int32 Index = Unsubscribers.Num() - 1; Index >= 0; --Index) Unsubscribers[Index]();
    Unsubscribers.Empty();
}
void FProductionStatsHooks::RefreshDescriptors()
{
    check(IsInGameThread());
    // Once at startup/new actor or after an unknown descriptor, not per factory tick.
    for (TObjectIterator<UClass> It; It; ++It)
    {
        auto* Class = *It;
        if (!Class->IsChildOf(UFGItemDescriptor::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract)) continue;
        { FScopeLock lock(&RegistryMutex); if (Descriptors.Contains(Class)) continue; }
        const auto Form = UFGItemDescriptor::GetForm(Class);
        if (Form != EResourceForm::RF_SOLID && Form != EResourceForm::RF_LIQUID && Form != EResourceForm::RF_GAS) continue;
        const double Scale = Form == EResourceForm::RF_SOLID ? 1. : UFGInventoryLibrary::GetAmountConvertedByForm(1, Form);
        if (!std::isfinite(Scale) || Scale <= 0) continue;
        auto Value = MakeShared<Descriptor, ESPMode::ThreadSafe>();
        Value->path = TCHAR_TO_UTF8(*Class->GetPathName());
        Value->category = Form == EResourceForm::RF_SOLID ? Category::Items : Category::Fluids; Value->scale = Scale;
        FScopeLock lock(&RegistryMutex); Descriptors.Add(Class, MoveTemp(Value)); RetainedClasses.Emplace(Class);
    }
}
bool FProductionStatsHooks::Register(AActor* Actor, const std::shared_ptr<QuantityInbox>& Inbox)
{
    check(IsInGameThread());
    if (!IsValid(Actor) || !Actor->HasAuthority() || !Actor->HasActorBegunPlay()) return false;
    Binding Value; Value.inbox = Inbox; Value.source = TCHAR_TO_UTF8(*Actor->GetClass()->GetPathName());
    if (auto* Machine = Cast<AFGBuildableManufacturer>(Actor)) { Value.input = Machine->GetInputInventory(); Value.output = Machine->GetOutputInventory(); if (!Value.input || !Value.output) return false; }
    else if (auto* Extractor = Cast<AFGBuildableResourceExtractor>(Actor)) { Value.output = Extractor->GetOutputInventory(); if (!Value.output) return false; }
    else if (auto* Miner = Cast<AFGPortableMiner>(Actor)) { Value.output = Miner->GetOutputInventory(); if (!Value.output) return false; }
    else if (auto* Generator = Cast<AFGBuildableGeneratorFuel>(Actor))
    {
        Value.input = Generator->GetFuelInventory(); if (!Value.input) return false;
        if (auto* Nuclear = Cast<AFGBuildableGeneratorNuclear>(Actor)) { Value.output = Nuclear->GetWasteInventory(); if (!Value.output) return false; }
    }
    else if (auto* Booster = Cast<AFGBuildablePowerBooster>(Actor)) { Value.input = Booster->GetFuelInventory(); if (!Value.input) return false; }
    else if (Actor->IsA<AFGBuildableResourceSink>()) Value.sink = true;
    else if (Actor->IsA<AFGBuildableFactorySimpleProducer>()) Value.simple = true;
    else return true; // An unrelated actor is not a production source.
    FScopeLock lock(&RegistryMutex); Registry.Add(Actor, std::move(Value));
    return true;
}
bool FProductionStatsHooks::IsSource(const AActor* Actor)
{
    return Actor->IsA<AFGBuildableManufacturer>() || Actor->IsA<AFGBuildableResourceExtractor>() ||
        Actor->IsA<AFGPortableMiner>() || Actor->IsA<AFGBuildableGeneratorFuel>() ||
        Actor->IsA<AFGBuildablePowerBooster>() || Actor->IsA<AFGBuildableResourceSink>() ||
        Actor->IsA<AFGBuildableFactorySimpleProducer>();
}
void FProductionStatsHooks::Unregister(AActor* Actor)
{
    check(IsInGameThread());
    FScopeLock lock(&RegistryMutex); Registry.Remove(Actor);
}
void FProductionStatsHooks::ReleaseDescriptorsIfIdle()
{
    check(IsInGameThread());
    FScopeLock lock(&RegistryMutex);
    if (Registry.IsEmpty()) { Descriptors.Empty(); RetainedClasses.Empty(); }
}
