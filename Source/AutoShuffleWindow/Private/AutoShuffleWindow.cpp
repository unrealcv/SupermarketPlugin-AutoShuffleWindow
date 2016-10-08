// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AutoShuffleWindowPrivatePCH.h"

#include "SlateBasics.h"
#include "SlateExtras.h"

#include "AutoShuffleWindowStyle.h"
#include "AutoShuffleWindowCommands.h"

#include "LevelEditor.h"

/** The following header files are not from the template */
#include "Json.h"

static const FName AutoShuffleWindowTabName("AutoShuffleWindow");

#define LOCTEXT_NAMESPACE "FAutoShuffleWindowModule"
DEFINE_LOG_CATEGORY(LogAutoShuffle);

#define VERBOSE_AUTO_SHUFFLE

void FAutoShuffleWindowModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FAutoShuffleWindowStyle::Initialize();
	FAutoShuffleWindowStyle::ReloadTextures();

	FAutoShuffleWindowCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FAutoShuffleWindowCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FAutoShuffleWindowModule::PluginButtonClicked),
		FCanExecuteAction());
		
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	
	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FAutoShuffleWindowModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
	
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FAutoShuffleWindowModule::AddToolbarExtension));
		
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AutoShuffleWindowTabName, FOnSpawnTab::CreateRaw(this, &FAutoShuffleWindowModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FAutoShuffleWindowTabTitle", "AutoShuffleWindow"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FAutoShuffleWindowModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FAutoShuffleWindowStyle::Shutdown();

	FAutoShuffleWindowCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AutoShuffleWindowTabName);
}

TSharedRef<SDockTab> FAutoShuffleWindowModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
    // init or re-init the SpinBoxes
    DensitySpinBox = SNew(SSpinBox<float>);
    DensitySpinBox->SetMinValue(0.f);
    DensitySpinBox->SetMaxValue(1.f);
    DensitySpinBox->SetMinSliderValue(0.f);
    DensitySpinBox->SetMaxSliderValue(1.f);
    DensitySpinBox->SetValue(0.5f);
    ProxmitySpinBox = SNew(SSpinBox<float>);
    ProxmitySpinBox->SetMinValue(0.f);
    ProxmitySpinBox->SetMaxValue(1.f);
    ProxmitySpinBox->SetMinSliderValue(0.f);
    ProxmitySpinBox->SetMaxSliderValue(1.f);
    ProxmitySpinBox->SetValue(0.5f);
    
    
    TSharedRef<SButton> Button = SNew(SButton);
    Button->SetVAlign(VAlign_Center);
    Button->SetHAlign(HAlign_Center);
    Button->SetContent(SNew(STextBlock).Text(FText::FromString(TEXT("Auto Shuffle"))));
    
    auto OnButtonClickedLambda = []() -> FReply
    {
        AutoShuffleImplementation();
        return FReply::Handled();
    };
    
    Button->SetOnClicked(FOnClicked::CreateLambda(OnButtonClickedLambda));
    FText Density = FText::FromString(TEXT("Density      "));
    FText Proxmity = FText::FromString(TEXT("Proxmity   "));
    
    return SNew(SDockTab).TabRole(ETabRole::NomadTab)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().Padding(30.f, 10.f).AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Center).AutoWidth()
            [
                SNew(STextBlock).Text(Density)
            ]
            + SHorizontalBox::Slot().HAlign(HAlign_Fill)
            [
                DensitySpinBox
            ]
        ]
        + SVerticalBox::Slot().Padding(30.f, 10.f).AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Center).AutoWidth()
            [
                SNew(STextBlock).Text(Proxmity)
            ]
            + SHorizontalBox::Slot().HAlign(HAlign_Fill)
            [
                ProxmitySpinBox
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(30.f, 10.f)
        [
            Button
        ]
    ];
}

void FAutoShuffleWindowModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->InvokeTab(AutoShuffleWindowTabName);
}

void FAutoShuffleWindowModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FAutoShuffleWindowCommands::Get().OpenPluginWindow);
}

void FAutoShuffleWindowModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FAutoShuffleWindowCommands::Get().OpenPluginWindow);
}

/** The folloinwg are the implemetations of the auto shuffle */
TSharedRef<SSpinBox<float>> FAutoShuffleWindowModule::DensitySpinBox = SNew(SSpinBox<float>);
TSharedRef<SSpinBox<float>> FAutoShuffleWindowModule::ProxmitySpinBox = SNew(SSpinBox<float>);
TArray<FAutoShuffleShelf>* FAutoShuffleWindowModule::ShelvesWhitelist = nullptr;
TArray<FAutoShuffleProduct>* FAutoShuffleWindowModule::ProductsWhitelist = nullptr;

