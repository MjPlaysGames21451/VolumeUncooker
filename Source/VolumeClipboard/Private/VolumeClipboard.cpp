#include "VolumeClipboard.h"
#include "CoreMinimal.h" 
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "ToolMenus.h"
#include "GameFramework/Volume.h"
#include "Components/BrushComponent.h"

// --- BUILDER INCLUDES ---
#include "Builders/CubeBuilder.h"
#include "Builders/CylinderBuilder.h"
#include "Builders/ConeBuilder.h"
#include "Builders/TetrahedronBuilder.h"
#include "Builders/SheetBuilder.h"
#include "Builders/LinearStairBuilder.h"
#include "Builders/CurvedStairBuilder.h"
#include "Builders/SpiralStairBuilder.h"
// ------------------------

#include "Editor.h"
#include "UnrealEd.h"
#include "Editor/UnrealEdTypes.h" 
#include "Engine/Selection.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h" 
#include "HAL/PlatformApplicationMisc.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "EditorStyleSet.h" 
#include "BSPOps.h" 
#include "Model.h" 
#include "Engine/Polys.h" 
#include "Engine/Level.h" 
#include "EngineUtils.h"                  
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h" 
#include "Engine/LevelStreamingVolume.h"  
#include "EditorLevelUtils.h"             
#include "Misc/MessageDialog.h"           
#include "Misc/PackageName.h"

static const FName VolumeClipboardTabName("VolumeClipboard");

#define LOCTEXT_NAMESPACE "FVolumeClipboardModule"

void FVolumeClipboardModule::StartupModule()
{
	bPasteToOriginalLevel = true;
	bDeleteOriginalActor = true;

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(VolumeClipboardTabName, FOnSpawnTab::CreateRaw(this, &FVolumeClipboardModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("VolumeClipboardTabTitle", "Volume Tools"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FVolumeClipboardModule::RegisterMenus));
}

void FVolumeClipboardModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(VolumeClipboardTabName);
}

// --- CHECKBOX HANDLERS ---
void FVolumeClipboardModule::OnPasteLevelCheckboxChanged(ECheckBoxState NewState)
{
	bPasteToOriginalLevel = (NewState == ECheckBoxState::Checked);
}

ECheckBoxState FVolumeClipboardModule::GetPasteLevelCheckboxState() const
{
	return bPasteToOriginalLevel ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FVolumeClipboardModule::OnDeleteOriginalCheckboxChanged(ECheckBoxState NewState)
{
	bDeleteOriginalActor = (NewState == ECheckBoxState::Checked);
}

ECheckBoxState FVolumeClipboardModule::GetDeleteOriginalCheckboxState() const
{
	return bDeleteOriginalActor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}
// -------------------------

TSharedRef<SDockTab> FVolumeClipboardModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("Header", "Volume Copy/Paste Tools"))
						.Justification(ETextJustify::Center)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10, 5)
				[
					SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ContentPadding(FMargin(10, 5))
						.Text(LOCTEXT("ExtractBtn", "Copy Selected Volumes"))
						.OnClicked(FOnClicked::CreateRaw(this, &FVolumeClipboardModule::OnExtractVolumesClicked))
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10, 2)
				[
					SNew(SCheckBox)
						.IsChecked(TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateRaw(this, &FVolumeClipboardModule::GetPasteLevelCheckboxState)))
						.OnCheckStateChanged_Raw(this, &FVolumeClipboardModule::OnPasteLevelCheckboxChanged)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("PasteLocChk", "Paste to Original Level"))
								.ToolTipText(LOCTEXT("PasteLocTip", "If checked, tries to paste into origin level. If unchecked, pastes into CURRENTLY SELECTED level."))
						]
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10, 2)
				[
					SNew(SCheckBox)
						.IsChecked(TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateRaw(this, &FVolumeClipboardModule::GetDeleteOriginalCheckboxState)))
						.OnCheckStateChanged_Raw(this, &FVolumeClipboardModule::OnDeleteOriginalCheckboxChanged)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("DelOrigChk", "Delete Original Actors"))
								.ToolTipText(LOCTEXT("DelOrigTip", "If checked, attempts to delete existing actors with the same name before pasting to prevent duplicates."))
						]
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10, 5)
				[
					SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ContentPadding(FMargin(10, 5))
						.Text(LOCTEXT("CreateBtn", "Paste Volumes"))
						.OnClicked(FOnClicked::CreateRaw(this, &FVolumeClipboardModule::OnCreateVolumesClicked))
				]
		];
}

void FVolumeClipboardModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus) return;

	UToolMenu* ToolbarMenu = ToolMenus->ExtendMenu("LevelEditor.LevelEditorToolBar");
	if (!ToolbarMenu) return;

	FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");

	Section.AddEntry(FToolMenuEntry::InitToolBarButton(
		"OpenVolumeTool",
		FUIAction(FExecuteAction::CreateRaw(this, &FVolumeClipboardModule::OpenPluginWindow)),
		LOCTEXT("VolumeToolBtn", "Volume Tools"),
		LOCTEXT("VolumeToolTooltip", "Open Volume Copy/Paste Tools"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details")
	));
}

void FVolumeClipboardModule::OpenPluginWindow()
{
	FGlobalTabmanager::Get()->TryInvokeTab(VolumeClipboardTabName);
}

// ---------------------------------------------------------
// HELPER: Property Filtering
// ---------------------------------------------------------
static bool IsPropertySafeToCopy(FProperty* Property, UObject* Container)
{
	FString Name = Property->GetName();

	if (Name.Contains("Guid") || Name.Contains("Cookie") || Name.StartsWith("bHidden") ||
		Name == "Brush" || Name == "BrushComponent" || Name == "RootComponent" ||
		Name == "Model" || Name == "BrushBuilder" || Name == "ActorLabel" ||
		Name == "Owner" || Name == "Instigator" || Name == "SavedSelections" ||
		Name == "RelativeLocation" || Name == "RelativeRotation" || Name == "RelativeScale3D" ||
		Name == "Rotation" || Name == "Location" || Name == "PhysicsTransform" ||
		Name == "ReplicatedMovement" || Name == "SpriteScale" || Name == "PivotOffset" ||
		Name == "PrePivot" || Name == "Tags" || Name == "Layers" || Name == "InputPriority")
	{
		return false;
	}

	if (Property->IsA(FNumericProperty::StaticClass())) return true;
	if (Property->IsA(FBoolProperty::StaticClass())) return true;
	if (Property->IsA(FStrProperty::StaticClass())) return true;
	if (Property->IsA(FNameProperty::StaticClass())) return true;
	if (Property->IsA(FTextProperty::StaticClass())) return true;
	if (Property->IsA(FEnumProperty::StaticClass())) return true;
	if (Property->IsA(FStructProperty::StaticClass())) return true;
	if (Property->IsA(FArrayProperty::StaticClass())) return true;
	if (Property->IsA(FObjectPropertyBase::StaticClass())) return true;
	if (Property->IsA(FInterfaceProperty::StaticClass())) return true;

	return false;
}

static FString DoubleToPrecisionString(double Val)
{
	return FString::Printf(TEXT("%.17g"), Val);
}

// ---------------------------------------------------------
// LOGIC: Serialization / Extraction
// ---------------------------------------------------------

void FVolumeClipboardModule::SerializeObjectProperties(UObject* Obj, TSharedPtr<FJsonObject>& OutJson)
{
	for (TFieldIterator<FProperty> PropIt(Obj->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient)) continue;
		if (!IsPropertySafeToCopy(Property, Obj)) continue;

		FString StringValue;
		Property->ExportTextItem(StringValue, Property->ContainerPtrToValuePtr<void>(Obj), nullptr, Obj, PPF_None);

		if (!StringValue.IsEmpty() && StringValue != "None" && StringValue != "()" && StringValue != "nullptr")
		{
			OutJson->SetStringField(Property->GetName(), StringValue);
		}
	}
}

