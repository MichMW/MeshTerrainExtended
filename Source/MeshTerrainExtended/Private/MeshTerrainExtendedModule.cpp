// Workaround for UE 5.8: "Assertion failed: ComponentNativeClass" (WorldPartitionActorDesc.cpp:944)
// when playing a Mesh Terrain / Mesh Partition map as Standalone Game.
//
// Cause: actors such as the "Section_X?-Y?_?" mesh providers carry a UMeshProviderModifier (or other
// modifier) component. Those classes live in MeshPartitionEditor, an Editor-type module that is not
// loaded in an uncooked "-game" process. World Partition still reads the actor descriptors there and
// check()s that every component class can be found.
//
// Second problem (terrain invisible): every Mesh Partition build variant puts its compiled sections in
// its own runtime data layer ("MPDL_<Variant>", initial state Unloaded). In PIE and in cooks the editor-only
// UPlatformCellTransformer strips those data layers from the streaming cells so the sections load normally.
// That transformer doesn't exist in -game, so the cells stay gated behind an Unloaded data layer.
// Fix: once the world's actors are initialized, activate the data layers of the build variants that the
// Mesh Partition definition assigns to the running platform (same lookup the editor uses).
//
// Fix for the crash: in uncooked -game only, redirect the modifier component classes to a small runtime stub whose
// component descriptor reads the same bytes. Editor, PIE, commandlets and packaged builds are untouched.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "UObject/CoreRedirects.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"
#include "HAL/PlatformProperties.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "MeshPartitionDataLayerContainer.h"
#include "MeshPartitionDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogMeshTerrainExtended, Log, All);

class FMeshTerrainExtendedModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		// Only needed in an uncooked game process (Standalone Game / -game). The editor has the real classes.
		if (GIsEditor || IsRunningCommandlet() || FParse::Param(FCommandLine::Get(), TEXT("NoMeshTerrainStandaloneFix")))
		{
			return;
		}

		// If someone loaded the real module anyway, don't hijack its classes.
		if (FModuleManager::Get().IsModuleLoaded(TEXT("MeshPartitionEditor")))
		{
			return;
		}

		static const TCHAR* const EditorOnlyModifierClasses[] =
		{
			// All UE::MeshPartition::UModifierComponent subclasses in MeshPartitionEditor (UE 5.8).
			TEXT("ModifierComponent"),
			TEXT("EditableModifierBase"),
			TEXT("MeshProviderModifier"),
			TEXT("PatchModifier"),
			TEXT("InstancedPatchModifier"),
			TEXT("SimpleWriteModifier"),
			TEXT("MeshBasedModifierBase"),
			TEXT("BooleanModifier"),
			TEXT("MeshProjectModifier"),
			TEXT("RemeshModifierBase"),
			TEXT("RemeshModifier"),
			TEXT("SplineRemeshModifier"),
			TEXT("LatticeModifier"),
			TEXT("NoiseModifier"),
			TEXT("ProjectMeshLayersModifier"),
			TEXT("TexturePatchModifier"),
			TEXT("InstancedTexturePatchModifier"),
			TEXT("SplineModifier"),
			TEXT("WeightUtilityModifier"),
			TEXT("LevelInstanceAdapter"),
			TEXT("InstancedProjectionModifier"),
			TEXT("TestModifierComponent"),
		};

		const FString StubClassPath = TEXT("/Script/MeshTerrainExtended.MeshPartitionModifierGameStub");

		TArray<FCoreRedirect> Redirects;
		for (const TCHAR* ClassName : EditorOnlyModifierClasses)
		{
			Redirects.Emplace(ECoreRedirectFlags::Type_Class, FString::Printf(TEXT("/Script/MeshPartitionEditor.%s"), ClassName), StubClassPath);
		}

		// Mesh Partition material expressions (also MeshPartitionEditor). Without these, terrain materials lose their
		// channel-sampling nodes in -game and render as the bare base layer (e.g. all white).
		static const TCHAR* const MaterialExpressionRedirects[][2] =
		{
			{ TEXT("MaterialExpressionMeshPartitionResource"),           TEXT("MeshTerrainExtendedMPResource") },
			{ TEXT("MaterialExpressionMeshPartitionTexcoord"),           TEXT("MeshTerrainExtendedMPTexcoord") },
			{ TEXT("MaterialExpressionMeshPartitionChannelSample"),      TEXT("MeshTerrainExtendedMPChannelSample") },
			{ TEXT("MaterialExpressionMeshPartitionChannelSampleIndex"), TEXT("MeshTerrainExtendedMPChannelSampleIndex") },
			{ TEXT("MaterialExpressionMeshPartitionInspector"),          TEXT("MeshTerrainExtendedMPInspector") },
		};
		for (const auto& Pair : MaterialExpressionRedirects)
		{
			Redirects.Emplace(ECoreRedirectFlags::Type_Class,
				FString::Printf(TEXT("/Script/MeshPartitionEditor.%s"), Pair[0]),
				FString::Printf(TEXT("/Script/MeshTerrainExtended.%s"), Pair[1]));
		}

		// Modifier subclasses from other editor-only modules can be added in DefaultEngine.ini:
		//   [MeshTerrainExtended]
		//   +ExtraModifierClasses=/Script/SomeEditorModule.SomeModifier
		TArray<FString> ExtraClasses;
		if (GConfig)
		{
			GConfig->GetArray(TEXT("MeshTerrainExtended"), TEXT("ExtraModifierClasses"), ExtraClasses, GEngineIni);
		}
		for (const FString& ClassPath : ExtraClasses)
		{
			Redirects.Emplace(ECoreRedirectFlags::Type_Class, ClassPath, StubClassPath);
		}

		FCoreRedirects::AddRedirectList(Redirects, TEXT("MeshTerrainExtended"));

		UE_LOG(LogMeshTerrainExtended, Log, TEXT("Uncooked -game: redirected %d editor-only Mesh Partition classes (modifier components -> %s, material expressions -> runtime copies)"), Redirects.Num(), *StubClassPath);

		OnWorldInitializedActorsHandle = FWorldDelegates::OnWorldInitializedActors.AddRaw(this, &FMeshTerrainExtendedModule::OnWorldInitializedActors);