void FAutoShuffleWindowModule::AutoShuffleImplementation()
{
    float Density = FAutoShuffleWindowModule::DensitySpinBox->GetValue();
    float Proxmity = FAutoShuffleWindowModule::ProxmitySpinBox->GetValue();
    FAutoShuffleWindowModule::ReadWhitelist();
    UE_LOG(LogAutoShuffle, Log, TEXT("Density: %f Proxmity: %f"), Density, Proxmity);
}

bool FAutoShuffleWindowModule::ReadWhitelist()
{
    // Always read the list even if the whitelists have been initialized
    // This is to make sure that changes of the configurations can be directly reflected every time the button is clicked
    // NOTE: the file may be in sandbox which is not changable after loading
    if (FAutoShuffleWindowModule::ShelvesWhitelist != nullptr)
    {
        delete FAutoShuffleWindowModule::ShelvesWhitelist;
        FAutoShuffleWindowModule::ShelvesWhitelist = nullptr;
    }
    if (FAutoShuffleWindowModule::ProductsWhitelist != nullptr)
    {
        delete FAutoShuffleWindowModule::ProductsWhitelist;
        FAutoShuffleWindowModule::ProductsWhitelist = nullptr;
    }
    FString PluginDir = FPaths::Combine(*FPaths::GamePluginsDir(), TEXT("AutoShuffleWindow"));
    FString ResourseDir = FPaths::Combine(*PluginDir, TEXT("Resources"));
    FString FileDir = FPaths::Combine(*ResourseDir, TEXT("Whitelist.json"));
    FString Filedata = "";
    FFileHelper::LoadFileToString(Filedata, *FileDir);
    TSharedPtr<FJsonObject> WhitelistJson = FAutoShuffleWindowModule::ParseJSON(Filedata, TEXT("Whitelist"), false);
    
    // Start reading JsonObject "Whitelist"
    {
        if (!WhitelistJson.IsValid())
        {
            UE_LOG(LogAutoShuffle, Warning, TEXT("Nothing to shuffle. Module quites."));
            return false;
        }
        if (WhitelistJson->HasField("Whitelist"))
        {
            UE_LOG(LogAutoShuffle, Log, TEXT("Reading Whitelist..."));
        }
        else
        {
            UE_LOG(LogAutoShuffle, Warning, TEXT("Whitelist reading failed. Module quites."));
            return false;
        }
    }
    TSharedPtr<FJsonObject> WhitelistObjectJson = WhitelistJson->GetObjectField("Whitelist");
    
    // Start reading JsonArray "Shelves"
    {
        if (WhitelistObjectJson->HasField("Shelves"))
        {
            UE_LOG(LogAutoShuffle, Log, TEXT("Reading Shelves..."));
        }
        else
        {
            UE_LOG(LogAutoShuffle, Warning, TEXT("Shelves reading failed. Module quits."));
            return false;
        }
    }
    TArray<TSharedPtr<FJsonValue>> ShelvesArrayJson = WhitelistObjectJson->GetArrayField("Shelves");
    FAutoShuffleWindowModule::ShelvesWhitelist = new TArray<FAutoShuffleShelf>();
    for (auto JsonValueIt = ShelvesArrayJson.CreateIterator(); JsonValueIt; ++JsonValueIt)
    {
        TSharedPtr<FJsonObject> ShelfObjectJson = (*JsonValueIt)->AsObject();
        ShelvesWhitelist->Add(FAutoShuffleShelf());
        FString NewName = ShelfObjectJson->GetStringField("Name");
        float NewScale = ShelfObjectJson->GetNumberField("Scale");
        TArray<float>* NewShelfBase = new TArray<float>();
        TArray<TSharedPtr<FJsonValue>> Shelfbase = ShelfObjectJson->GetArrayField("Shelfbase");
        for (auto BaseValueIt = Shelfbase.CreateIterator(); BaseValueIt; ++BaseValueIt)
        {
            NewShelfBase->Add((*BaseValueIt)->AsNumber());
        }
        ShelvesWhitelist->Top().SetShelfBase(NewShelfBase);
        ShelvesWhitelist->Top().SetScale(NewScale);
        ShelvesWhitelist->Top().SetName(NewName);
    }
#ifdef VERBOSE_AUTO_SHUFFLE
    for (auto ShelfIt = ShelvesWhitelist->CreateIterator(); ShelfIt; ++ShelfIt)
    {
        UE_LOG(LogAutoShuffle, Log, TEXT("Shelf: %s in Scale: %f"), *(ShelfIt->GetName()), ShelfIt->GetScale());
        for (auto BaseIt = ShelfIt->GetShelfBase()->CreateIterator(); BaseIt; ++BaseIt)
        {
            UE_LOG(LogAutoShuffle, Log, TEXT("Shelfbase: %f"), *BaseIt);
        }
    }
#endif
    
    // Start reading JsonArray "Products"
    {
        if (WhitelistObjectJson->HasField("Products"))
        {
            UE_LOG(LogAutoShuffle, Log, TEXT("Reading Products..."));
        }
        else
        {
            UE_LOG(LogAutoShuffle, Warning, TEXT("Products reading failed. Module quits."));
            return false;
        }
    }
    TArray<TSharedPtr<FJsonValue>> ProductsArrayJson = WhitelistObjectJson->GetArrayField("Products");
    
    UE_LOG(LogAutoShuffle, Log, TEXT("Collected %d Shelves and %d Products Group"), ShelvesArrayJson.Num(), ProductsArrayJson.Num());
    
    return true;
}

