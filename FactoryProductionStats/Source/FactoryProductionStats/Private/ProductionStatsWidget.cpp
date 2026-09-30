// SPDX-License-Identifier: 0BSD
#include "ProductionStatsWidget.h"
#include "ProductionStatsSubsystem.h"
#include "ProductionStatsView.h"
#include "Subsystem/SubsystemActorManager.h"
#include "FGPlayerController.h"
#include "FGInputLibrary.h"
#include "UI/FGGameUI.h"
#include "Resources/FGItemDescriptor.h"
#include "Resources/FGBuildingDescriptor.h"
#include "UObject/UObjectIterator.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "TimerManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include <map>

#define LOCTEXT_NAMESPACE "FactoryProductionStats"
using namespace FactoryProductionStats;
namespace
{
constexpr int32 CurveLimit = 32;
FText Number(std::optional<double> Value)
{
    if (!Value) return LOCTEXT("Unknown", "尚无数据");
    FNumberFormattingOptions Options; Options.MaximumFractionalDigits = 3;
    return FText::AsNumber(*Value, &Options);
}
FText UnitLabel(const SeriesId& Id)
{
    if (Id.category == Category::Items) return LOCTEXT("ItemsRate", "件/min");
    if (Id.category == Category::Fluids) return FText::FromString(TEXT("m³/min"));
    return FText::FromString(UnitFor(Id) == Unit::MegawattHours ? TEXT("MWh") : TEXT("MW"));
}
FText AmountText(const SeriesResult& Row)
{
    if (!Row.summary.value) return LOCTEXT("Unknown", "尚无数据");
    if (const auto* QuantityValue = std::get_if<Quantity>(&*Row.summary.value))
    {
        if (const auto* Count = std::get_if<std::int64_t>(QuantityValue)) return FText::AsNumber(*Count);
        return FText::Format(LOCTEXT("Volume", "{0} m³"), Number(std::get<double>(*QuantityValue)));
    }
    return FText::Format(LOCTEXT("MetricValue", "{0} {1}"), Number(DisplayValue(Row.series, Row.summary)), UnitLabel(Row.series));
}
FLinearColor Colour(const SeriesId& Id)
{
    const auto Hue = static_cast<uint8>(ColourKey(Id) % 256);
    return FLinearColor::MakeFromHSV8(Hue, 180, 240);
}
struct FDisplayRow
{
    SeriesResult Data;
    FText Name;
    FSlateBrush Icon;
    bool Selected = false;
    double Ratio = 0;
    FText Extra;
    std::optional<std::uint32_t> Devices;
    PlotSegments Plot;
};
using FRowPtr = TSharedPtr<FDisplayRow>;
FText CoverageText(const SeriesResult& Row)
{
    const auto& Bin = Row.summary;
    return FText::Format(LOCTEXT("RowCoverage", "实际范围 {0}–{1}s；已观测 {2}s；{3}；缺口标记 {4}"),
        Number(Bin.range.begin), Number(Bin.range.end), Number(Bin.coverage.observedSeconds),
        Bin.coverage.completeSources ? LOCTEXT("Complete", "来源覆盖完整") : LOCTEXT("Partial", "来源覆盖未验证／不完整"),
        FText::AsNumber(Bin.coverage.gapReasons));
}
class SStatsChart final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SStatsChart) {} SLATE_END_ARGS()
    void Construct(const FArguments&) { SetClipping(EWidgetClipping::ClipToBounds); }
    FVector2D ComputeDesiredSize(float) const override { return {260, 190}; }
    void Update(const TArray<FRowPtr>& Rows, TimeRange Range, FText Units)
    {
        Curves.Reset(); Begin = Range.begin; End = Range.end; Label = Units;
        Low = 0; High = 1;
        for (const auto& Row : Rows)
        {
            if (!Row->Selected || Curves.Num() >= CurveLimit) continue;
            Curves.Add(Row);
            for (const auto& Segment : Row->Plot) for (const auto& Point : Segment)
            { Low = FMath::Min(Low, Point.value); High = FMath::Max(High, Point.value); }
        }
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        const double Width = FMath::Max(1., static_cast<double>(Geometry.GetLocalSize().X) - 54);
        const double Fraction = FMath::Clamp((Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()).X - 44) / Width, 0., 1.);
        const double Time = Begin + Fraction * (End - Begin);
        FString Tooltip = FText::Format(LOCTEXT("HoverTime", "统计时间 {0}s"), Number(Time)).ToString();
        for (const auto& Row : Curves)
        {
            std::optional<double> Value;
            const Bucket* Found = nullptr;
            for (const auto& Bin : Row->Data.points)
                if (Time >= Bin.range.begin && (Time < Bin.range.end || (Time == End && Bin.range.end == End))) { Found = &Bin; break; }
            if (Found && !HasTimeGap(*Found)) Value = DisplayValue(Row->Data.series, *Found);
            Tooltip += TEXT("\n") + Row->Name.ToString() + TEXT(": ") + Number(Value).ToString() + TEXT(" ") + Label.ToString();
            if (Found)
            {
                Tooltip += FText::Format(LOCTEXT("HoverCoverage", "（观测 {0}s／{1}s；缺口 {2}）"), Number(Found->coverage.observedSeconds),
                    Number(Found->range.end - Found->range.begin), FText::AsNumber(Found->coverage.gapReasons)).ToString();
                if (Found->value) if (const auto* State = std::get_if<EnergyState>(&*Found->value))
                    Tooltip += FText::Format(LOCTEXT("SampleTime", "，能量快照时间 {0}s"), Number(State->time)).ToString();
            }
        }
        SetToolTipText(FText::FromString(Tooltip));
        return FReply::Handled();
    }
    int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
        int32 Layer, const FWidgetStyle&, bool) const override
    {
        const FVector2D Size = Geometry.GetLocalSize();
        const double Width = FMath::Max(1., static_cast<double>(Size.X) - 54), Height = FMath::Max(1., static_cast<double>(Size.Y) - 38);
        const auto Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);
        const auto Text = [&](FVector2D Position, const FText& Value)
        { FSlateDrawElement::MakeText(Elements, Layer, Geometry.ToPaintGeometry(Position, Size), Value, Font, ESlateDrawEffect::None, FLinearColor::White); };
        for (int32 i = 0; i <= 4; ++i)
        {
            const double Y = 8 + Height * i / 4;
            TArray<FVector2D> Grid{{44, Y}, {44 + Width, Y}};
            FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Grid, ESlateDrawEffect::None, FLinearColor(.22f, .22f, .22f), true);
            Text({0, Y}, Number(High - (High - Low) * i / 4));
        }
        Text({44, Size.Y - 22}, FText::Format(LOCTEXT("Ago", "{0}s前"), Number(End - Begin)));
        Text({Size.X - 40, Size.Y - 22}, LOCTEXT("Now", "现在"));
        Text({44, 0}, Label);
        for (const auto& Row : Curves) for (const auto& Segment : Row->Plot)
        {
            TArray<FVector2D> Points;
            for (const auto& Point : Segment)
                Points.Add({44 + Width * FMath::Clamp((Point.time - Begin) / FMath::Max(1., End - Begin), 0., 1.),
                    8 + Height * (High - Point.value) / (High - Low)});
            if (Points.Num() == 1) Points.Add(Points[0] + FVector2D(2, 0));
            FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Colour(Row->Data.series), true, 1.5f);
        }
        return Layer + 1;
    }
