#ifndef GLTF_SCENE_H
#define GLTF_SCENE_H
#include <vulkan/vulkan.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
// #define TINYGLTF_IMPLEMENTATION
// #define STB_IMAGE_IMPLEMENTATION
// #define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#include "rast/texture.h"

class VulkanglTFScene
{
public:
	// The class requires some Vulkan objects so it can create it's own resources
	struct VulkanDevice {
		VkDevice logicalDevice;
		VkPhysicalDevice physicalDevice;
		VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	} vkdevice;

	VkQueue graphicsQueue;
	VkCommandPool commandPool;

	// The vertex layout for the samples' model
	struct Vertex {
		glm::vec3 pos;
		// glm::vec3 normal;
		glm::vec3 color;
		glm::vec2 uv;
		// glm::vec4 tangent;
	};

	// Single vertex buffer for all primitives
	// struct {
	// 	VkBuffer buffer;
	// 	VkDeviceMemory memory;
	// } vertices;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;


	// Single index buffer for all primitives
	// struct {
	// 	int count;
	// 	VkBuffer buffer;
	// 	VkDeviceMemory memory;
	// } indices;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	// The following structures roughly represent the glTF scene structure
	// To keep things simple, they only contain those properties that are required for this sample
	struct Node;

	// A primitive contains the data for a single draw call
	struct Primitive {
		uint32_t firstIndex;
		uint32_t indexCount;
		int32_t materialIndex;
	};

	// Contains the node's (optional) geometry and can be made up of an arbitrary number of primitives
	struct Mesh {
		std::vector<Primitive> primitives;
	};

	// A node represents an object in the glTF scene graph
	struct Node {
		Node* parent;
		std::vector<Node*> children;
		Mesh mesh;
		glm::mat4 matrix;
		std::string name;
		bool visible = true;
		~Node() {
			for (auto& child : children) {
				delete child;
			}
		}
	};

	// A glTF material stores information in e.g. the texture that is attached to it and colors
	struct Material {
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		uint32_t baseColorTextureIndex;
		uint32_t normalTextureIndex;
		uint32_t metallicRoughnessTextureIndex;
		std::string alphaMode = "OPAQUE";
		float alphaCutOff;
		bool doubleSided = false;
		std::vector<VkDescriptorSet> descriptorSets;
		// VkPipeline pipeline;
	};

	// Contains the texture for a single glTF image
	// Images may be reused by texture objects and are as such separated
	struct Image {
		Texture texture;
	};

	// A glTF texture stores a reference to the image and a sampler
	// In this sample, we are only interested in the image
	struct TextureIdx {
		int32_t imageIndex;
	};

	/*
		Model data
	*/
	std::vector<Texture> images;
	std::vector<int32_t> textures;
	std::vector<Material> materials;
	std::vector<Node*> nodes;

	std::vector<VkDescriptorSet> descriptorSets;

	std::string path;

	~VulkanglTFScene();

	void loadglTFFile(std::string filename, VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, VkCommandPool commandPool);
	VkDescriptorImageInfo getTextureDescriptor(const size_t index);
	void loadImages(tinygltf::Model& input);
	void loadTextures(tinygltf::Model& input);
	void loadMaterials(tinygltf::Model& input);
	void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, VulkanglTFScene::Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<VulkanglTFScene::Vertex>& vertexBuffer);
	void createVertexBuffer();
	void createIndexBuffer();
	uint32_t findMaterialCount();
	void drawNode(VkPipeline graphicsPipeline, VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VulkanglTFScene::Node* node,int CURRENT_FRAME);
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	VkCommandBuffer beginSingleTimeCommands();
	// void setDescriptors(VkDescriptorPool descriptorPool, std::vector<VkDescriptorSet>& descriptorSets, VkDescriptorSetLayout descriptorSetLayout, const int MAX_FRAMES_IN_FLIGHT, std::vector<VkBuffer> uniformBuffers, std::size_t ubo_size);
	void createDescriptorSets(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, const int MAX_FRAMES_IN_FLIGHT, std::vector<VkBuffer> uniformBuffers, std::size_t ubo_size);
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);
	void draw(VkPipeline graphicsPipeline, VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, int CURRENT_FRAME);
};

#endif // GLTF_SCENE_H