TSharedPtr<FJsonObject> FAutoShuffleWindowModule::ParseJSON(const FString& FileContents, const FString& NameForErrors, bool bSilent)
{
    // Parsing source code from:
    // https://github.com/EpicGames/UnrealEngine/blob/master/Engine/Plugins/2D/Paper2D/Source/PaperSpriteSheetImporter/Private/PaperJsonSpriteSheetImporter.cpp
    if (!FileContents.IsEmpty())
    {
        const TSharedRef<TJsonReader<>>& Reader = TJsonReaderFactory<>::Create(FileContents);
        TSharedPtr<FJsonObject> SpriteDescriptorObject;
        if (FJsonSerializer::Deserialize(Reader, SpriteDescriptorObject) && SpriteDescriptorObject.IsValid())
        {
            // File was loaded and deserialized OK!
            return SpriteDescriptorObject;
        }
        else
        {
            if (!bSilent)
            {
                UE_LOG(LogAutoShuffle, Warning, TEXT("Faled to parse sprite descriptor file '%s'. Error: '%s'"), *NameForErrors, *Reader->GetErrorMessage());
            }
            return nullptr;
        }
    }
    else
    {
        if (!bSilent)
        {
            UE_LOG(LogAutoShuffle, Warning, TEXT("Sprite descriptor file '%s' was empty. This sprite cannot be imported."), *NameForErrors);
        }
        return nullptr;
    }
}

FAutoShuffleObject::FAutoShuffleObject()
{
    Scale = 1.f;
    Name = TEXT("Uninitialized Object Name");
    Position = FVector(0.f, 0.f, 0.f);
    Rotation = FVector(0.f, 0.f, 0.f);
}

FAutoShuffleObject::~FAutoShuffleObject()
{
}

void FAutoShuffleObject::SetName(FString& NewName)
{
    Name = NewName;
}

FString FAutoShuffleObject::GetName() const
{
    return Name;
}

void FAutoShuffleObject::SetPosition(FVector& NewPosition)
{
    Position = NewPosition;
}

FVector FAutoShuffleObject::GetPosition() const
{
    return Position;
}

void FAutoShuffleObject::SetRotation(FVector& NewRotation)
{
    Rotation = NewRotation;
}

FVector FAutoShuffleObject::GetRotation() const
{
    return Rotation;
}

void FAutoShuffleObject::SetScale(float NewScale)
{
    Scale = NewScale;
}

float FAutoShuffleObject::GetScale() const
{
    return Scale;
}

FAutoShuffleShelf::FAutoShuffleShelf() : FAutoShuffleObject()
{
    ShelfBase = nullptr;
}

FAutoShuffleShelf::~FAutoShuffleShelf()
{
    if (ShelfBase != nullptr)
    {
        delete ShelfBase;
    }
}

void FAutoShuffleShelf::SetShelfBase(TArray<float>* NewShelfBase)
{
    if (ShelfBase != nullptr)
    {
        delete ShelfBase;
    }
    ShelfBase = NewShelfBase;
}

TArray<float>* FAutoShuffleShelf::GetShelfBase() const
{
    return ShelfBase;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAutoShuffleWindowModule, AutoShuffleWindow)