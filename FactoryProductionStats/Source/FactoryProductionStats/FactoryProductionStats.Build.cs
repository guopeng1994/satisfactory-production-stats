// SPDX-License-Identifier: 0BSD
using UnrealBuildTool;

public class FactoryProductionStats : ModuleRules
{
    public FactoryProductionStats(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "FactoryGame", "SML" });
    }
}
