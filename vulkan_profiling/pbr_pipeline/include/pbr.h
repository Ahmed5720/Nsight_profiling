#ifndef PBR_H
#define PBR_H

#include <string>
#include <memory>
#include "vulkanexamplebase.h"


class PBR: public VulkanExampleBase {
	private: 
	VulkanglTFScene glTFScene;
	std::string model_path;
	glm::mat4 view_cust;
	glm::mat4 proj_cust;
	glm::mat4 model_cust;
	glm::vec3 cam_pos;

	float light_strength = 1.0f;
	float ambient_strength = 0.01f;

	bool use_shadow = true;
	bool use_pcf = true;

	struct ShaderData {
		vks::Buffer buffer;
		struct Values {
			glm::mat4 projection;
			glm::mat4 view;
			glm::mat4 lightSpaceMatrix[6];
			glm::vec4 viewPos;
		} values;
	} shaderData;

	struct LightDir{
		vks::Buffer buffer;
		struct Values {
			glm::vec4 lightdir[6] = {
				{1.0f, 0.0f, 0.0f, 1.0f},
				{0.0f, 1.0f, 0.0f, 1.0f},
				{0.0f, 0.0f, 1.0f, 1.0f},
				{-1.0f, 0.0f, 0.0f, 1.0f},
				{0.0f, -1.0f, 0.0f, 1.0f},
				{0.0f, 0.0f, -1.0f, 1.0f}
			};
		} values;
	} lightDir;

	struct OffscreenData {
		vks::Buffer buffer;
		struct Values {
			glm::mat4 depthMVP[6];
		} values;
	} offscreenData;

	struct OffscreenPC {
		glm::mat4 model;
		int index;
	};

	// Framebuffer for offscreen rendering
	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	};
	struct OffscreenPass {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment depth;
		VkSampler depthSampler;
		VkDescriptorImageInfo descriptor;
	} ;

	std::vector<VkCommandBuffer> shadowCmdBuffer;

	OffscreenPass offscreenPass[6] = {{}};

	VkRenderPass renderPassOffscreen{ VK_NULL_HANDLE };

	// 16 bits of depth is enough for such a small scene
	const VkFormat offscreenDepthFormat{ VK_FORMAT_D16_UNORM };
	const uint32_t shadowMapize{ 2048 };

	// Depth bias (and slope) are used to avoid shadowing artifacts
	// Constant depth bias factor (always applied)
	float depthBiasConstant = 1.25f;
	// Slope depth bias factor, applied depending on polygon's slope
	float depthBiasSlope = 1.75f;

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayoutOffscreen{ VK_NULL_HANDLE };
	VkPipeline pipelineOffscreen{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorPool descriptorPoolOffscreen{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayoutOffscreen{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSetOffscreen{ VK_NULL_HANDLE };
	// VkDescriptorSet descriptorSetLights{ VK_NULL_HANDLE };

	struct DescriptorSetLayouts {
		VkDescriptorSetLayout matrices{ VK_NULL_HANDLE };
		VkDescriptorSetLayout textures{ VK_NULL_HANDLE };
		VkDescriptorSetLayout lights{ VK_NULL_HANDLE };
	} descriptorSetLayouts;

	struct DescriptorSetLAyoutsOffscreen {
		VkDescriptorSetLayout matrices{ VK_NULL_HANDLE };
		VkDescriptorSetLayout textures{ VK_NULL_HANDLE };
	} descriptorSetLayoutsOffscreen;
	// virtual void getEnabledFeatures() {
	// 	if (deviceFeatures.samplerAnisotropy) {
	// 		enabledFeatures.samplerAnisotropy = VK_TRUE;
	// 	}
	// }
	virtual void getEnabledFeatures();
	void buildCommandBuffers();
	void buildCommandBuffer(uint32_t currentBuffer);
	void buildOffscreenCommandBuffer(int index);
	void loadglTFFile(std::string filename);
	void loadAssets();
	void setupDescriptors();
	void setupDescriptorsOffscreen();
	void preparePipelines();
	void prepareUniformBuffers();
	void prepareOffscreenRenderpass();
	void prepareOffscreenFramebuffer(int index);
	void prepareOffscreenPipeline();
	void generateShadowMap();
	void saveScreenshot(std::string filename, uint32_t currentImage);
	// void saveScreenshotOffscreen(std::string filename, uint32_t currentImage);
	void updateUniformBuffers();
	void prepare();
	void render();
	template <size_t N>
    VkShaderModule createShaderModule(const uint32_t (&source)[N]);
	// virtual void draw();
	// virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay);
	public:
		PBR();
		~PBR();
		void SetMatrices(const float* view, const float* proj, const float* model, const float* camPos);
		void SetModelPath(const std::string& model_p);
		void SetOutputPath(const std::string& output_p);
		void SetUseShadow(bool use_shadow = true, bool use_pcf = true) {
			this->use_shadow = use_shadow;
			this->use_pcf = use_pcf;
		}
		void SetLightStrength(float strength, float ambient_strength = 0.01f) {
			this->light_strength = strength;
			this->ambient_strength = ambient_strength;
		}
		void run();
		// void ConfigureLighting(const float* light_position, const float* light_color);
	// private:
  	// class Impl;
  	// std::shared_ptr<Impl> impl_;
};

#endif // PBR_H