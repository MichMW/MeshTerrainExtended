// Runtime copies of the Mesh Partition material expressions for uncooked -game (Standalone Game).
//
// The real expressions (UE::MeshPartition::UMaterialExpressionMeshPartition*) live in the editor-only
// MeshPartitionEditor module. In a -game process that module isn't loaded, so every terrain material loses
// its channel-sampling nodes on load: the channel texture parameter disappears from the material, the
// texture slots no longer line up with the shader ("Requesting an invalid TextureIndex") and all channel
// weights read 0, which renders the terrain as the plain base layer (white with the default MPD).
//
// These classes are byte-for-byte functional copies (same properties, same Compile output, same shared
// parameter GUID) of MeshPartitionMaterialExpressionUtils.h/.cpp from UE 5.8. The module redirects the
// original class paths to them in uncooked -game only.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2DArray.h"
#include "Materials/MaterialExpression.h"
#include "MaterialValueType.h"
#include "MeshPartitionDefinition.h"

#include "MeshPartitionMaterialExpressionStubs.generated.h"

/** Copy of UE::MeshPartition::UMaterialExpressionMeshPartitionResource */
UCLASS(HideCategories = Object)
class UMeshTerrainExtendedMPResource : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(Transient)
	TWeakObjectPtr<UTexture2DArray> TextureParameter;

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif

	virtual FGuid& GetParameterExpressionId() override;
	virtual bool CanReferenceTexture() const override { return true; }
	virtual UObject* GetReferencedTexture() const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override { return MCT_Texture2DArray; }
};

/** Copy of UE::MeshPartition::UMaterialExpressionMeshPartitionTexcoord */
UCLASS(HideCategories = Object)
class UMeshTerrainExtendedMPTexcoord : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

protected:
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual uint32 GetOutputType(int32 OutputIndex) override { return MCT_Float2; }
};

/** Copy of UE::MeshPartition::UMaterialExpressionMeshPartitionChannelSample */
UCLASS(HideCategories = Object)
class UMeshTerrainExtendedMPChannelSample : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

protected:
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual EMaterialValueType GetInputValueType(int32 InputIndex) override { return MCT_Unknown; }
	virtual EMaterialValueType GetOutputValueType(int32 OutputIndex) override { return MCT_Float1; }
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif

public:
	UPROPERTY()
	FExpressionInput ChannelTextureInput;

	UPROPERTY(EditAnywhere, Category = MeshPartition)
	FName Channel;

	UPROPERTY(EditAnywhere, Category = MeshPartition)
	TObjectPtr<UE::MeshPartition::UMeshPartitionDefinition> MeshPartitionDefinition;

	int32 FindChannelIndex(FName InChannel) const;
};

/** Copy of UE::MeshPartition::UMaterialExpressionMeshPartitionChannelSampleIndex */
UCLASS(HideCategories = Object)
class UMeshTerrainExtendedMPChannelSampleIndex : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

protected:
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual EMaterialValueType GetInputValueType(int32 InputIndex) override { return InputIndex == 1 ? MCT_UInt1 : MCT_Unknown; }
	virtual EMaterialValueType GetOutputValueType(int32 OutputIndex) override { return MCT_Float1; }
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif

public:
	UPROPERTY()
	FExpressionInput ChannelTextureInput;

	UPROPERTY(meta = (RequiredInput = "false"))
	FExpressionInput ChannelIndex;

	UPROPERTY(EditAnywhere, Category = MeshPartition, meta = (OverridingInputProperty = "ChannelIndex"))
	int32 ConstChannelIndex = 0;
};

/** Copy of UE::MeshPartition::UMaterialExpressionMeshPartitionInspector (random debug colours, like the original) */
UCLASS(HideCategories = Object)
class UMeshTerrainExtendedMPInspector : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

protected:
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual EMaterialValueType GetInputValueType(int32 InputIndex) override { return MCT_Unknown; }
	virtual EMaterialValueType GetOutputValueType(int32 OutputIndex) override { return MCT_Float3; }
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif

	UPROPERTY()
	FExpressionInput ChannelTextureInput;
};
