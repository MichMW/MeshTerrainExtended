// Mesh Terrain Extended: extensions and fixes for Epic's Mesh Terrain / Mesh Partition plugin (UE 5.8).
//
// Current contents (all in this UncookedOnly module: built for editor targets, loaded in the editor and in
// uncooked -game / Standalone Game, never part of a packaged game):
//   - Standalone Game support: redirects editor-only modifier component / material expression classes to
//     runtime stand-ins and activates the platform's compiled-section data layers.
//   - FWeldSeamNormalsTransformer: Mesh Partition transformer that removes hidden hard edges on UV-island
//     seams (fixes Nanite tessellation cracks).
//
// For future runtime features that must ship in a packaged game, add a separate "Runtime" module to the
// .uplugin; for editor tools, an "Editor" module.

using UnrealBuildTool;

public class MeshTerrainExtended : ModuleRules
{
	public MeshTerrainExtended(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bAllowUETypesInNamespaces = true; // FWeldSeamNormalsTransformer derives from UE::MeshPartition::FTransformer

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MeshPartition",
			}
		);
	}
}