private:
    TArray<FRowPtr> Curves;
    double Begin = 0, End = 0, Low = 0, High = 1;
    FText Label;
};
struct FResource
{
    FText Name;
    TStrongObjectPtr<UClass> Class;
    TStrongObjectPtr<UTexture2D> Icon;
};
}

class SProductionStatsWindow final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SProductionStatsWindow) {} SLATE_ARGUMENT(UProductionStatsWidget*, Host) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Host = Args._Host;
        auto Times = SNew(SWrapBox).UseAllottedSize(true);
        const TCHAR* Labels[]{TEXT("5s"), TEXT("1m"), TEXT("10m"), TEXT("1h"), TEXT("10h"), TEXT("50h"), TEXT("250h"), TEXT("1000h"), TEXT("All")};
        for (int32 i = 0; i < 9; ++i)
        {
            const auto Value = static_cast<Window>(i);
            Times->AddSlot().Padding(2)[SNew(SButton).Text(FText::FromString(Labels[i]))
                .ButtonColorAndOpacity_Lambda([this, Value] { return SelectedWindow == Value ? FLinearColor(.9f, .55f, .15f) : FLinearColor(.25f, .25f, .25f); })
                .OnClicked_Lambda([this, Value] { SelectedWindow = Value; Refresh(); return FReply::Handled(); })];
        }
        auto Tabs = SNew(SHorizontalBox);
        const FText TabNames[]{LOCTEXT("Items", "物品"), LOCTEXT("Fluids", "流体"), LOCTEXT("Power", "电力")};
        for (int32 i = 0; i < 3; ++i)
        {
            const auto Value = static_cast<Category>(i);
            Tabs->AddSlot().AutoWidth().Padding(2)[SNew(SButton).Text(TabNames[i])
                .ButtonColorAndOpacity_Lambda([this, Value] { return Page == Value ? FLinearColor(.9f, .55f, .15f) : FLinearColor(.25f, .25f, .25f); })
                .OnClicked_Lambda([this, Value] { Page = Value; Refresh(); return FReply::Handled(); })];
        }
        auto Body = SNew(SHorizontalBox);
        for (int32 i = 0; i < 3; ++i)
        {
            auto& Panel = Panels[i];
            Body->AddSlot().FillWidth(1).Padding(4)[SNew(SBox).MinDesiredWidth(230)
                .Visibility_Lambda([this, i] { return i < (Page == Category::Power ? 3 : 2) ? EVisibility::Visible : EVisibility::Collapsed; })[
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SAssignNew(Panel.Title, STextBlock)]
                + SVerticalBox::Slot().AutoHeight()[SAssignNew(Panel.Chart, SStatsChart)]
                + SVerticalBox::Slot().AutoHeight()[SAssignNew(Panel.Legend, STextBlock)]
                + SVerticalBox::Slot().FillHeight(1)[SAssignNew(Panel.List, SListView<FRowPtr>)
                    .ListItemsSource(&Panel.Rows).SelectionMode(ESelectionMode::None)
                    .OnGenerateRow_Lambda([this, i](FRowPtr Row, const TSharedRef<STableViewBase>& Owner) { return MakeRow(i, Row, Owner); })]
            ]];
        }
        ChildSlot[SNew(SSafeZone).HAlign(HAlign_Center).VAlign(VAlign_Center)[
            SNew(SBox).WidthOverride_Lambda([this] { return ViewSize().X; }).HeightOverride_Lambda([this] { return ViewSize().Y; })[
                SNew(SBorder).Padding(12).BorderBackgroundColor(FLinearColor(.055f, .055f, .065f, 1))[
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(LOCTEXT("Title", "生产统计"))]
                        + SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Search, SSearchBox).HintText(LOCTEXT("Search", "搜索名称"))
                            .OnTextChanged_Lambda([this](const FText& Text) { SearchText = Text.ToString(); ApplyRows(); })]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this] { return ByName ? LOCTEXT("NameSort", "排序：名称") : LOCTEXT("ValueSort", "排序：数值"); })
                            .OnClicked_Lambda([this] { ByName = !ByName; ApplyRows(); return FReply::Handled(); })]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("Reset", "恢复显示"))
                            .OnClicked_Lambda([this] { Choices.clear(); for (auto& CategoryFlags : Manual) for (auto& Flag : CategoryFlags) Flag = false;
                                ByName = false; Search->SetText(FText::GetEmpty()); SearchText.Empty(); ApplyRows(); return FReply::Handled(); })]
                        + SHorizontalBox::Slot().AutoWidth()[SAssignNew(CloseButton, SButton).Text(LOCTEXT("Close", "关闭"))
                            .OnClicked_Lambda([this] { if (Host.IsValid()) Host->Close(); return FReply::Handled(); })]]
                    + SVerticalBox::Slot().AutoHeight()[Times]
                    + SVerticalBox::Slot().AutoHeight()[Tabs]
                    + SVerticalBox::Slot().AutoHeight()[SAssignNew(Status, STextBlock).AutoWrapText(true)]
                    + SVerticalBox::Slot().AutoHeight()[SAssignNew(PowerOverview, STextBlock).AutoWrapText(true)]
                    + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox).Orientation(Orient_Vertical)
                        + SScrollBox::Slot()[SNew(SBox).HeightOverride_Lambda([this] { return FMath::Max(360., ViewSize().Y - 210); })[
                            SNew(SScrollBox).Orientation(Orient_Horizontal)
                            + SScrollBox::Slot()[SNew(SBox).WidthOverride_Lambda([this] { return FMath::Max(730., ViewSize().X - 24); })[Body]]
                        ]]]
                ]
            ]
        ]];
    }
    bool EditingSearch() const { return Search.IsValid() && (Search->HasKeyboardFocus() || Search->HasFocusedDescendants()); }
    void FocusClose()
    {
        if (FSlateApplication::IsInitialized() && CloseButton) FSlateApplication::Get().SetKeyboardFocus(CloseButton, EFocusCause::SetDirectly);
    }
    void Refresh()
    {
        auto* World = Host.IsValid() ? Host->GetWorld() : nullptr;
        auto* Manager = World ? World->GetSubsystem<USubsystemActorManager>() : nullptr;
        auto* Stats = Manager ? Manager->GetSubsystemActor<AProductionStatsSubsystem>() : nullptr;
        if (!Stats)
        {
            Result = {}; Result.error = Error::NoCoverage;
            Status->SetText(LOCTEXT("NoAuthority", "统计尚不可用；首版仅支持本地主机／单人，联机客户端尚未实现。"));
            PowerOverview->SetText(FText::GetEmpty()); ApplyRows(); return;
        }
        Result = Stats->Query({Page, SelectedWindow, Stats->RecordingTime(), {}, 300});
        if (Result.error != Error::None)
            Status->SetText(FText::Format(LOCTEXT("QueryError", "统计不可用（错误 {0}）；未知／损坏存档保留原始数据，不覆盖。"), FText::AsNumber(static_cast<int32>(Result.error))));
        else
            Status->SetText(FText::Format(LOCTEXT("QueryRange", "所选时间：请求 {0}–{1}s，实际 {2}–{3}s；桶宽 {4}s。{5} 未观测区间断线；采集覆盖仍待 Windows 验证。"),
                Number(Result.requestedRange.begin), Number(Result.requestedRange.end), Number(Result.actualRange.begin), Number(Result.actualRange.end),
                Number(Result.resolutionSeconds), Result.approximateBoundary ? LOCTEXT("Approx", "约：边界已对齐到整桶。") : FText::GetEmpty()));
        if (Result.error == Error::None)
        {
            const auto RequestedSeconds = WindowSeconds(SelectedWindow);
            const bool Short = RequestedSeconds && Stats->RecordingDuration() < *RequestedSeconds;
            Status->SetText(FText::Format(LOCTEXT("RecordingDuration", "已记录 {0}s。{1} {2}"), Number(Stats->RecordingDuration()),
                Short ? LOCTEXT("ShortHistory", "所选窗口历史不足，速率按实际有效观测时间计算。") : FText::GetEmpty(), Status->GetText()));
        }
        if (Result.error == Error::None && Stats->PersistenceStatus() != Error::None)
            Status->SetText(FText::Format(LOCTEXT("SaveFailure", "{0} 保存失败（错误 {1}），请检查日志。"), Status->GetText(), FText::AsNumber(static_cast<int32>(Stats->PersistenceStatus()))));
        const auto& Snapshot = Stats->CurrentPower();
        const auto Current = [&](Metric Value) -> std::optional<double>
        { for (const auto& Reading : Snapshot.readings) if (Reading.series.scope == Scope::World && Reading.series.metric == Value) return Reading.value; return std::nullopt; };
        const auto Percent = [&](Metric Numerator, Metric Denominator, const FText& Empty)
        {
            const auto Top = Current(Numerator), Bottom = Current(Denominator);
            if (!Top || !Bottom) return LOCTEXT("Unknown", "尚无数据");
            return *Bottom > 0 ? FText::Format(LOCTEXT("Percent", "{0}%"), Number(*Top / *Bottom * 100)) : Empty;
        };
        if (Page == Category::Power)
            PowerOverview->SetText(FText::Format(LOCTEXT("CurrentPower", "当前（快照 {0}s）：实际消耗 {1} MW／需求 {2} MW；实际发电 {3} MW／容量 {4} MW；储能 {5}／{6} MWh；充电 {7} MW，放电 {8} MW；网络 {9}，跳闸 {10}；发电负载 {11}，储能比例 {12}。"),
                Number(Snapshot.time), Number(Current(Metric::ActualConsumption)), Number(Current(Metric::MaximumDemand)),
                Number(Current(Metric::ActualProduction)), Number(Current(Metric::ProductionCapacity)), Number(Current(Metric::StoredEnergy)), Number(Current(Metric::StorageCapacity)),
                Number(Current(Metric::ChargePower)), Number(Current(Metric::DischargePower)),
                Snapshot.networkCount ? FText::AsNumber(*Snapshot.networkCount) : LOCTEXT("Unknown", "尚无数据"),
                Snapshot.trippedNetworkCount ? FText::AsNumber(*Snapshot.trippedNetworkCount) : LOCTEXT("Unknown", "尚无数据"),
                Percent(Metric::ActualProduction, Metric::ProductionCapacity, LOCTEXT("NoCapacity", "无容量")),
                Percent(Metric::StoredEnergy, Metric::StorageCapacity, LOCTEXT("NoStorage", "无储能"))));
        else PowerOverview->SetText(FText::GetEmpty());
        ActiveSeries.clear();
        for (auto& Data : Result.series)
        {
            ActiveSeries.push_back(Data.series);
            Resolve(Data);
            auto& Entry = Cache[Data.series];
            if (!Entry) Entry = MakeShared<FDisplayRow>();
            Entry->Data = std::move(Data);
            Entry->Name = FText::FromString(UTF8_TO_TCHAR(Entry->Data.displayName.c_str()));
            Entry->Plot = MakePlot(Entry->Data);
            const auto Found = Resources.find(Entry->Data.series.key);
            Entry->Icon.SetResourceObject(Found != Resources.end() ? Found->second.Icon.Get() : nullptr);
            Entry->Icon.ImageSize = FVector2D(28, 28);
            Entry->Icon.DrawAs = Entry->Icon.GetResourceObject() ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
            Entry->Extra = FText::GetEmpty();
            if (Page == Category::Power)
            {
                std::optional<std::uint32_t> Count;
                for (const auto& Reading : Snapshot.readings) if (Reading.series == Entry->Data.series) Count = Reading.coveredDeviceCount;
                Entry->Devices = Count;
            }
        }
        ApplyRows();
    }
