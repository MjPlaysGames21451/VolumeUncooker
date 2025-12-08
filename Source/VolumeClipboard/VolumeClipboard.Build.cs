using UnrealBuildTool;

public class VolumeClipboard : ModuleRules
{
    public VolumeClipboard(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "InputCore",

				// --- VITAL DEPENDENCIES FOR UE 4.27 ---
				"UnrealEd",        // Required for BSP Ops and GEditor
				"LevelEditor",     // Required for Toolbar extensions
				"ToolMenus",       // Required for Menu registration
				"Json",            // Required for serialization
				"JsonUtilities",   // Required for JSON utilities
				"EditorStyle",     // Required for FEditorStyle
				"ApplicationCore"  // Required for Clipboard Copy/Paste
			}
        );
    }
}