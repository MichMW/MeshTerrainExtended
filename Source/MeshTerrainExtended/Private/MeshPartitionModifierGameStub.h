// Stand-in for Mesh Partition modifier components when running uncooked -game (Standalone Game).

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MeshPartitionModifierGameStub.generated.h"

class FWorldPartitionComponentDesc;

/**
 * Inert scene component that the editor-only Mesh Partition modifier component classes
 * (/Script/MeshPartitionEditor.*Modifier*) are redirected to in uncooked -game processes.
 *
 * Its only job is to supply a World Partition component descriptor whose serialization layout
 * matches UE::MeshPartition::FWorldPartitionModifierComponentDesc, so actor descriptors that
 * reference modifier components can be read without hitting
 * "Assertion failed: ComponentNativeClass" in FWorldPartitionActorDesc::Serialize.
 */
UCLASS(HideDropdown, NotBlueprintable, NotBlueprintType, ClassGroup = Hidden)
class UMeshPartitionModifierGameStub : public USceneComponent
{
	GENERATED_BODY()

public:
	UMeshPartitionModifierGameStub();

#if WITH_EDITOR
private:
	virtual TUniquePtr<FWorldPartitionComponentDesc> CreateClassComponentDesc() const override;
#endif
};
