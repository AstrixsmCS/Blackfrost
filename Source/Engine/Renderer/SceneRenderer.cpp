#include "SceneRenderer.hpp"

#include "MaterialSystem.hpp"

#include "Renderer/Vulkan/Descriptors.hpp"
#include "Renderer/Vulkan/Shader.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>

#include "Vulkan/VulkanUtils.hpp"

static constexpr VkImageAspectFlags    DEPTH_STENCIL_ASPECTS = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
static constexpr VkPipelineStageFlags2 DEPTH_STAGES          = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
static constexpr VkPipelineStageFlags2 DEPTH_READ_STAGES     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
static constexpr VkPipelineStageFlags2 POST_READ_STAGES      = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

static uint32_t DivideRoundUp(uint32_t value, uint32_t divisor)
{
	return (value + divisor - 1) / divisor;
}

SceneRenderer::SceneRenderer(SceneRendererSpecification specification)
	: m_Specification(specification)
{
	Initialize();
}

SceneRenderer::~SceneRenderer()
{
	Shutdown();
}

void SceneRenderer::Initialize()
{
	static_assert(sizeof(UBCamera) == 6 * sizeof(glm::mat4));

	assert(!m_Initialized);

	m_ViewportWidth  = m_Specification.ViewportWidth;
	m_ViewportHeight = m_Specification.ViewportHeight;

	// 0 means application window size
	if (m_ViewportWidth == 0 || m_ViewportHeight == 0)
	{
		const VkExtent2D extent = Renderer::GetSwapChain().GetExtent();
		m_ViewportWidth         = extent.width;
		m_ViewportHeight        = extent.height;
	}

	m_NeedsResize = m_ViewportWidth > 0 && m_ViewportHeight > 0;

	m_DrawList.reserve(64);

	// Uniform buffers (one per frame in flight)
	for (uint32_t i = 0; i < Renderer::GetFramesInFlight(); ++i)
	{
		m_CameraBuffers[i].Create({ .DebugName = "Camera Buffer", .Usage = BufferUsage::Uniform, .Memory = BufferMemory::HostVisible, .Size = sizeof(UBCamera) });
		m_SceneBuffers[i].Create({ .DebugName = "Scene Buffer", .Usage = BufferUsage::Uniform, .Memory = BufferMemory::HostVisible, .Size = sizeof(UBScene) });

		m_PointLightBuffers[i].Create({ .DebugName = "Point Lights", .Usage = BufferUsage::Uniform, .Memory = BufferMemory::HostVisible, .Size = sizeof(UBPointLights) });
		m_SpotLightBuffers[i].Create({ .DebugName = "Spot Lights", .Usage = BufferUsage::Uniform, .Memory = BufferMemory::HostVisible, .Size = sizeof(UBSpotLights) });
	}

	ShaderLibrary& shaders = Renderer::GetShaderLibrary();

	// Geometry pass
	{
		m_GBufferMaterial.SetShader(shaders.Get("Mesh"));

		m_GBufferState.VertexLayout =
		{
			{ ShaderDataType::Float3, "Position" },
			{ ShaderDataType::Float3, "Normal" },
			{ ShaderDataType::Float2, "TexCoord" },
			{ ShaderDataType::Float4, "Tangent" },
		};

		m_GBufferState.DepthTest    = true;
		m_GBufferState.DepthWrite   = true;
		m_GBufferState.DepthCompare = CompareOp::Greater; // Reversed-Z
		m_GBufferState.CullMode     = CullMode::Back;
	}

	// Lighting pass
	{
		m_LightingMaterial.SetShader(shaders.Get("Lighting"));
	}

	// Tonemap pass
	{
		m_TonemapMaterial.SetShader(shaders.Get("Tonemap"));
	}

	// Composite pass
	{
		m_CompositeMaterial.SetShader(shaders.Get("Composite"));

		m_CompositeState.DepthTest  = false;
		m_CompositeState.DepthWrite = false;
		m_CompositeState.CullMode   = CullMode::None;
	}

	m_Initialized = true;
}

