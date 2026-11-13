using UnrealBuildTool;

public class GASComboGraph : ModuleRules
{
    public GASComboGraph(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "EnhancedInput", "GameplayTags", "GameplayAbilities" });
        PrivateDependencyModuleNames.Add("GASCombatBridge");
    }
}
