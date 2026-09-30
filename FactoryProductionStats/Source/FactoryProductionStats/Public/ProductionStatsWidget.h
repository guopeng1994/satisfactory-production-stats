// SPDX-License-Identifier: 0BSD
#pragma once
#include "CoreMinimal.h"
#include "UI/FGInteractWidget.h"
#include "ProductionStatsWidget.generated.h"
class SProductionStatsWindow;

// Native UMG host, rendered with Slate. No fictitious Widget Blueprint asset.
UCLASS(NotBlueprintable)
class FACTORYPRODUCTIONSTATS_API UProductionStatsWidget : public UFGInteractWidget
{
    GENERATED_BODY()
public:
    UProductionStatsWidget(const FObjectInitializer& ObjectInitializer);
    void Close();
    bool IsEditingSearch() const;
    void OnEscapePressed_Implementation() override;
    void SetupDefaultFocus_Implementation() override;
    void OnPushedToGameUI_Implementation() override;
    void ReleaseSlateResources(bool ReleaseChildren) override;
protected:
    TSharedRef<SWidget> RebuildWidget() override;
    void NativeConstruct() override;
    void NativeDestruct() override;
    FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    void Refresh();
    void StartRefresh();
    TSharedPtr<SProductionStatsWindow> Content;
    FTimerHandle RefreshTimer;
};
