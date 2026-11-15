using UnrealBuildTool;

public class PinecoDebug : ModuleRules
{
    public PinecoDebug(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.Default;
        bLegacyParentIncludePaths = true;
        SetupGameplayDebuggerSupport(Target);
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GameplayAbilities", "PhysicsCore", "PinecoGAS", "TargetingSystem" });
    }
}
