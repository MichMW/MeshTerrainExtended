#include "WeldSeamNormalsTransformer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WeldSeamNormalsTransformer)

DEFINE_LOG_CATEGORY_STATIC(LogMeshTerrainNormals, Log, All);

namespace UE::MeshTerrainExtended
{
bool FWeldSeamNormalsTransformer::Execute(UE::MeshPartition::FTransformerContext& InTransformerContext) const
{
	const float CosMaxAngle = FMath::Cos(FMath::DegreesToRadians(MaxWeldAngle));

	for (UE::MeshPartition::FTransformerUnit& Unit : InTransformerContext.TransformerUnits)
	{
		if (!Unit.MeshData.IsValid())
		{
			continue;
		}

		const bool bIncomingRecompute = Unit.bShouldRecomputeNormals;

		TSharedPtr<UE::MeshPartition::FMeshData> Mesh = MakeShared<UE::MeshPartition::FMeshData>();
		Mesh->Copy(*Unit.MeshData);

		if (bIncomingRecompute)
		{
			Mesh->RecomputeNormals();
		}

		// Group vertices by exact position.
		TMap<FVector3d, TArray<int32, TInlineAllocator<4>>> Groups;
		Groups.Reserve(Mesh->VertexCount());
		int32 NumVertices = 0;
		for (int VertexID : Mesh->VertexIndicesItr())
		{
			if (Mesh->IsVertex(VertexID))
			{
				Groups.FindOrAdd(Mesh->GetVertex(VertexID)).Add(VertexID);
				++NumVertices;
			}
		}

		int32 NumSharedGroups = 0, NumWelded = 0, NumSkippedSharp = 0, NumMaxGroup = 0;
		float WorstDot = 1.0f;

		for (const TPair<FVector3d, TArray<int32, TInlineAllocator<4>>>& Group : Groups)
		{
			const TArray<int32, TInlineAllocator<4>>& IDs = Group.Value;
			NumMaxGroup = FMath::Max(NumMaxGroup, IDs.Num());
			if (IDs.Num() < 2)
			{
				continue;
			}
			++NumSharedGroups;

			FVector3f Sum = FVector3f::ZeroVector;
			float GroupWorstDot = 1.0f;
			const FVector3f First = Mesh->GetVertexNormal(IDs[0]);
			for (int32 ID : IDs)
			{
				const FVector3f N = Mesh->GetVertexNormal(ID);
				Sum += N;
				GroupWorstDot = FMath::Min(GroupWorstDot, FVector3f::DotProduct(First, N));
			}
			WorstDot = FMath::Min(WorstDot, GroupWorstDot);

			if (!bWeldCoincidentNormals)
			{
				continue;
			}
			if (GroupWorstDot < CosMaxAngle)
			{
				++NumSkippedSharp;
				continue;
			}

			const FVector3f Welded = Sum.GetSafeNormal();
			if (Welded.IsNearlyZero())
			{
				continue;
			}
			for (int32 ID : IDs)
			{
				Mesh->SetVertexNormal(ID, Welded);
			}
			++NumWelded;
		}

		UE_LOG(LogMeshTerrainNormals, Display,
			TEXT("Section %s: recompute=%d verts=%d positions=%d shared=%d (largest group %d) welded=%d skippedSharp=%d worstAngle=%.1f deg"),
			*GetNameSafe(Unit.Section.Get()), bIncomingRecompute ? 1 : 0, NumVertices, Groups.Num(), NumSharedGroups, NumMaxGroup,
			NumWelded, NumSkippedSharp, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(WorstDot, -1.0f, 1.0f))));

		Unit.MeshData = Mesh;
		Unit.bShouldRecomputeNormals = false;
	}
	return true;
}
} // namespace UE::MeshTerrainExtended
