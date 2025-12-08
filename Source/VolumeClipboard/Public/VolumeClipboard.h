#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FVolumeClipboardModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void RegisterMenus();
	void OpenPluginWindow();

private:
	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

	// Button Handlers
	FReply OnExtractVolumesClicked();
	FReply OnCreateVolumesClicked();

	// Checkbox Handlers
	void OnPasteLevelCheckboxChanged(ECheckBoxState NewState);
	ECheckBoxState GetPasteLevelCheckboxState() const;

	void OnDeleteOriginalCheckboxChanged(ECheckBoxState NewState);
	ECheckBoxState GetDeleteOriginalCheckboxState() const;

	// Helpers
	static void SerializeObjectProperties(UObject* Obj, TSharedPtr<class FJsonObject>& OutJson);
	static void RestoreObjectProperties(UObject* Obj, const TSharedPtr<class FJsonObject>& InJson);

	// State
	bool bPasteToOriginalLevel;
	bool bDeleteOriginalActor; // New Boolean
};