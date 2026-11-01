#include "MeshPartitionMaterialExpressionStubs.h"

#include "UObject/ConstructorHelpers.h"
#include "MeshPartitionChannel.h"
#include "MeshPartitionDefinition.h"

#if WITH_EDITOR
#include "MaterialCompiler.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshPartitionMaterialExpressionStubs)

namespace UE::MeshTerrainExtended::MaterialExpression
{
#if WITH_EDITOR
	using UE::MeshPartition::FChannelPacking;

	static int32 CompileChannelTexcoordUV(FMaterialCompiler* Compiler)
	{
		const int32 CHANNEL_TEXCOORD_INDEX = 0;
		return Compiler->TextureCoordinate(CHANNEL_TEXCOORD_INDEX, false, false);
	}

	static int32 CompilePickChannelTexture(FMaterialCompiler* Compiler, int32 InTextureCollection_IDX)
	{
		if (InTextureCollection_IDX >= 0)
		{
			return InTextureCollection_IDX;
		}
		return Compiler->Error(TEXT("Invalid default texture collection. Please assign a valid texture collection containing at least one texture."));
	}

	static int32 CompileUnpackChannelTextureSlot(FMaterialCompiler* Compiler, int32 InChannelIndex_IDX)
	{
		const EMaterialValueType ChannelTableCustomPrimitiveDataType = MCT_Float4;
		const int32 ChannelTable_IDX = Compiler->CustomPrimitiveData(UE::MeshPartition::PrimitiveDataIndex, ChannelTableCustomPrimitiveDataType);
		const int32 ChannelTableWordIndex_IDX = Compiler->Floor(Compiler->Mul(InChannelIndex_IDX, Compiler->Constant(1.0f / FChannelPacking::WordNumSlots)));
		const int32 ChannelTableWord_IDX = Compiler->VectorComponent(ChannelTable_IDX, ChannelTableWordIndex_IDX);
		const int32 ChannelTableWordSlot_IDX = Compiler->Sub(InChannelIndex_IDX, Compiler->Mul(ChannelTableWordIndex_IDX, Compiler->Constant(FChannelPacking::WordNumSlots)));
		return Compiler->BitfieldExtract(ChannelTableWord_IDX,
			Compiler->Mul(ChannelTableWordSlot_IDX, Compiler->Constant(FChannelPacking::SlotNumBits)),
			Compiler->Constant(FChannelPacking::SlotNumBits));
	}

	static int32 CompileFetchChannel(FMaterialCompiler* Compiler, int32 InTextureCollection_IDX, int32 InChannelIndex_IDX)
	{
		if (InChannelIndex_IDX < 0)
		{
			return Compiler->Constant(0.0f);
		}

		const int32 ChannelTextureSlot_IDX = CompileUnpackChannelTextureSlot(Compiler, InChannelIndex_IDX);
		const int32 TextureFromCollectionCodeIndex = CompilePickChannelTexture(Compiler, InTextureCollection_IDX);
		const int32 UV_IDX = CompileChannelTexcoordUV(Compiler);
		const int32 UVSlice_IDX = Compiler->AppendVector(UV_IDX, ChannelTextureSlot_IDX);
		const int32 UV_ConstSlice_IDX = Compiler->AppendVector(UV_IDX, Compiler->Constant(0));
		const int32 FetchedValueChannel_IDX = Compiler->TextureSample(TextureFromCollectionCodeIndex, UVSlice_IDX, SAMPLERTYPE_Masks,
			Compiler->DDX(UV_ConstSlice_IDX), Compiler->DDY(UV_ConstSlice_IDX), TMVM_Derivative, SSM_FromTextureAsset);

		return Compiler->If(
			ChannelTextureSlot_IDX,
			Compiler->Constant(FChannelPacking::SlotInvalid),
			Compiler->Constant(0.0f),
			Compiler->Constant(0.0f),
			FetchedValueChannel_IDX,
			Compiler->Constant(0.00001f));
	}
#endif
}

// ---------------------------------------------------------------- Resource

UMeshTerrainExtendedMPResource::UMeshTerrainExtendedMPResource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2DArray> DefaultTextureArray(TEXT("/MeshPartition/Textures/Void2DArray.Void2DArray"));
	TextureParameter = DefaultTextureArray.Object;

#if WITH_EDITORONLY_DATA
	bShowOutputNameOnPin = true;
	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("Channel Texture")));
#endif
}

UObject* UMeshTerrainExtendedMPResource::GetReferencedTexture() const
{
	return TextureParameter.IsValid() ? TextureParameter.Get() : nullptr;
}

FGuid& UMeshTerrainExtendedMPResource::GetParameterExpressionId()
{
	// Must match the original: shared, constant GUID for every Mesh Partition resource expression.
	static FGuid SharedGUID(0x4D656761, 0x4D657368, 0x20466F72, 0x65766572);
	return SharedGUID;
}

