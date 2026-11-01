// Mesh Partition transformer that removes the "hidden hard edges" from compiled sections.
//
// Compiled sections are UV-split into islands (every vertex on an island border is duplicated), and
// FStaticMeshTransformer then recomputes per-vertex normals on that split mesh. Each copy of a border vertex
// only sees the triangles on its own side, so the copies get different normals: an invisible hard edge.
// Nanite tessellation/displacement pushes along that normal, the copies move apart -> cracks.
//
// This transformer does the same recompute itself, optionally gives every set of coincident vertices one
// averaged normal, and tells the following transformers not to recompute again.
// It also logs statistics (LogMeshTerrainNormals) so we can see what the mesh actually looks like.
//
// Put this transformer FIRST in the render-data transformer pipeline(s) used by your build variants.

#pragma once

#include "CoreMinimal.h"
#include "MeshPartitionTransformer.h"
#include "MeshPartitionMeshData.h"

#include "WeldSeamNormalsTransformer.generated.h"

namespace UE::MeshTerrainExtended
{
USTRUCT(meta = (DisplayName = "Weld Seam Normals (fixes tessellation cracks)"))
struct FWeldSeamNormalsTransformer : public UE::MeshPartition::FTransformer
{
	GENERATED_BODY()

	/** Average the normals of vertices that share a position. Off = only recompute (diagnostic). */
	UPROPERTY(EditAnywhere, Category = "Normals")
	bool bWeldCoincidentNormals = true;

	/** Only weld copies whose normals are already within this angle (degrees). Keeps genuinely sharp edges sharp. */
	UPROPERTY(EditAnywhere, Category = "Normals", meta = (ClampMin = "0", ClampMax = "180"))
	float MaxWeldAngle = 60.0f;

	using UE::MeshPartition::FTransformer::Execute; // keep the preview-component overload visible

	virtual bool Execute(UE::MeshPartition::FTransformerContext& InTransformerContext) const override;
};
} // namespace UE::MeshTerrainExtended
