#include "WeldSeamNormalsTransformer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WeldSeamNormalsTransformer)

DEFINE_LOG_CATEGORY_STATIC(LogMeshTerrainNormals, Log, All);

namespace UE::MeshTerrainExtended
{
namespace WeldLocals
{
	// Minimal union-find over vertex IDs.
	struct FUnionFind
	{
		TArray<int32> Parent;
		explicit FUnionFind(int32 Num)
		{
			Parent.SetNumUninitialized(Num);
			for (int32 i = 0; i < Num; ++i) { Parent[i] = i; }
		}
		int32 Find(int32 X)
		{
			while (Parent[X] != X) { Parent[X] = Parent[Parent[X]]; X = Parent[X]; }
			return X;
		}
		void Union(int32 A, int32 B)
		{
			A = Find(A); B = Find(B);
			if (A != B) { Parent[FMath::Max(A, B)] = FMath::Min(A, B); }
		}
	};

	inline FIntVector CellOf(const FVector3d& P, double InvCell)
	{
		return FIntVector(FMath::FloorToInt32(P.X * InvCell), FMath::FloorToInt32(P.Y * InvCell), FMath::FloorToInt32(P.Z * InvCell));
	}
}

void FWeldSeamNormalsTransformer::GatherDependencies(UE::MeshPartition::IDependencyInterface& InDependencies) const
{
	// Change this GUID whenever Execute() changes behaviour.
	static const FGuid CodeVersion(TEXT("6f1c2a7e-3b4d-4e5f-9a8b-7c6d5e4f3a21"));
	FGuid Version = CodeVersion;
	bool bWeld = bWeldCoincidentNormals;
	float Angle = MaxWeldAngle;
	bool bSharp = bWeldSharpEdges;
	float Tol = WeldTolerance;
	InDependencies += Version;
	InDependencies += bWeld;
	InDependencies += Angle;
	InDependencies += bSharp;
	InDependencies += Tol;
}

bool FWeldSeamNormalsTransformer::Execute(UE::MeshPartition::FTransformerContext& InTransformerContext) const
{
	using namespace WeldLocals;
	const float CosMaxAngle = FMath::Cos(FMath::DegreesToRadians(MaxWeldAngle));
	const double Tolerance = FMath::Max(0.0, (double)WeldTolerance);

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

		const int32 MaxID = Mesh->MaxVertexID();
		FUnionFind UF(MaxID);
		int32 NumVertices = 0;

		// 1) Find coincident (or nearly coincident) vertices.
		if (Tolerance > 0.0)
		{
			const double InvCell = 1.0 / Tolerance;
			const double TolSq = Tolerance * Tolerance;
			TMap<FIntVector, TArray<int32, TInlineAllocator<4>>> Cells;
			Cells.Reserve(MaxID);
			for (int32 VertexID = 0; VertexID < MaxID; ++VertexID)
			{
				if (!Mesh->IsVertex(VertexID)) { continue; }
				++NumVertices;
				Cells.FindOrAdd(CellOf(Mesh->GetVertex(VertexID), InvCell)).Add(VertexID);
			}
			for (const TPair<FIntVector, TArray<int32, TInlineAllocator<4>>>& Cell : Cells)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				for (int32 DY = -1; DY <= 1; ++DY)
				for (int32 DZ = -1; DZ <= 1; ++DZ)
				{
					const FIntVector Other = Cell.Key + FIntVector(DX, DY, DZ);
					// Visit each cell pair once (and the cell with itself).
					if (Other.X < Cell.Key.X || (Other.X == Cell.Key.X && (Other.Y < Cell.Key.Y || (Other.Y == Cell.Key.Y && Other.Z < Cell.Key.Z))))
					{
						continue;
					}
					const TArray<int32, TInlineAllocator<4>>* OtherIDs = Cells.Find(Other);
					if (!OtherIDs) { continue; }
					const bool bSame = (Other == Cell.Key);
					for (int32 i = 0; i < Cell.Value.Num(); ++i)
					{
						const FVector3d P = Mesh->GetVertex(Cell.Value[i]);
						for (int32 j = bSame ? i + 1 : 0; j < OtherIDs->Num(); ++j)
						{
							if (FVector3d::DistSquared(P, Mesh->GetVertex((*OtherIDs)[j])) <= TolSq)
							{
								UF.Union(Cell.Value[i], (*OtherIDs)[j]);
							}
						}
					}
				}
			}
		}
		else
		{
			TMap<FVector3d, int32> FirstAt;
			FirstAt.Reserve(MaxID);
			for (int32 VertexID = 0; VertexID < MaxID; ++VertexID)
			{
				if (!Mesh->IsVertex(VertexID)) { continue; }
				++NumVertices;
				if (const int32* Existing = FirstAt.Find(Mesh->GetVertex(VertexID)))
				{
					UF.Union(*Existing, VertexID);
				}
				else
				{
					FirstAt.Add(Mesh->GetVertex(VertexID), VertexID);
				}
			}
		}

