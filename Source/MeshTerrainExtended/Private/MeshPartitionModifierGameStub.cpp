#include "MeshPartitionModifierGameStub.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionActorDescArchive.h"
#include "MeshPartitionModule.h" // UE::MeshPartition::FCustomVersion
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshPartitionModifierGameStub)

#if WITH_EDITOR
namespace UE::MeshTerrainExtended
{
	/** Same bytes as UE::MeshPartition::FBaseGrowth (declared in the editor-only MeshPartitionEditor module). */
	struct FBaseGrowthStub
	{
		bool X = false;
		bool Y = false;
		bool Z = true;

		bool operator==(const FBaseGrowthStub&) const = default;

		friend FArchive& operator<<(FArchive& Ar, FBaseGrowthStub& BaseGrowth)
		{
			return Ar << BaseGrowth.X << BaseGrowth.Y << BaseGrowth.Z;
		}
	};

	/**
	 * Mirror of UE::MeshPartition::FWorldPartitionModifierComponentDesc (MeshPartitionModifierComponentDesc.cpp, UE 5.8).
	 * Serialize() must stay byte-for-byte identical to the original, otherwise the rest of the actor descriptor
	 * stream is read misaligned. Values are read and discarded; nothing in -game uses them.
	 */
	class FModifierComponentDescStub : public FWorldPartitionComponentDesc
	{
	public:
		virtual uint32 GetSizeOf() const override
		{
			return sizeof(*this);
		}

		virtual void Serialize(FArchive& Ar) override
		{
			FWorldPartitionComponentDesc::Serialize(Ar);

			Ar.UsingCustomVersion(UE::MeshPartition::FCustomVersion::GUID);

			if (Ar.CustomVer(UE::MeshPartition::FCustomVersion::GUID) >= UE::MeshPartition::FCustomVersion::ComponentToActorTransform)
			{
				Ar << ComponentToActorTransform;
			}
			else
			{
				Ar << ComponentTransform;
			}
			Ar << ModifierPath;
			Ar << MegaMeshGuid;

			Ar << TDeltaSerialize<FName>(Type);
			Ar << TDeltaSerialize<double>(Priority);
			Ar << TDeltaSerialize<FBox>(LocalBounds);
			Ar << TDeltaSerialize<FBaseGrowthStub>(BaseGrowth);
			Ar << TDeltaSerialize<double>(Complexity);
			Ar << TDeltaSerialize<float>(ComplexityMultiplier);
			Ar << TDeltaSerialize<bool>(bIsContiguous);
			Ar << TDeltaSerialize<bool>(bIsDisabled);

			if (Ar.CustomVer(UE::MeshPartition::FCustomVersion::GUID) >= UE::MeshPartition::FCustomVersion::VirtualIsBaseModifier)
			{
				Ar << TDeltaSerialize<bool>(bIsBase);
			}
		}

		FTransform ComponentTransform;
		FTransform ComponentToActorTransform;
		FSoftObjectPath ModifierPath;
		FName Type;
		double Priority = 0.0;
		FGuid MegaMeshGuid;
		FBox LocalBounds = FBox(ForceInit);
		FBaseGrowthStub BaseGrowth;
		double Complexity = 0.0;
		float ComplexityMultiplier = 1.0f;
		bool bIsContiguous = false;
		bool bIsDisabled = false;
		bool bIsBase = false;
	};
}
#endif // WITH_EDITOR

UMeshPartitionModifierGameStub::UMeshPartitionModifierGameStub()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = false;
}

#if WITH_EDITOR
TUniquePtr<FWorldPartitionComponentDesc> UMeshPartitionModifierGameStub::CreateClassComponentDesc() const
{
	return MakeUnique<UE::MeshTerrainExtended::FModifierComponentDescStub>();
}
#endif