private:
    struct FPanel { TArray<FRowPtr> Rows; TSharedPtr<SListView<FRowPtr>> List; TSharedPtr<SStatsChart> Chart; TSharedPtr<STextBlock> Title, Legend; };
    FPanel Panels[3];
    TWeakObjectPtr<UProductionStatsWidget> Host;
    TSharedPtr<SSearchBox> Search;
    TSharedPtr<SButton> CloseButton;
    TSharedPtr<STextBlock> Status, PowerOverview;
    Category Page = Category::Items;
    Window SelectedWindow = Window::Minute1;
    bool ByName = false, Manual[3][3]{};
    FString SearchText;
    QueryResult Result;
    std::vector<SeriesId> ActiveSeries;
    std::map<SeriesId, FRowPtr> Cache;
    std::map<SeriesId, bool> Choices;
    std::map<std::string, FResource> Resources;
    std::map<std::string, TStrongObjectPtr<UClass>> BuildingDescriptors;
    bool BuildingIndexReady = false;
    FVector2D ViewSize() const
    {
        auto* World = Host.IsValid() ? Host->GetWorld() : nullptr;
        const auto Viewport = World ? UWidgetLayoutLibrary::GetViewportSize(World) : FVector2D(1280, 720);
        const float Scale = World ? UWidgetLayoutLibrary::GetViewportScale(World) : 1;
        return {FMath::Max(320., Viewport.X / FMath::Max(.1f, Scale) * .94), FMath::Max(300., Viewport.Y / FMath::Max(.1f, Scale) * .90)};
    }
    void Resolve(SeriesResult& Row)
    {
        if (Row.series.scope == Scope::World && Page == Category::Power) { Row.displayName = TCHAR_TO_UTF8(*LOCTEXT("World", "世界总量").ToString()); Row.resourceResolved = true; return; }
        if (Row.series.scope == Scope::Unclassified) { Row.displayName = TCHAR_TO_UTF8(*LOCTEXT("Residual", "未分类／电网调整").ToString()); Row.resourceResolved = true; return; }
        auto Found = Resources.find(Row.series.key);
        if (Found == Resources.end())
        {
            FResource Resource;
            auto* Class = LoadObject<UClass>(nullptr, UTF8_TO_TCHAR(Row.series.key.c_str()));
            Resource.Class.Reset(Class);
            UClass* Descriptor = Class && Class->IsChildOf(UFGItemDescriptor::StaticClass()) ? Class : nullptr;
            if (Class && Page == Category::Power)
            {
                if (!BuildingIndexReady)
                {
                    // One descriptor census per window, never one scan per row/tick.
                    for (TObjectIterator<UClass> It; It; ++It)
                        if (!It->HasAnyClassFlags(CLASS_Abstract) && It->IsChildOf(UFGBuildingDescriptor::StaticClass()))
                            if (auto* Buildable = UFGBuildingDescriptor::GetBuildableClass(*It).Get())
                                BuildingDescriptors.try_emplace(TCHAR_TO_UTF8(*Buildable->GetPathName()), *It);
                    BuildingIndexReady = true;
                }
                const auto Building = BuildingDescriptors.find(Row.series.key);
                if (Building != BuildingDescriptors.end()) Descriptor = Building->second.Get();
            }
            if (Descriptor)
            {
                Resource.Name = UFGItemDescriptor::GetItemName(Descriptor);
                Resource.Icon.Reset(UFGItemDescriptor::GetSmallIcon(Descriptor));
            }
            if (Resource.Name.IsEmpty()) Resource.Name = FText::Format(LOCTEXT("MissingResource", "未知资源：{0}"), FText::FromString(UTF8_TO_TCHAR(Row.series.key.c_str())));
            Found = Resources.emplace(Row.series.key, std::move(Resource)).first;
        }
        Row.displayName = TCHAR_TO_UTF8(*Found->second.Name.ToString());
        Row.resourceResolved = Found->second.Class.IsValid() && Found->second.Icon.IsValid();
    }
    FText PowerDetails(const SeriesId& Id) const
    {
        FString Text;
        const Metric Metrics[]{Metric::ProductionCapacity, Metric::MaximumDemand, Metric::ChargePower, Metric::DischargePower, Metric::ProductionBoost, Metric::StorageCapacity};
        const FText Names[]{LOCTEXT("Capacity", "发电容量"), LOCTEXT("Demand", "最大需求"), LOCTEXT("Charge", "充电"), LOCTEXT("Discharge", "放电"), LOCTEXT("Boost", "发电增益"), LOCTEXT("StorageCapacity", "储能容量")};
        for (int32 i = 0; i < 6; ++i)
        {
            SeriesId Related = Id; Related.metric = Metrics[i];
            const auto Found = Cache.find(Related);
            if (Found == Cache.end()) continue;
            if (!Text.IsEmpty()) Text += TEXT("；");
            Text += Names[i].ToString() + TEXT(" ") + Number(DisplayValue(Related, Found->second->Data.summary)).ToString() + TEXT(" ") + UnitLabel(Related).ToString();
        }
        return FText::FromString(Text);
    }
    bool InPanel(const SeriesId& Id, int32 Index) const
    {
        if (Id.category != Page) return false;
        if (Page != Category::Power) return Id.direction == (Index == 0 ? Direction::Produced : Direction::Consumed);
        const Metric Primary[]{Metric::ActualConsumption, Metric::ActualProduction, Metric::StoredEnergy};
        return Id.metric == Primary[Index] && Id.scope != Scope::World;
    }
    void ApplyRows()
    {
        const FText Titles[]{LOCTEXT("Produced", "产出"), LOCTEXT("Consumed", "消耗")};
        const FText PowerTitles[]{LOCTEXT("Consumption", "消耗"), LOCTEXT("Generation", "发电"), LOCTEXT("Storage", "储能")};
        for (int32 i = 0; i < 3; ++i)
        {
            auto& Panel = Panels[i]; Panel.Rows.Reset();
            if (Result.error == Error::None)
                for (const auto& Id : ActiveSeries)
                {
                    const auto Entry = Cache.find(Id);
                    if (Entry == Cache.end() || !InPanel(Id, i)) continue;
                    if (!SearchText.IsEmpty() && !Entry->second->Name.ToString().Contains(SearchText, ESearchCase::IgnoreCase)) continue;
                    Panel.Rows.Add(Entry->second);
                }
            Panel.Rows.Sort([this](const FRowPtr& A, const FRowPtr& B) { return SortBefore(A->Data, B->Data, ByName); });
            double Maximum = 0;
            for (const auto& Row : Panel.Rows) if (const auto Value = DisplayValue(Row->Data.series, Row->Data.summary)) Maximum = FMath::Max(Maximum, std::abs(*Value));
            int32 SelectedCount = 0;
            for (int32 j = 0; j < Panel.Rows.Num(); ++j)
            {
                auto& Row = Panel.Rows[j];
                const auto Choice = Choices.find(Row->Data.series);
                Row->Selected = Manual[static_cast<int32>(Page)][i] ? Choice != Choices.end() && Choice->second : j < 8;
                SelectedCount += Row->Selected;
                Row->Ratio = RelativeBar(DisplayValue(Row->Data.series, Row->Data.summary), Maximum);
                if (Page == Category::Power) Row->Extra = FText::Format(LOCTEXT("Devices", "当前覆盖设备数 {0}；{1}"),
                    Row->Devices ? FText::AsNumber(*Row->Devices) : LOCTEXT("Unknown", "尚无数据"), PowerDetails(Row->Data.series)); // Related rows all refreshed first.
            }
            Panel.Title->SetText(Page == Category::Power ? PowerTitles[i] : Titles[FMath::Min(i, 1)]);
            Panel.Legend->SetText(FText::Format(LOCTEXT("Legend", "所选时间；条目 {0}，选中 {1}，绘制 {2}／上限32。{3}"), FText::AsNumber(Panel.Rows.Num()), FText::AsNumber(SelectedCount), FText::AsNumber(FMath::Min(SelectedCount, CurveLimit)),
                Panel.Rows.IsEmpty() ? LOCTEXT("Empty", "尚无条目／无匹配结果") : FText::GetEmpty()));
            const SeriesId Units{Page, i == 2 && Page == Category::Power ? Metric::StoredEnergy : Metric::ActualConsumption, Direction::None, Scope::World, {}};
            Panel.Chart->Update(Panel.Rows, Result.actualRange, UnitLabel(Units));
            Panel.List->RequestListRefresh();
        }
    }
    TSharedRef<ITableRow> MakeRow(int32 Index, FRowPtr Row, const TSharedRef<STableViewBase>& Owner)
    {
        return SNew(STableRow<FRowPtr>, Owner).ToolTipText_Lambda([Row] { return CoverageText(Row->Data); })[SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()[SNew(SCheckBox)
                    .IsChecked_Lambda([Row] { return Row->Selected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                    .OnCheckStateChanged_Lambda([this, Index, Row](ECheckBoxState State)
                    {
                        if (!Manual[static_cast<int32>(Page)][Index])
                        { for (const auto& Entry : Panels[Index].Rows) Choices[Entry->Data.series] = Entry->Selected; Manual[static_cast<int32>(Page)][Index] = true; }
                        Choices[Row->Data.series] = State == ECheckBoxState::Checked; ApplyRows();
                    })]
                + SHorizontalBox::Slot().AutoWidth().Padding(3)[SNew(SBox).WidthOverride(28).HeightOverride(28)[SNew(SOverlay)
                    + SOverlay::Slot()[SNew(SImage).Image(&Row->Icon)]
                    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("?")))
                        .Visibility_Lambda([Row] { return Row->Icon.GetResourceObject() ? EVisibility::Collapsed : EVisibility::Visible; })]
                ]]
                + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text_Lambda([Row] { return Row->Name; }).AutoWrapText(true)]
                + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([Row] { return FText::Format(LOCTEXT("RowValue", "{0} {1}"), Number(DisplayValue(Row->Data.series, Row->Data.summary)), UnitLabel(Row->Data.series)); })]]
            + SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(5)[SNew(SProgressBar).Percent_Lambda([Row] { return static_cast<float>(Row->Ratio); })
                .FillColorAndOpacity(Colour(Row->Data.series)).ToolTipText(LOCTEXT("Ratio", "相对本栏显示条目最大绝对值的比例；不是库存或电网供电比例。"))]]
            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Text_Lambda([Row]
                { return FText::Format(LOCTEXT("Total", "所选时间：{0}；{1}"), AmountText(Row->Data), Row->Extra); })]
        ];
    }
};

