// SPDX-License-Identifier: 0BSD
using UnrealBuildTool;

public class FactoryProductionStats : ModuleRules
{
    public FactoryProductionStats(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PrivateDependencyModuleNames.Add("Core");
    }
}