void SceneRenderer::Shutdown()
{
	if (!m_Initialized)
		return;

	assert(!m_Active);

	Renderer::WaitForGPU();

	m_DrawList.clear();

	m_LDRColor.Destroy();
	m_HDRColor.Destroy();

	m_GBuffer.DepthStencil.Destroy();
	m_GBuffer.Emissive.Destroy();
	m_GBuffer.Material.Destroy();
	m_GBuffer.Normal.Destroy();
	m_GBuffer.Albedo.Destroy();

	m_CompositeMaterial.SetShader(nullptr);
	m_TonemapMaterial.SetShader(nullptr);
	m_LightingMaterial.SetShader(nullptr);
	m_GBufferMaterial.SetShader(nullptr);

	for (uint32_t i = 0; i < Renderer::GetFramesInFlight(); ++i)
	{
		m_SpotLightBuffers[i].Destroy();
		m_PointLightBuffers[i].Destroy();
		m_SceneBuffers[i].Destroy();
		m_CameraBuffers[i].Destroy();
	}

	m_Initialized = false;
}

void SceneRenderer::SetViewportSize(uint32_t width, uint32_t height)
{
	if (m_ViewportWidth == width && m_ViewportHeight == height)
		return;

	m_ViewportWidth  = width;
	m_ViewportHeight = height;
	m_NeedsResize    = true;
}

void SceneRenderer::BeginScene(const SceneRendererCamera& camera, const LightEnvironment& lights)
{
	assert(m_Initialized);
	assert(!m_Active);

	m_Active = true;

	if (m_NeedsResize && m_ViewportWidth > 0 && m_ViewportHeight > 0)
		ResizeTargets();

	UpdateUniformBuffers(camera, lights);
}

void SceneRenderer::EndScene()
{
	assert(m_Active);

	FlushDrawList();

	m_Active = false;
}

void SceneRenderer::SubmitMesh(const Mesh& mesh, const glm::mat4& transform)
{
	assert(m_Active);

	for (const std::shared_ptr<Material>& material : mesh.GetMaterials())
		MaterialSystem::PrepareMaterial(material);

	m_DrawList.push_back({ .Mesh = &mesh, .Transform = transform });
}

void SceneRenderer::ResizeTargets()
{
	m_NeedsResize = false;

	// Frames in flight may still use the old targets.
	Renderer::WaitForGPU();

	const Dimensions size = { m_ViewportWidth, m_ViewportHeight, 1 };

	// Geometry
	//   Albedo       RGBA8_SRGB         rgb = base color,          a = ambient occlusion (alpha is always linear)
	//   Normal       RG16_UNorm         xy = octahedral-encoded world-space normal
	//   Material     RGBA8_UNorm        r = unused, g = roughness, b = metallic (glTF metallicRoughness packing)
	//   Emissive     RGBA16_Float       rgb = emitted radiance,    a = unused
	//   DepthStencil D32_Float_S8_UInt  depth sampled by the lighting pass, stencil reserved
	constexpr TextureUsageFlags gbufferUsage = TextureUsageBits_Attachment | TextureUsageBits_Sampled;

	m_GBuffer.Albedo.Destroy();
	m_GBuffer.Normal.Destroy();
	m_GBuffer.Material.Destroy();
	m_GBuffer.Emissive.Destroy();
	m_GBuffer.DepthStencil.Destroy();

	m_GBuffer.Albedo.Create({ .Format = Format::RGBA8_SRGB, .Size = size, .Usage = gbufferUsage, .DebugName = "Albedo/AO" });
	m_GBuffer.Normal.Create({ .Format = Format::RG16_UNorm, .Size = size, .Usage = gbufferUsage, .DebugName = "Normal" });
	m_GBuffer.Material.Create({ .Format = Format::RGBA8_UNorm, .Size = size, .Usage = gbufferUsage, .DebugName = "Roughness/Metallic" });
	m_GBuffer.Emissive.Create({ .Format = Format::RGBA16_Float, .Size = size, .Usage = gbufferUsage, .DebugName = "Emissive" });
	m_GBuffer.DepthStencil.Create({ .Format = Format::D32_Float_S8_UInt, .Size = size, .Usage = gbufferUsage, .DebugName = "Depth/Stencil" });

	// Lighting
	m_HDRColor.Destroy();
	m_HDRColor.Create(
	{
		.Format    = Format::RGBA16_Float,
		.Size      = size,
		.Usage     = TextureUsageBits_Storage | TextureUsageBits_Attachment | TextureUsageBits_Sampled,
		.DebugName = "HDR Color",
	});

	// Tonemap
	m_LDRColor.Destroy();
	m_LDRColor.Create(
	{
		.Format    = Format::RGBA8_UNorm,
		.Size      = size,
		.Usage     = TextureUsageBits_Storage | TextureUsageBits_Sampled,
		.DebugName = "LDR Color",
	});
}