void FVolumeClipboardModule::RestoreObjectProperties(UObject* Obj, const TSharedPtr<FJsonObject>& InJson)
{
	for (auto& Pair : InJson->Values)
	{
		FString PropName = Pair.Key;
		FString PropValString = Pair.Value->AsString();
		FProperty* Property = Obj->GetClass()->FindPropertyByName(*PropName);

		if (Property && IsPropertySafeToCopy(Property, Obj))
		{
			Property->ImportText(*PropValString, Property->ContainerPtrToValuePtr<void>(Obj), 0, Obj);
		}
	}
}

FReply FVolumeClipboardModule::OnExtractVolumesClicked()
{
	TArray<TSharedPtr<FJsonValue>> VolumeArray;
	if (!GEditor) return FReply::Handled();

	UWorld* World = GEditor->GetEditorWorldContext().World();

	USelection* SelectedActors = GEditor->GetSelectedActors();
	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		AVolume* Volume = Cast<AVolume>(Actor);

		if (Volume)
		{
			TSharedPtr<FJsonObject> VolObj = MakeShareable(new FJsonObject);

			VolObj->SetStringField("Class", Volume->GetClass()->GetPathName());
			VolObj->SetStringField("InternalName", Volume->GetName());

			if (Volume->GetLevel())
			{
				UPackage* LevelPackage = Volume->GetLevel()->GetOutermost();
				FString LevelPackageName = LevelPackage->GetName();
				FString LevelShortName = FPackageName::GetShortName(LevelPackageName);

				VolObj->SetStringField("OriginLevel", LevelShortName);
				VolObj->SetStringField("OriginLevelPackage", LevelPackageName);
			}

			if (World && Cast<ALevelStreamingVolume>(Volume))
			{
				TArray<TSharedPtr<FJsonValue>> StreamLinks;
				for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
				{
					if (StreamingLevel)
					{
						int32 SlotIndex = StreamingLevel->EditorStreamingVolumes.Find(Cast<ALevelStreamingVolume>(Volume));
						if (SlotIndex != INDEX_NONE)
						{
							TSharedPtr<FJsonObject> LinkObj = MakeShareable(new FJsonObject);
							LinkObj->SetStringField("Package", StreamingLevel->GetWorldAssetPackageName());
							LinkObj->SetNumberField("Slot", SlotIndex);
							StreamLinks.Add(MakeShareable(new FJsonValueObject(LinkObj)));
						}
					}
				}
				VolObj->SetArrayField("StreamLinks", StreamLinks);
			}

			FVector CenterLocation = Volume->GetActorLocation();
			VolObj->SetStringField("LocX", DoubleToPrecisionString(CenterLocation.X));
			VolObj->SetStringField("LocY", DoubleToPrecisionString(CenterLocation.Y));
			VolObj->SetStringField("LocZ", DoubleToPrecisionString(CenterLocation.Z));

			FQuat Quat = Volume->GetActorQuat();
			VolObj->SetStringField("QuatX", DoubleToPrecisionString(Quat.X));
			VolObj->SetStringField("QuatY", DoubleToPrecisionString(Quat.Y));
			VolObj->SetStringField("QuatZ", DoubleToPrecisionString(Quat.Z));
			VolObj->SetStringField("QuatW", DoubleToPrecisionString(Quat.W));

			FVector Scale = Volume->GetActorScale3D();
			VolObj->SetStringField("SclX", DoubleToPrecisionString(Scale.X));
			VolObj->SetStringField("SclY", DoubleToPrecisionString(Scale.Y));
			VolObj->SetStringField("SclZ", DoubleToPrecisionString(Scale.Z));

			VolObj->SetNumberField("SpawnMethod", (int32)Volume->SpawnCollisionHandlingMethod);
			if (Volume->GetRootComponent())
			{
				VolObj->SetNumberField("Mobility", (int32)Volume->GetRootComponent()->Mobility);
			}
			VolObj->SetNumberField("BrushType", (int32)Volume->BrushType);

			TSharedPtr<FJsonObject> ActorProps = MakeShareable(new FJsonObject);
			SerializeObjectProperties(Volume, ActorProps);
			VolObj->SetObjectField("Properties", ActorProps);

			TArray<TSharedPtr<FJsonValue>> ComponentList;
			for (UActorComponent* Comp : Volume->GetComponents())
			{
				if (Comp->IsA(UBrushComponent::StaticClass())) continue;

				TSharedPtr<FJsonObject> CompObj = MakeShareable(new FJsonObject);
				CompObj->SetStringField("ClassName", Comp->GetClass()->GetName());

				TSharedPtr<FJsonObject> CompProps = MakeShareable(new FJsonObject);
				SerializeObjectProperties(Comp, CompProps);

				CompObj->SetObjectField("Props", CompProps);
				ComponentList.Add(MakeShareable(new FJsonValueObject(CompObj)));
			}
			VolObj->SetArrayField("Components", ComponentList);

			UModel* Model = Volume->Brush;
			if (!Model && Volume->GetBrushComponent()) Model = Volume->GetBrushComponent()->Brush;

			if (Model)
			{
				TArray<TSharedPtr<FJsonValue>> PolyArray;

				if (Model->Polys && Model->Polys->Element.Num() > 0)
				{
					for (const FPoly& Poly : Model->Polys->Element)
					{
						TSharedPtr<FJsonObject> PolyObj = MakeShareable(new FJsonObject);
						TArray<TSharedPtr<FJsonValue>> VertArray;

						PolyObj->SetNumberField("Flags", (double)Poly.PolyFlags);

						for (const FVector& V : Poly.Vertices)
						{
							TSharedPtr<FJsonObject> VObj = MakeShareable(new FJsonObject);
							VObj->SetNumberField("X", V.X);
							VObj->SetNumberField("Y", V.Y);
							VObj->SetNumberField("Z", V.Z);
							VertArray.Add(MakeShareable(new FJsonValueObject(VObj)));
						}
						PolyObj->SetArrayField("Verts", VertArray);
						PolyArray.Add(MakeShareable(new FJsonValueObject(PolyObj)));
					}
				}
				else if (Model->Nodes.Num() > 0)
				{
					for (int32 i = 0; i < Model->Nodes.Num(); i++)
					{
						const FBspNode& Node = Model->Nodes[i];
						if (Node.NumVertices < 3) continue;

						TSharedPtr<FJsonObject> PolyObj = MakeShareable(new FJsonObject);
						TArray<TSharedPtr<FJsonValue>> VertArray;

						PolyObj->SetNumberField("Flags", (double)Node.NodeFlags);

						for (int32 v = 0; v < Node.NumVertices; v++)
						{
							int32 VertIndex = Model->Verts[Node.iVertPool + v].pVertex;
							const FVector& V = Model->Points[VertIndex];

							TSharedPtr<FJsonObject> VObj = MakeShareable(new FJsonObject);
							VObj->SetNumberField("X", V.X);
							VObj->SetNumberField("Y", V.Y);
							VObj->SetNumberField("Z", V.Z);
							VertArray.Add(MakeShareable(new FJsonValueObject(VObj)));
						}
						PolyObj->SetArrayField("Verts", VertArray);
						PolyArray.Add(MakeShareable(new FJsonValueObject(PolyObj)));
					}
				}
				VolObj->SetArrayField("RawPolys", PolyArray);
			}

			VolObj->SetStringField("BuilderType", "CustomPolys");
			VolumeArray.Add(MakeShareable(new FJsonValueObject(VolObj)));
		}
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(VolumeArray, Writer);
	FPlatformApplicationMisc::ClipboardCopy(*OutputString);

	return FReply::Handled();
}

