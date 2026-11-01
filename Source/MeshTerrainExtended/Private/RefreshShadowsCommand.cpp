// Console command  MeshTerrain.RefreshShadows
// Re-creates the render state of all Mesh Partition sections (compiled sections in PIE / Standalone,
// preview sections in the editor viewport). That drops their cached virtual shadow map pages, so the
// shadows are re-rendered from the current terrain. Manual only: nothing runs on its own.

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogMeshTerrainShadows, Log, All);

namespace UE::MeshTerrainExtended
{
	static void RefreshMeshPartitionShadows()
	{
		if (!GEngine)
		{
			return;
		}

		// Looked up by name: no Mesh Partition headers needed, and the preview class only exists in the editor.
		UClass* SectionClasses[] =
		{
			FindObject<UClass>(nullptr, TEXT("/Script/MeshPartition.CompiledSection")),
			FindObject<UClass>(nullptr, TEXT("/Script/MeshPartitionEditor.PreviewSection")),
		};

		int32 NumComponents = 0;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !(World->IsGameWorld() || World->WorldType == EWorldType::Editor))
			{
				continue;
			}

			for (UClass* SectionClass : SectionClasses)
			{
				if (!SectionClass)
				{
					continue;
				}
				for (TActorIterator<AActor> It(World, SectionClass); It; ++It)
				{
					It->ForEachComponent<UPrimitiveComponent>(false, [&NumComponents](UPrimitiveComponent* Component)
					{
						if (Component->IsRegistered())
						{
							Component->MarkRenderStateDirty();
							++NumComponents;
						}
					});
				}
			}
		}

		UE_LOG(LogMeshTerrainShadows, Log, TEXT("Refreshed %d Mesh Partition section components."), NumComponents);
	}

	static FAutoConsoleCommand RefreshShadowsCommand(
		TEXT("MeshTerrain.RefreshShadows"),
		TEXT("Re-create the render state of all Mesh Partition sections, so their cached virtual shadows are re-rendered."),
		FConsoleCommandDelegate::CreateStatic(&RefreshMeshPartitionShadows));
}