void SceneRenderer::UpdateUniformBuffers(const SceneRendererCamera& camera, const LightEnvironment& lights)
{
	const uint32_t frameSlot = Renderer::GetFrameSlot();

	// Camera
	{
		const glm::mat4& projection        = camera.Camera.GetProjection();
		const glm::mat4  viewInverse       = glm::inverse(camera.ViewMatrix);
		const glm::mat4  projectionInverse = glm::inverse(projection);

		m_CameraUB.ViewProjection        = projection * camera.ViewMatrix;
		m_CameraUB.InverseViewProjection = viewInverse * projectionInverse;
		m_CameraUB.Projection            = projection;
		m_CameraUB.InverseProjection     = projectionInverse;
		m_CameraUB.View                  = camera.ViewMatrix;
		m_CameraUB.InverseView           = viewInverse;

		m_CameraBuffers[frameSlot].SetData(&m_CameraUB, sizeof(m_CameraUB));

		m_SceneUB.CameraPosition = viewInverse[3];
	}

	// Scene (first directional light only)
	{
		const DirectionalLight& directionalLight = lights.DirectionalLights[0];

		m_SceneUB.Lights.Direction = directionalLight.Direction;
		m_SceneUB.Lights.Radiance  = directionalLight.Radiance;
		m_SceneUB.Lights.Intensity = directionalLight.Intensity;

		m_SceneBuffers[frameSlot].SetData(&m_SceneUB, sizeof(m_SceneUB));
	}

	// Point lights
	{
		m_PointLightsUB.Count = static_cast<uint32_t>(std::min<size_t>(lights.PointLights.size(), MAX_POINT_LIGHTS));
		std::memcpy(m_PointLightsUB.PointLights, lights.PointLights.data(), m_PointLightsUB.Count * sizeof(PointLight));

		m_PointLightBuffers[frameSlot].SetData(&m_PointLightsUB, offsetof(UBPointLights, PointLights) + m_PointLightsUB.Count * sizeof(PointLight));
	}

	// Spot lights
	{
		m_SpotLightsUB.Count = static_cast<uint32_t>(std::min<size_t>(lights.SpotLights.size(), MAX_SPOT_LIGHTS));
		std::memcpy(m_SpotLightsUB.SpotLights, lights.SpotLights.data(), m_SpotLightsUB.Count * sizeof(SpotLight));

		m_SpotLightBuffers[frameSlot].SetData(&m_SpotLightsUB, offsetof(UBSpotLights, SpotLights) + m_SpotLightsUB.Count * sizeof(SpotLight));
	}
}

void SceneRenderer::FlushDrawList()
{
	MaterialSystem::UploadPendingMaterials(Renderer::GetFrameSlot());

	m_CommandBuffer = &Renderer::AcquireCommandBuffer();

	if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
	{
		GeometryPass();
		LightingPass();

		TonemapPass();
		CompositePass();
	}
	else
	{
		ClearPass();
	}

	m_DrawList.clear();

	m_CommandBuffer = nullptr;
}