FReply FVolumeClipboardModule::OnCreateVolumesClicked()
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	if (ClipboardContent.IsEmpty()) return FReply::Handled();

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ClipboardContent);

	if (FJsonSerializer::Deserialize(Reader, JsonArray))
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World) return FReply::Handled();

		// Save current level so we can restore it ONCE at the end
		ULevel* SavedCurrentLevel = World->GetCurrentLevel();

		// =========================================================================================
		// PHASE 1: SCAN FOR REQUIRED LEVELS & LOAD THEM IMMEDIATELY
		// =========================================================================================
		TSet<FString> RequiredLevelPaths;
		EAppReturnType::Type MissingLevelResponse = EAppReturnType::Retry;

		// 1. Collect all potential level paths from the clipboard JSON
		for (TSharedPtr<FJsonValue> Val : JsonArray)
		{
			TSharedPtr<FJsonObject> Obj = Val->AsObject();
			if (!Obj.IsValid()) continue;

			const TSharedPtr<FJsonObject>* PropsPtr;
			if (Obj->TryGetObjectField("Properties", PropsPtr))
			{
				if ((*PropsPtr)->HasField("StreamingLevelNames"))
				{
					FString RawNames = (*PropsPtr)->GetStringField("StreamingLevelNames");
					RawNames = RawNames.Replace(TEXT("("), TEXT("")).Replace(TEXT(")"), TEXT("")).Replace(TEXT("\""), TEXT("")).Replace(TEXT("\'"), TEXT(""));

					TArray<FString> Targets;
					RawNames.ParseIntoArray(Targets, TEXT(","), true);
					for (FString& Path : Targets)
					{
						FString CleanPath = Path.TrimStartAndEnd();
						if (!CleanPath.IsEmpty())
						{
							RequiredLevelPaths.Add(CleanPath);
						}
					}
				}
			}
		}

		// 2. Iterate and Load Levels
		FString CurrentWorldPkg = World->GetOutermost()->GetName();
		FString CurrentWorldShort = FPackageName::GetShortName(CurrentWorldPkg);

		for (const FString& PathToCheck : RequiredLevelPaths)
		{
			// Check if Package Exists (Prevent crash on invalid path)
			if (!FPackageName::DoesPackageExist(PathToCheck)) continue;

			// Safety: Don't load self (Recursion Crash Fix)
			FString CheckShort = FPackageName::GetShortName(PathToCheck);
			if (PathToCheck == CurrentWorldPkg || CheckShort == CurrentWorldShort) continue;

			// Check if already loaded
			bool bIsLoaded = false;
			for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
			{
				if (StreamingLevel && (StreamingLevel->GetWorldAssetPackageName() == PathToCheck || FPackageName::GetShortName(StreamingLevel->GetWorldAssetPackageName()) == CheckShort))
				{
					bIsLoaded = true;
					break;
				}
			}

			if (bIsLoaded) continue;

			// Check User Preference
			if (MissingLevelResponse == EAppReturnType::NoAll) continue;

			bool bShouldLoad = false;
			if (MissingLevelResponse == EAppReturnType::YesAll)
			{
				bShouldLoad = true;
			}
			else
			{
				FText Message = FText::Format(LOCTEXT("MissingLevelPrompt", "The Level '{0}' referenced by this volume is not in the current world.\n\nDo you want to add it as a Sub-Level now?"), FText::FromString(CheckShort));
				MissingLevelResponse = FMessageDialog::Open(EAppMsgType::YesNoYesAllNoAll, Message);

				if (MissingLevelResponse == EAppReturnType::Yes || MissingLevelResponse == EAppReturnType::YesAll)
				{
					bShouldLoad = true;
				}
			}

			if (bShouldLoad)
			{
				// CRITICAL FIX: Ensure no actors are selected in the new map, which prevents state corruption during context switches
				GEditor->SelectNone(true, true);
				GEditor->NoteSelectionChange();

				// LOAD LEVEL
				// FIX: Use 'auto' to handle the return type safely.
				// In UE4.27 this returns ULevel*, but if your build expects ULevelStreaming*, auto handles the assignment.
				auto NewLevel = UEditorLevelUtils::AddLevelToWorld(World, *PathToCheck, ULevelStreamingDynamic::StaticClass());

				// CRITICAL FIX: FORCE RESET to Persistent Level immediately inside the loop.
				// AddLevelToWorld automatically sets the new level as "Current".
				// Failing to reset this causes the NEXT AddLevelToWorld call to try adding a sublevel-to-a-sublevel, which crashes on the 2nd attempt.
				if (World->PersistentLevel)
				{
					World->SetCurrentLevel(World->PersistentLevel);
				}

				// Flush streaming state to ensure memory is stable before next iteration
				if (NewLevel)
				{
					World->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);
				}
			}
		}

		// =========================================================================================
		// PHASE 2: SPAWN VOLUMES (INSIDE TRANSACTION)
		// =========================================================================================

		GEditor->BeginTransaction(LOCTEXT("PasteVolumes", "Paste Volumes"));
		GEditor->SelectNone(true, true);

		TArray<ALevelStreamingVolume*> PastedStreamingVolumes;

		for (TSharedPtr<FJsonValue> Val : JsonArray)
		{
			TSharedPtr<FJsonObject> Obj = Val->AsObject();
			if (!Obj.IsValid()) continue;

			FString ClassPath = Obj->GetStringField("Class");
			UClass* ActorClass = LoadObject<UClass>(nullptr, *ClassPath);

			if (ActorClass && ActorClass->IsChildOf(AVolume::StaticClass()))
			{
				FString InternalName = Obj->GetStringField("InternalName");

				// --- 1. DETERMINE TARGET LEVEL ---
				ULevel* TargetLevel = SavedCurrentLevel; // Default to saved level
				bool bFoundSpecificLevel = false;

				if (bPasteToOriginalLevel && Obj->HasField("OriginLevel"))
				{
					FString TargetShortName = Obj->GetStringField("OriginLevel");
					FString TargetPackageName = "";
					if (Obj->HasField("OriginLevelPackage")) TargetPackageName = Obj->GetStringField("OriginLevelPackage");

					if (!TargetPackageName.IsEmpty())
					{
						for (ULevel* Level : World->GetLevels())
						{
							if (Level && Level->GetOutermost()->GetName() == TargetPackageName)
							{
								TargetLevel = Level;
								bFoundSpecificLevel = true;
								break;
							}
						}
					}
					if (!bFoundSpecificLevel)
					{
						for (ULevel* Level : World->GetLevels())
						{
							if (Level)
							{
								FString LoadedLevelName = FPackageName::GetShortName(Level->GetOutermost()->GetName());
								if (LoadedLevelName == TargetShortName)
								{
									TargetLevel = Level;
									bFoundSpecificLevel = true;
									break;
								}
							}
						}
					}
				}

				World->SetCurrentLevel(TargetLevel);

				// --- 2. DELETE ORIGINAL ---
				if (bDeleteOriginalActor)
				{
					AActor* ExistingActor = Cast<AActor>(StaticFindObject(AActor::StaticClass(), TargetLevel, *InternalName));
					if (ExistingActor)
					{
						FString TrashName = InternalName + TEXT("_TRASH_") + FGuid::NewGuid().ToString();
						ExistingActor->Rename(*TrashName, nullptr, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
						World->EditorDestroyActor(ExistingActor, true);
					}
				}

				// --- 3. SPAWN ---
				FVector Location;
				Location.X = FCString::Atod(*Obj->GetStringField("LocX"));
				Location.Y = FCString::Atod(*Obj->GetStringField("LocY"));
				Location.Z = FCString::Atod(*Obj->GetStringField("LocZ"));

				FQuat Quat = FQuat::Identity;
				if (Obj->HasField("QuatW"))
				{
					Quat.X = FCString::Atod(*Obj->GetStringField("QuatX"));
					Quat.Y = FCString::Atod(*Obj->GetStringField("QuatY"));
					Quat.Z = FCString::Atod(*Obj->GetStringField("QuatZ"));
					Quat.W = FCString::Atod(*Obj->GetStringField("QuatW"));
				}

				FVector Scale;
				Scale.X = FCString::Atod(*Obj->GetStringField("SclX"));
				Scale.Y = FCString::Atod(*Obj->GetStringField("SclY"));
				Scale.Z = FCString::Atod(*Obj->GetStringField("SclZ"));

				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				SpawnParams.bNoFail = true;

				if (bDeleteOriginalActor || StaticFindObject(nullptr, TargetLevel, *InternalName) == nullptr)
				{
					SpawnParams.Name = FName(*InternalName);
				}

				if (Obj->HasField("SpawnMethod"))
				{
					SpawnParams.SpawnCollisionHandlingOverride = (ESpawnActorCollisionHandlingMethod)(int32)Obj->GetNumberField("SpawnMethod");
				}

				AVolume* NewVolume = World->SpawnActor<AVolume>(ActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

				if (NewVolume)
				{
					NewVolume->PreEditChange(nullptr);
					if (!InternalName.IsEmpty() && bDeleteOriginalActor) NewVolume->SetActorLabel(InternalName);

					if (Obj->HasField("BrushType"))
					{
						NewVolume->BrushType = (EBrushType)(int32)Obj->GetNumberField("BrushType");
					}
					if (Obj->HasField("SpawnMethod"))
					{
						NewVolume->SpawnCollisionHandlingMethod = (ESpawnActorCollisionHandlingMethod)(int32)Obj->GetNumberField("SpawnMethod");
					}
					if (NewVolume->GetRootComponent() && Obj->HasField("Mobility"))
					{
						NewVolume->GetRootComponent()->SetMobility((EComponentMobility::Type)(int32)Obj->GetNumberField("Mobility"));
					}

					// GEOMETRY
					NewVolume->Brush = NewObject<UModel>(NewVolume, NAME_None, RF_Transactional);
					NewVolume->Brush->Initialize(nullptr, true);
					NewVolume->Brush->Polys = NewObject<UPolys>(NewVolume->Brush, NAME_None, RF_Transactional);

					if (NewVolume->GetBrushComponent())
					{
						NewVolume->GetBrushComponent()->Brush = NewVolume->Brush;
					}

					const TArray<TSharedPtr<FJsonValue>>* PolyArray;
					if (Obj->TryGetArrayField("RawPolys", PolyArray))
					{
						for (const auto& PolyVal : *PolyArray)
						{
							TSharedPtr<FJsonObject> PolyObj = PolyVal->AsObject();
							if (!PolyObj.IsValid()) continue;

							FPoly NewPoly;
							NewPoly.Init();

							if (PolyObj->HasField("Flags")) NewPoly.PolyFlags = (uint32)PolyObj->GetNumberField("Flags");
							else NewPoly.PolyFlags = PF_NotSolid;

							const TArray<TSharedPtr<FJsonValue>>* VertArray;
							if (PolyObj->TryGetArrayField("Verts", VertArray))
							{
								for (const auto& VVal : *VertArray)
								{
									TSharedPtr<FJsonObject> VObj = VVal->AsObject();
									if (!VObj.IsValid()) continue;
									FVector Vertex;
									Vertex.X = VObj->GetNumberField("X");
									Vertex.Y = VObj->GetNumberField("Y");
									Vertex.Z = VObj->GetNumberField("Z");
									NewPoly.Vertices.Add(Vertex);
								}
							}
							if (NewPoly.Vertices.Num() >= 3)
							{
								NewPoly.Base = NewPoly.Vertices[0];
								NewPoly.Finalize(NewVolume, 0);
								NewVolume->Brush->Polys->Element.Add(NewPoly);
							}
						}
					}

					FBSPOps::bspBuild(NewVolume->Brush, FBSPOps::BSP_Optimal, 15, 70, 1, 0);
					FBSPOps::csgPrepMovingBrush(NewVolume);
					NewVolume->Brush->BuildBound();

					const TSharedPtr<FJsonObject>* PropsObject;
					if (Obj->TryGetObjectField("Properties", PropsObject))
					{
						RestoreObjectProperties(NewVolume, *PropsObject);
					}

					const TArray<TSharedPtr<FJsonValue>>* ComponentsArray;
					if (Obj->TryGetArrayField("Components", ComponentsArray))
					{
						for (const auto& CompVal : *ComponentsArray)
						{
							TSharedPtr<FJsonObject> CompData = CompVal->AsObject();
							FString CompClass = CompData->GetStringField("ClassName");
							const TSharedPtr<FJsonObject>* CompProps;
							if (CompData->TryGetObjectField("Props", CompProps))
							{
								for (UActorComponent* ExistingComp : NewVolume->GetComponents())
								{
									if (ExistingComp->GetClass()->GetName() == CompClass)
									{
										RestoreObjectProperties(ExistingComp, *CompProps);
									}
								}
							}
						}
					}

					// Store for Link Phase
					if (ALevelStreamingVolume* StreamingVol = Cast<ALevelStreamingVolume>(NewVolume))
					{
						PastedStreamingVolumes.Add(StreamingVol);
					}

					NewVolume->PostEditChange();
					FTransform FinalTransform;
					FinalTransform.SetLocation(Location);
					FinalTransform.SetRotation(Quat);
					FinalTransform.SetScale3D(Scale);

					if (USceneComponent* RootComp = NewVolume->GetRootComponent())
					{
						RootComp->SetRelativeTransform(FinalTransform, false, nullptr, ETeleportType::TeleportPhysics);
						RootComp->UpdateBounds();
					}
					else
					{
						NewVolume->SetActorTransform(FinalTransform, false, nullptr, ETeleportType::TeleportPhysics);
					}

					GEditor->SelectActor(NewVolume, true, false);
				}
			}
		}

		if (SavedCurrentLevel)
		{
			World->SetCurrentLevel(SavedCurrentLevel);
		}

		// =========================================================================================
		// PHASE 3: RELINK VOLUMES (INSIDE TRANSACTION)
		// =========================================================================================

		for (ALevelStreamingVolume* StreamingVol : PastedStreamingVolumes)
		{
			if (!StreamingVol || !IsValid(StreamingVol)) continue;

			// We already loaded all missing levels in Phase 1, so they are guaranteed to exist now.
			TArray<FName> RequiredLevelNames = StreamingVol->StreamingLevelNames;

			for (const FName& RequiredName : RequiredLevelNames)
			{
				FString RequiredPath = RequiredName.ToString();
				FString TargetShort = FPackageName::GetShortName(RequiredPath);

				for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
				{
					if (StreamingLevel)
					{
						FString StreamPkg = StreamingLevel->GetWorldAssetPackageName();
						FString StreamShort = FPackageName::GetShortName(StreamPkg);

						if (StreamPkg == RequiredPath || StreamShort == TargetShort)
						{
							StreamingLevel->Modify();
							StreamingLevel->EditorStreamingVolumes.AddUnique(StreamingVol);
						}
					}
				}
			}
		}

		GEditor->EndTransaction();
		GEditor->RebuildAlteredBSP();
		GEditor->RedrawAllViewports(true);
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVolumeClipboardModule, VolumeClipboard)