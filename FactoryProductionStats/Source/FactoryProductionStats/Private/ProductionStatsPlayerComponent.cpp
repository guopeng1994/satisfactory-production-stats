// SPDX-License-Identifier: 0BSD
#include "ProductionStatsPlayerComponent.h"
#include "ProductionStatsWidget.h"
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGHealthComponent.h"
#include "UI/FGGameUI.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableText.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

namespace
{
bool TextHasFocus()
{
    if (!FSlateApplication::IsInitialized()) return false;
    const auto Focus = FSlateApplication::Get().GetKeyboardFocusedWidget();
    if (!Focus) return false;
    const FName Type = Focus->GetType();
    return Type == TEXT("SEditableText") || Type == TEXT("SEditableTextBox") ||
        Type == TEXT("SMultiLineEditableText") || Type == TEXT("SMultiLineEditableTextBox");
}
}
UProductionStatsPlayerComponent* UProductionStatsPlayerComponent::Attach(AFGPlayerController* Player)
{
    if (WITH_EDITOR || !IsValid(Player) || !Player->IsLocalController() || !Player->GetWorld()->IsGameWorld()) return nullptr;
    if (auto* Existing = Player->FindComponentByClass<UProductionStatsPlayerComponent>()) return Existing;
    auto* Component = NewObject<UProductionStatsPlayerComponent>(Player);
    Player->AddInstanceComponent(Component);
    Component->RegisterComponent();
    return Component;
}
void UProductionStatsPlayerComponent::OnRegister()
{
    Super::OnRegister();
    if (auto* Player = Cast<AFGPlayerController>(GetOwner()))
        Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &UProductionStatsPlayerComponent::OnPawnChanged);
}
void UProductionStatsPlayerComponent::BindInput(AFGCharacterPlayer* Character, UInputComponent* Input)
{
    auto* Enhanced = Cast<UEnhancedInputComponent>(Input);
    if (!IsValid(Character) || Character->GetController() != GetOwner() || !Enhanced) return;
    if (auto* Previous = BoundInput.Get()) Previous->ClearBindingsForObject(this);
    if (auto* Health = BoundHealth.Get()) Health->DeathDelegate.RemoveDynamic(this, &UProductionStatsPlayerComponent::OnDeath);
    BoundInput = Enhanced;
    BoundHealth = Character->GetHealthComponent();
    if (auto* Health = BoundHealth.Get()) Health->DeathDelegate.AddUniqueDynamic(this, &UProductionStatsPlayerComponent::OnDeath);
    ToggleAction = LoadObject<UInputAction>(nullptr, TEXT("/FactoryProductionStats/Inputs/IA_ProductionStats.IA_ProductionStats"));
    if (!ToggleAction) { UE_LOG(LogTemp, Warning, TEXT("FactoryProductionStats: create the real input assets with tools/create_input_assets.py; key binding unavailable")); return; }
    Enhanced->BindAction(ToggleAction, ETriggerEvent::Started, this, &UProductionStatsPlayerComponent::Toggle);
}
void UProductionStatsPlayerComponent::Toggle()
{
    auto* Player = Cast<AFGPlayerController>(GetOwner());
    auto* Character = Player ? Cast<AFGCharacterPlayer>(Player->GetPawn()) : nullptr;
    if (!Character || !Character->GetHealthComponent() || Character->GetHealthComponent()->IsDead() || TextHasFocus()) return;
    auto* GameUI = Player->GetGameUI();
    if (!GameUI || GameUI->IsPauseMenuOpen()) return;
    const auto Stack = GameUI->GetInteractWidgetStack();
    if (IsValid(Window) && Stack.Contains(Window.Get()))
    {
        // Widget handles its own rebound key after text/popups have first refusal.
        return;
    }
    if (GameUI->HasActiveInteractWidget()) return;
    // Controller focus on a console/modal is outside the normal viewport path.
    if (FSlateApplication::IsInitialized())
    {
        const auto Focus = FSlateApplication::Get().GetKeyboardFocusedWidget();
        if (Focus && Focus->GetType() != TEXT("SViewport")) return;
    }
    if (!Window) Window = CreateWidget<UProductionStatsWidget>(Player);
    GameUI->PushWidget(Window);
}
void UProductionStatsPlayerComponent::Close()
{
    if (Window) Window->Close();
}
void UProductionStatsPlayerComponent::OnDeath(AActor*) { Close(); }
void UProductionStatsPlayerComponent::OnPawnChanged(APawn*, APawn*)
{
    Close();
    if (auto* Input = BoundInput.Get()) Input->ClearBindingsForObject(this);
    if (auto* Health = BoundHealth.Get()) Health->DeathDelegate.RemoveDynamic(this, &UProductionStatsPlayerComponent::OnDeath);
    BoundInput.Reset(); BoundHealth.Reset();
}
void UProductionStatsPlayerComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    OnPawnChanged(nullptr, nullptr);
    if (auto* Player = Cast<AFGPlayerController>(GetOwner()))
        Player->OnPossessedPawnChanged.RemoveDynamic(this, &UProductionStatsPlayerComponent::OnPawnChanged);
    Window = nullptr; ToggleAction = nullptr;
    Super::EndPlay(Reason);
}