		// 2) Collect groups.
		TMap<int32, TArray<int32, TInlineAllocator<4>>> Groups;
		for (int32 VertexID = 0; VertexID < MaxID; ++VertexID)
		{
			if (!Mesh->IsVertex(VertexID)) { continue; }
			const int32 Root = UF.Find(VertexID);
			if (Root != VertexID || Groups.Contains(Root))
			{
				Groups.FindOrAdd(Root).Add(VertexID);
			}
		}

		int32 NumSharedGroups = 0, NumWelded = 0, NumWeldedSharp = 0, NumSkippedSharp = 0, NumSnapped = 0, NumMaxGroup = 0;
		float WorstDot = 1.0f;

		for (TPair<int32, TArray<int32, TInlineAllocator<4>>>& Group : Groups)
		{
			TArray<int32, TInlineAllocator<4>>& IDs = Group.Value;
			IDs.AddUnique(Group.Key); // the root itself
			NumMaxGroup = FMath::Max(NumMaxGroup, IDs.Num());
			if (IDs.Num() < 2)
			{
				continue;
			}
			++NumSharedGroups;

			// Snap near-coincident copies onto one position, so displacement can't open a hairline crack.
			const FVector3d Anchor = Mesh->GetVertex(IDs[0]);
			for (int32 k = 1; k < IDs.Num(); ++k)
			{
				if (Mesh->GetVertex(IDs[k]) != Anchor)
				{
					Mesh->SetVertex(IDs[k], Anchor);
					++NumSnapped;
				}
			}

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
			const bool bSharp = GroupWorstDot < CosMaxAngle;
			if (bSharp && !bWeldSharpEdges)
			{
				++NumSkippedSharp;
				continue;
			}

			FVector3f Welded = Sum.GetSafeNormal();
			if (Welded.IsNearlyZero())
			{
				// Opposing normals (a folded sliver): any single shared normal is better than a crack.
				Welded = First.GetSafeNormal();
				if (Welded.IsNearlyZero()) { continue; }
			}
			for (int32 ID : IDs)
			{
				Mesh->SetVertexNormal(ID, Welded);
			}
			++NumWelded;
			if (bSharp) { ++NumWeldedSharp; }
		}

		UE_LOG(LogMeshTerrainNormals, Display,
			TEXT("Section %s: recompute=%d verts=%d shared=%d (largest group %d) welded=%d (sharp %d) skippedSharp=%d snapped=%d worstAngle=%.1f deg"),
			*GetNameSafe(Unit.Section.Get()), bIncomingRecompute ? 1 : 0, NumVertices, NumSharedGroups, NumMaxGroup,
			NumWelded, NumWeldedSharp, NumSkippedSharp, NumSnapped,
			FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(WorstDot, -1.0f, 1.0f))));

		Unit.MeshData = Mesh;
		Unit.bShouldRecomputeNormals = false;
	}
	return true;
}
} // namespace UE::MeshTerrainExtended
