using UnrealBuildTool;

public class GASComboGraphEditor : ModuleRules
{
    public GASComboGraphEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "GASComboGraph", "Slate", "SlateCore",
            "UnrealEd", "GraphEditor", "PropertyEditor", "AssetTools", "AssetRegistry", "InputCore", "EnhancedInput",
            "GASCombatBridge", "GameplayAbilities"
        });
    }
}