#if WITH_EDITOR
int32 UMeshTerrainExtendedMPResource::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	UTexture* TextureToUse = TextureParameter.Get();
	if (!TextureToUse)
	{
		return Compiler->Errorf(TEXT("Missing MeshPartition Texture."));
	}
	return Compiler->TextureParameter(UE::MeshPartition::ChannelTextureParameterName, TextureToUse, EMaterialSamplerType::SAMPLERTYPE_Masks);
}

void UMeshTerrainExtendedMPResource::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MeshPartition Shared Resource Param"));
	OutCaptions.Add(TEXT("MeshPartitionResource"));
}
#endif

// ---------------------------------------------------------------- Texcoord

UMeshTerrainExtendedMPTexcoord::UMeshTerrainExtendedMPTexcoord(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bShowOutputNameOnPin = true;
	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("uv")));
	Outputs.Add(FExpressionOutput(TEXT("uv [unit]")));
#endif
}

#if WITH_EDITOR
int32 UMeshTerrainExtendedMPTexcoord::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	const int32 UV_IDX = UE::MeshTerrainExtended::MaterialExpression::CompileChannelTexcoordUV(Compiler);
	if (OutputIndex == 0)
	{
		return UV_IDX;
	}
	const int32 UnitPerUV = Compiler->CustomPrimitiveData(4, EMaterialValueType::MCT_Float2);
	return Compiler->Mul(UV_IDX, UnitPerUV);
}

void UMeshTerrainExtendedMPTexcoord::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MeshPartitionTexCoord"));
}
#endif

// ---------------------------------------------------------------- ChannelSample

UMeshTerrainExtendedMPChannelSample::UMeshTerrainExtendedMPChannelSample(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UMeshTerrainExtendedMPChannelSample::FindChannelIndex(FName InChannel) const
{
	if (const UE::MeshPartition::UMeshPartitionDefinition* Definition = Cast<UE::MeshPartition::UMeshPartitionDefinition>(MeshPartitionDefinition.Get()))
	{
		return Definition->GetChannelMap().FindChannel(InChannel);
	}
	return INDEX_NONE;
}

#if WITH_EDITOR
int32 UMeshTerrainExtendedMPChannelSample::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!ChannelTextureInput.IsConnected())
	{
		return Compiler->Errorf(TEXT("Texture input not connected."));
	}

	const int32 TextureCollection_IDX = ChannelTextureInput.Compile(Compiler);

	const int32 ChannelIdx = FindChannelIndex(Channel);
	if (ChannelIdx == INDEX_NONE)
	{
		return Compiler->Errorf(TEXT("Invalid channel, not found in the definition."));
	}

	return UE::MeshTerrainExtended::MaterialExpression::CompileFetchChannel(Compiler, TextureCollection_IDX, Compiler->Constant(ChannelIdx));
}

void UMeshTerrainExtendedMPChannelSample::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Emplace(TEXT("MeshPartitionChannelSample"));
}
#endif

// ---------------------------------------------------------------- ChannelSampleIndex

UMeshTerrainExtendedMPChannelSampleIndex::UMeshTerrainExtendedMPChannelSampleIndex(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
int32 UMeshTerrainExtendedMPChannelSampleIndex::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!ChannelTextureInput.IsConnected())
	{
		return Compiler->Errorf(TEXT("Texture input not connected."));
	}

	const int32 TextureCollection_IDX = ChannelTextureInput.Compile(Compiler);
	const int32 Arg1 = ChannelIndex.GetTracedInput().Expression ? ChannelIndex.Compile(Compiler) : Compiler->Constant(ConstChannelIndex);
	return UE::MeshTerrainExtended::MaterialExpression::CompileFetchChannel(Compiler, TextureCollection_IDX, Arg1);
}

void UMeshTerrainExtendedMPChannelSampleIndex::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Emplace(TEXT("MeshPartitionChannelSampleIndex"));
}
#endif

// ---------------------------------------------------------------- Inspector

UMeshTerrainExtendedMPInspector::UMeshTerrainExtendedMPInspector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
int32 UMeshTerrainExtendedMPInspector::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!ChannelTextureInput.IsConnected())
	{
		return Compiler->Errorf(TEXT("Texture input not connected."));
	}

	const int32 TextureCollection_IDX = ChannelTextureInput.Compile(Compiler);
	const int32 NumChannels = UE::MeshPartition::FChannelPacking::MaxNumberPackedChannels;

	int32 Value_Idx = Compiler->Constant3(0, 0, 0);
	for (int32 ChannelIdx = 0; ChannelIdx < NumChannels; ++ChannelIdx)
	{
		const int32 FetchedValue_Idx = UE::MeshTerrainExtended::MaterialExpression::CompileFetchChannel(Compiler, TextureCollection_IDX, Compiler->Constant(ChannelIdx));
		const FLinearColor Color = FLinearColor::MakeRandomColor();
		Value_Idx = Compiler->Lerp(Value_Idx, Compiler->Constant3(Color.R, Color.G, Color.B), FetchedValue_Idx);
	}
	return Value_Idx;
}

void UMeshTerrainExtendedMPInspector::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Emplace(TEXT("MeshPartition Channels Inspector"));
}
#endif
