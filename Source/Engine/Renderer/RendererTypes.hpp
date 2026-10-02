#pragma once

#include <cstdint>

enum class PresentMode : uint8_t
{
	FIFO = 0,
	FIFORelaxed,
	Immediate,
	Mailbox,
};

struct ScissorRect
{
	uint32_t X = 0;
	uint32_t Y = 0;
	uint32_t Width = 0;
	uint32_t Height = 0;
};

struct Dimensions
{
	uint32_t Width = 1;
	uint32_t Height = 1;
	uint32_t Depth = 1;

	bool operator==(const Dimensions&) const = default;
};

struct Viewport
{
	float X = 0.0f;
	float Y = 0.0f;

	float Width = 1.0f;
	float Height = 1.0f;

	float MinDepth = 0.0f;
	float MaxDepth = 1.0f;
};

union ClearColorValue
{
	float Float32[4];
	int32_t Int32[4];
	uint32_t UInt32[4];
};

enum class ImageUsage : uint32_t
{
	None = 0,
	Texture,
	Storage,
	Attachment
};

enum class IndexFormat : uint8_t
{
	UInt8 = 0,
	UInt16,
	UInt32
};

enum class Topology : uint8_t
{
	Point = 0,
	Line,
	LineStrip,
	Triangle,
	TriangleStrip,
	Patch
};

enum class ColorSpace : uint8_t
{
	SRGBNonlinear = 0,
	SRGBExtendedLinear,
	HDR10,
	BT709Linear
};

enum class TextureType : uint8_t
{
	Texture2D = 0,
	Texture3D,
	TextureCube
};

enum class CullMode : uint8_t
{
	None = 0,
	Front,
	Back
};

enum class WindingMode : uint8_t
{
	CCW = 0,
	CW
};

enum class CompareOp : uint8_t
{
	Never = 0,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};

enum class StencilOp : uint8_t
{
	Keep = 0,
	Zero,
	Replace,
	IncrementClamp,
	DecrementClamp,
	Invert,
	IncrementWrap,
	DecrementWrap
};

enum class BlendMode : uint8_t
{
	None = 0,
	Alpha,
	PremultipliedAlpha,
	Additive,
	Multiply
};

enum class PolygonMode : uint8_t
{
	Fill = 0,
	Line,
	Point
};

enum class ShaderDataType
{
	None = 0,
	Float, Float2, Float3, Float4,
	Mat3, Mat4,
	Int, Int2, Int3, Int4,
	UInt, UInt2, UInt3, UInt4,
	Bool
};

enum class Format : uint8_t
{
	Invalid = 0,

	// 8-bit
	R8_UNorm,
	R8_SNorm,
	R8_UInt,
	R8_SInt,

	RG8_UNorm,
	RG8_SNorm,
	RG8_UInt,
	RG8_SInt,

	RGBA8_UNorm,
	RGBA8_SNorm,
	RGBA8_UInt,
	RGBA8_SInt,
	RGBA8_SRGB,

	BGRA8_UNorm,
	BGRA8_SRGB,

	// 16-bit
	R16_UNorm,
	R16_SNorm,
	R16_UInt,
	R16_SInt,
	R16_Float,

	RG16_UNorm,
	RG16_SNorm,
	RG16_UInt,
	RG16_SInt,
	RG16_Float,

	RGBA16_UNorm,
	RGBA16_SNorm,
	RGBA16_UInt,
	RGBA16_SInt,
	RGBA16_Float,

	// 32-bit
	R32_UInt,
	R32_SInt,
	R32_Float,

	RG32_UInt,
	RG32_SInt,
	RG32_Float,

	RGB32_UInt,
	RGB32_SInt,
	RGB32_Float,

	RGBA32_UInt,
	RGBA32_SInt,
	RGBA32_Float,

	// Packed
	RGB10A2_UNorm,
	BGR10A2_UNorm,
	R11G11B10_UFloat,
	RGB9E5_UFloat,

	// Depth / Stencil
	D16_UNorm,
	D24_UNorm_S8_UInt,
	D32_Float,
	D32_Float_S8_UInt,
	S8_UInt,

	// BC
	BC1_RGB_UNorm,
	BC1_RGB_SRGB,
	BC1_RGBA_UNorm,
	BC1_RGBA_SRGB,

	BC2_RGBA_UNorm,
	BC2_RGBA_SRGB,

	BC3_RGBA_UNorm,
	BC3_RGBA_SRGB,

	BC4_R_UNorm,
	BC4_R_SNorm,

	BC5_RG_UNorm,
	BC5_RG_SNorm,

	BC6H_RGB_UFloat,
	BC6H_RGB_SFloat,

	BC7_RGBA_UNorm,
	BC7_RGBA_SRGB,

	// ETC2
	ETC2_RGB8_UNorm,
	ETC2_RGB8_SRGB,
	ETC2_RGBA8_UNorm,
	ETC2_RGBA8_SRGB,

	// YUV / Video
	YUV_NV12,
	YUV_420P
};

enum class LoadOp : uint8_t
{
	Load = 0,
	Clear,
	DontCare,
	None,
	Invalid = 0xFF
};

enum class StoreOp : uint8_t
{
	Store = 0,
	DontCare,
	None
};

enum class ResolveMode : uint8_t
{
	None = 0,
	SampleZero,
	Average,
	Min,
	Max,
};

enum class DefaultSampler : uint32_t
{
	LinearRepeat = 0,
	LinearClamp,
	NearestClamp,
	AnisotropicRepeat,
	ShadowCompare,

	Count
};

enum class ShaderStage : uint32_t
{
	None = 0,

	Vertex,
	Fragment,
	Compute,

	RayGen,
	Miss,
	ClosestHit,
	AnyHit,
	Intersection,
	Callable,

	Mesh,
	Task
};