#endif
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(OnWorldInitializedActorsHandle);
	}

private:
#if WITH_EDITOR
	void OnWorldInitializedActors(const FActorsInitializedParams& Params)
	{
		using namespace UE::MeshPartition;

		UWorld* World = Params.World;
		if (!World || !World->IsGameWorld() || !World->GetWorldPartition())
		{
			return;
		}

		AMeshPartitionDataLayerContainer* Container = AMeshPartitionDataLayerContainer::Get(World);
		if (!Container)
		{
			return;
		}

		UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(World);
		if (!DataLayerManager)
		{
			return;
		}

		// PerDefinitionBuildVariantDataLayers is a private UPROPERTY, read it through reflection.
		using FLayerMap = TMap<TObjectPtr<const UMeshPartitionDefinition>, FPerBuildVariantDataLayers>;
		const FMapProperty* MapProperty = FindFProperty<FMapProperty>(AMeshPartitionDataLayerContainer::StaticClass(), TEXT("PerDefinitionBuildVariantDataLayers"));
		if (!MapProperty)
		{
			UE_LOG(LogMeshTerrainExtended, Warning, TEXT("PerDefinitionBuildVariantDataLayers not found on %s; Mesh Partition data layers left untouched."), *Container->GetClass()->GetName());
			return;
		}
		const FLayerMap& PerDefinitionLayers = *MapProperty->ContainerPtrToValuePtr<FLayerMap>(Container);

		const FName PlatformName(FPlatformProperties::PlatformName());
		const FName IniPlatformName(FPlatformProperties::IniPlatformName());

		for (const TPair<TObjectPtr<const UMeshPartitionDefinition>, FPerBuildVariantDataLayers>& Entry : PerDefinitionLayers)
		{
			const UMeshPartitionDefinition* Definition = Entry.Key.Get();

			TArray<FName> VariantNames;
			if (Definition && Definition->GetCompiledSectionBuildVariants().Num() > 0)
			{
				VariantNames = Definition->GetCompiledSectionBuildVariantNamesForPlatform(PlatformName, IniPlatformName);
			}
			if (VariantNames.IsEmpty())
			{
				// No per-platform mapping: fall back to the Default variant, or the first one available.
				if (Entry.Value.Layers.Contains(NAME_Default))
				{
					VariantNames.Add(NAME_Default);
				}
				else if (!Entry.Value.Layers.IsEmpty())
				{
					VariantNames.Add(Entry.Value.Layers.CreateConstIterator().Key());
				}
			}

			for (const FName& VariantName : VariantNames)
			{
				const UDataLayerAsset* DataLayerAsset = Entry.Value.Layers.FindRef(VariantName).Get();
				if (!DataLayerAsset)
				{
					continue;
				}

				const bool bSet = DataLayerManager->SetDataLayerRuntimeState(DataLayerAsset, EDataLayerRuntimeState::Activated);
				UE_LOG(LogMeshTerrainExtended, Log, TEXT("Uncooked -game: activating Mesh Partition build variant '%s' (data layer %s) for %s: %s"),
					*VariantName.ToString(), *DataLayerAsset->GetName(), Definition ? *Definition->GetName() : TEXT("<null definition>"), bSet ? TEXT("ok") : TEXT("FAILED"));
			}
		}
	}
#endif

	FDelegateHandle OnWorldInitializedActorsHandle;
};

IMPLEMENT_MODULE(FMeshTerrainExtendedModule, MeshTerrainExtended)