void SceneRenderer::GeometryPass()
{
	CommandBuffer&                 commandBuffer = *m_CommandBuffer;
	const std::shared_ptr<Shader>& shader        = m_GBufferMaterial.GetShader();

	for (const Texture* texture : { &m_GBuffer.Albedo, &m_GBuffer.Normal, &m_GBuffer.Material, &m_GBuffer.Emissive })
	{
		commandBuffer.ImageBarrier(texture->GetHandle(), VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
	}

	commandBuffer.ImageBarrier(m_GBuffer.DepthStencil.GetHandle(), DEPTH_STENCIL_ASPECTS,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		DEPTH_READ_STAGES, DEPTH_STAGES);

	auto colorAttachment = [](Texture& texture) -> RenderingAttachmentInfo
	{
		return
		{
			.ImageView  = texture.GetAttachmentView(),
			.LoadOp     = LoadOp::Clear,
			.StoreOp    = StoreOp::Store,
			.ClearValue = { .Color = { .Float32 = { 0.0f, 0.0f, 0.0f, 0.0f } } },
			.Layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};
	};

	const std::array<RenderingAttachmentInfo, 4> colors
	{
		colorAttachment(m_GBuffer.Albedo),
		colorAttachment(m_GBuffer.Normal),
		colorAttachment(m_GBuffer.Material),
		colorAttachment(m_GBuffer.Emissive),
	};

	const RenderingAttachmentInfo depth
	{
		.ImageView  = m_GBuffer.DepthStencil.GetAttachmentView(),
		.LoadOp     = LoadOp::Clear,
		.StoreOp    = StoreOp::Store,
		.ClearValue = { .DepthStencil = { 0.0f, 0 } }, // Reversed-Z: 0.0 is infinitely far
		.Layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	const RenderingInfo passInfo
	{
		.ColorAttachments = { colors.data(), colors.size() },
		.DepthAttachment  = &depth,
		.RenderArea       = { .X = 0, .Y = 0, .Width = m_ViewportWidth, .Height = m_ViewportHeight },
	};

	commandBuffer.BeginRendering(passInfo);
	{
		DebugLabelScope label(commandBuffer, "Geometry Pass", 0xAEC6CFFF);

		shader->Bind(commandBuffer);
		commandBuffer.SetGraphicsState(m_GBufferState, static_cast<uint32_t>(colors.size()));

		const VkDescriptorSet descriptorSet = Descriptor::GetSet();
		vkCmdBindDescriptorSets(commandBuffer.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, shader->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

		const uint32_t           frameSlot = Renderer::GetFrameSlot();
		const PushConstantRange& range     = shader->GetPushConstantRanges()[0];

		m_GBufferMaterial.Set("UBCamera", m_CameraBuffers[frameSlot].GetDeviceAddress());
		m_GBufferMaterial.Set("SBMaterials", MaterialSystem::GetBuffer(frameSlot).GetDeviceAddress());

		// Cull mode is per material (glTF doubleSided -> CullMode::None) only change it when it differs.
		CullMode currentCullMode = m_GBufferState.CullMode;

		for (const DrawCommand& drawCommand : m_DrawList)
		{
			const Mesh&                                   mesh      = *drawCommand.Mesh;
			const std::vector<std::shared_ptr<Material>>& materials = mesh.GetMaterials();

			commandBuffer.BindVertexBuffer(mesh.GetVertexBuffer());
			commandBuffer.BindIndexBuffer(mesh.GetIndexBuffer(), IndexFormat::UInt32);

			for (const Submesh& submesh : mesh.GetSubmeshes())
			{
				const bool hasMaterial = submesh.MaterialIndex < materials.size();

				// Already registered in SubmitMesh so this is a lookup.
				const uint32_t materialIndex = hasMaterial ? MaterialSystem::PrepareMaterial(materials[submesh.MaterialIndex]) : MaterialSystem::FallbackIndex;
				const CullMode cullMode      = hasMaterial ? materials[submesh.MaterialIndex]->GetCullMode() : CullMode::Back;

				if (cullMode != currentCullMode)
				{
					vkCmdSetCullMode(commandBuffer.GetHandle(), ToVulkan(cullMode));
					currentCullMode = cullMode;
				}

				m_GBufferMaterial.Set("Model", drawCommand.Transform * submesh.Transform);
				m_GBufferMaterial.Set("MaterialIndex", materialIndex);

				const std::vector<uint8_t>& storage = m_GBufferMaterial.GetUniformStorage();
				commandBuffer.PushConstants(shader->GetPipelineLayout(), range.StageFlags, storage.data() + range.Offset, range.Size, range.Offset);

				commandBuffer.DrawIndexed(submesh.IndexCount, 1, submesh.BaseIndex, static_cast<int32_t>(submesh.BaseVertex));
			}
		}
	}
	commandBuffer.EndRendering();
}

void SceneRenderer::LightingPass()
{
	CommandBuffer&                 commandBuffer = *m_CommandBuffer;
	const std::shared_ptr<Shader>& shader        = m_LightingMaterial.GetShader();

	for (const Texture* texture : { &m_GBuffer.Albedo, &m_GBuffer.Normal, &m_GBuffer.Material, &m_GBuffer.Emissive })
	{
		commandBuffer.ImageBarrier(texture->GetHandle(), VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
	}

	commandBuffer.ImageBarrier(m_GBuffer.DepthStencil.GetHandle(), DEPTH_STENCIL_ASPECTS,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		DEPTH_STAGES, DEPTH_READ_STAGES);

	commandBuffer.ImageBarrier(m_HDRColor.GetHandle(), VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		POST_READ_STAGES, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

	DebugLabelScope label(commandBuffer, "Lighting Pass", 0xFFD27FFF);

	shader->Bind(commandBuffer);

	const VkDescriptorSet descriptorSet = Descriptor::GetSet();
	vkCmdBindDescriptorSets(commandBuffer.GetHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, shader->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

	const uint32_t frameSlot = Renderer::GetFrameSlot();

	m_LightingMaterial.Set("UBCamera", m_CameraBuffers[frameSlot].GetDeviceAddress());
	m_LightingMaterial.Set("UBScene", m_SceneBuffers[frameSlot].GetDeviceAddress());
	m_LightingMaterial.Set("UBPointLights", m_PointLightBuffers[frameSlot].GetDeviceAddress());
	m_LightingMaterial.Set("UBSpotLights", m_SpotLightBuffers[frameSlot].GetDeviceAddress());

	m_LightingMaterial.Set("AlbedoIndex", m_GBuffer.Albedo.GetBindlessIndex());
	m_LightingMaterial.Set("NormalIndex", m_GBuffer.Normal.GetBindlessIndex());
	m_LightingMaterial.Set("MaterialIndex", m_GBuffer.Material.GetBindlessIndex());
	m_LightingMaterial.Set("EmissiveIndex", m_GBuffer.Emissive.GetBindlessIndex());
	m_LightingMaterial.Set("DepthIndex", m_GBuffer.DepthStencil.GetBindlessIndex());
	m_LightingMaterial.Set("OutputIndex", m_HDRColor.GetStorageIndex());
	m_LightingMaterial.Set("Extent", glm::uvec2(m_ViewportWidth, m_ViewportHeight));

	const PushConstantRange&    range   = shader->GetPushConstantRanges()[0];
	const std::vector<uint8_t>& storage = m_LightingMaterial.GetUniformStorage();
	commandBuffer.PushConstants(shader->GetPipelineLayout(), range.StageFlags, storage.data() + range.Offset, range.Size, range.Offset);

	commandBuffer.Dispatch(DivideRoundUp(m_ViewportWidth, 8), DivideRoundUp(m_ViewportHeight, 8));
}

void SceneRenderer::TonemapPass()
{
	CommandBuffer&                 commandBuffer = *m_CommandBuffer;
	const std::shared_ptr<Shader>& shader        = m_TonemapMaterial.GetShader();

	commandBuffer.ImageBarrier(m_HDRColor.GetHandle(), VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

	commandBuffer.ImageBarrier(m_LDRColor.GetHandle(), VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

	DebugLabelScope label(commandBuffer, "Tonemap Pass", 0xC3A6FFFF);

	shader->Bind(commandBuffer);

	const VkDescriptorSet descriptorSet = Descriptor::GetSet();
	vkCmdBindDescriptorSets(commandBuffer.GetHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, shader->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

	m_TonemapMaterial.Set("InputIndex", m_HDRColor.GetBindlessIndex());
	m_TonemapMaterial.Set("OutputIndex", m_LDRColor.GetStorageIndex());
	m_TonemapMaterial.Set("Extent", glm::uvec2(m_ViewportWidth, m_ViewportHeight));
	m_TonemapMaterial.Set("Exposure", m_Exposure);

	const PushConstantRange&    range   = shader->GetPushConstantRanges()[0];
	const std::vector<uint8_t>& storage = m_TonemapMaterial.GetUniformStorage();
	commandBuffer.PushConstants(shader->GetPipelineLayout(), range.StageFlags, storage.data() + range.Offset, range.Size, range.Offset);

	commandBuffer.Dispatch(DivideRoundUp(m_ViewportWidth, 8), DivideRoundUp(m_ViewportHeight, 8));
}

// Copies the LDR target to the swapchain.
void SceneRenderer::CompositePass()
{
	CommandBuffer&                 commandBuffer = *m_CommandBuffer;
	const std::shared_ptr<Shader>& shader        = m_CompositeMaterial.GetShader();
	SwapChain&                     swapChain     = Renderer::GetSwapChain();
	const VkExtent2D               extent        = swapChain.GetExtent();

	commandBuffer.ImageBarrier(m_LDRColor.GetHandle(), VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

	commandBuffer.ImageBarrier(swapChain.GetCurrentImage(), VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

	const RenderingAttachmentInfo color
	{
		.ImageView  = swapChain.GetCurrentImageView(),
		.LoadOp     = LoadOp::Clear,
		.StoreOp    = StoreOp::Store,
		.ClearValue = { .Color = { .Float32 = { 0.0f, 0.0f, 0.0f, 1.0f } } },
		.Layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	const RenderingInfo passInfo
	{
		.ColorAttachments = { &color, 1 },
		.RenderArea       = { .X = 0, .Y = 0, .Width = extent.width, .Height = extent.height },
	};

	commandBuffer.BeginRendering(passInfo);
	{
		DebugLabelScope label(commandBuffer, "Composite Pass", 0xB0E57CFF);

		shader->Bind(commandBuffer);
		commandBuffer.SetGraphicsState(m_CompositeState, 1);

		const VkDescriptorSet descriptorSet = Descriptor::GetSet();
		vkCmdBindDescriptorSets(commandBuffer.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, shader->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

		m_CompositeMaterial.Set("InputIndex", m_LDRColor.GetBindlessIndex());

		const PushConstantRange&    range   = shader->GetPushConstantRanges()[0];
		const std::vector<uint8_t>& storage = m_CompositeMaterial.GetUniformStorage();
		commandBuffer.PushConstants(shader->GetPipelineLayout(), range.StageFlags, storage.data() + range.Offset, range.Size, range.Offset);

		commandBuffer.Draw(3);
	}
	commandBuffer.EndRendering();

	commandBuffer.ImageBarrier(swapChain.GetCurrentImage(), VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_NONE);
}

// Zero-sized viewport clear the swapchain so the frame still presents a valid image.
void SceneRenderer::ClearPass()
{
	CommandBuffer&   commandBuffer = *m_CommandBuffer;
	SwapChain&       swapChain     = Renderer::GetSwapChain();
	const VkExtent2D extent        = swapChain.GetExtent();

	commandBuffer.ImageBarrier(swapChain.GetCurrentImage(), VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

	const RenderingAttachmentInfo color
	{
		.ImageView  = swapChain.GetCurrentImageView(),
		.LoadOp     = LoadOp::Clear,
		.StoreOp    = StoreOp::Store,
		.ClearValue = { .Color = { .Float32 = { 0.0f, 0.0f, 0.0f, 1.0f } } },
		.Layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	const RenderingInfo passInfo
	{
		.ColorAttachments = { &color, 1 },
		.RenderArea       = { .X = 0, .Y = 0, .Width = extent.width, .Height = extent.height },
	};

	commandBuffer.BeginRendering(passInfo);
	commandBuffer.EndRendering();

	commandBuffer.ImageBarrier(swapChain.GetCurrentImage(), VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_NONE);
}