UProductionStatsWidget::UProductionStatsWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    mUseKeyboard = true; mUseMouse = true; mDisablePlayerActions = true;
    mDisableBuildGunActions = true; mDisablePlayerEquipmentManagement = true;
    mSupportsCaching = false; mSupportsStacking = true; bIgnoreDefaultKeybindings = true;
    SetIsFocusable(true);
}
TSharedRef<SWidget> UProductionStatsWidget::RebuildWidget()
{
    SAssignNew(Content, SProductionStatsWindow).Host(this);
    return Content.ToSharedRef();
}
void UProductionStatsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    StartRefresh();
}
void UProductionStatsWidget::OnPushedToGameUI_Implementation()
{
    Super::OnPushedToGameUI_Implementation();
    StartRefresh();
}
void UProductionStatsWidget::StartRefresh()
{
    Refresh();
    GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &UProductionStatsWidget::Refresh, 1, true);
}
void UProductionStatsWidget::Refresh()
{
    auto* Player = Cast<AFGPlayerController>(GetOwningPlayer());
    auto* UI = Player ? Player->GetGameUI() : nullptr;
    if (Content && UI && UI->GetInteractWidgetStack().Contains(this)) Content->Refresh();
}
void UProductionStatsWidget::NativeDestruct()
{
    if (auto* World = GetWorld()) World->GetTimerManager().ClearTimer(RefreshTimer);
    Super::NativeDestruct(); // Native stack restores contexts, cursor and movement.
}
void UProductionStatsWidget::ReleaseSlateResources(bool ReleaseChildren) { Super::ReleaseSlateResources(ReleaseChildren); Content.Reset(); }
bool UProductionStatsWidget::IsEditingSearch() const { return Content && Content->EditingSearch(); }
void UProductionStatsWidget::SetupDefaultFocus_Implementation() { if (Content) Content->FocusClose(); }
void UProductionStatsWidget::OnEscapePressed_Implementation()
{
    if (IsEditingSearch()) { Content->FocusClose(); return; }
    Close();
}
void UProductionStatsWidget::Close()
{
    if (auto* World = GetWorld()) World->GetTimerManager().ClearTimer(RefreshTimer);
    if (auto* Player = Cast<AFGPlayerController>(GetOwningPlayer())) if (auto* UI = Player->GetGameUI()) UI->PopWidget(this);
}
FReply UProductionStatsWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape && Event.IsRepeat()) return FReply::Handled();
    if (Event.GetKey() == EKeys::Escape && !Event.IsRepeat()) { OnEscapePressed(); return FReply::Handled(); }
    if (!IsEditingSearch())
    {
        FKey Primary; TArray<FKey> Modifiers;
        if (UFGInputLibrary::GetCurrentMappingForAction(GetOwningPlayer(), TEXT("FactoryProductionStats_Toggle"), Primary, Modifiers) && Event.GetKey() == Primary)
        {
            bool Match = true;
            for (const auto& Modifier : Modifiers)
            {
                if (Modifier == EKeys::LeftShift || Modifier == EKeys::RightShift) Match &= Event.IsShiftDown();
                else if (Modifier == EKeys::LeftControl || Modifier == EKeys::RightControl) Match &= Event.IsControlDown();
                else if (Modifier == EKeys::LeftAlt || Modifier == EKeys::RightAlt) Match &= Event.IsAltDown();
                else if (Modifier == EKeys::LeftCommand || Modifier == EKeys::RightCommand) Match &= Event.IsCommandDown();
                else Match = false;
            }
            if (Match) { if (!Event.IsRepeat()) Close(); return FReply::Handled(); }
        }
    }
    return Super::NativeOnKeyDown(Geometry, Event);
}
#undef LOCTEXT_NAMESPACE
