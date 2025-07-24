#ifndef TEXTURE_H
#define TEXTURE_H

#include <vulkan/vulkan.h>
#include <string>

class Texture
{
  public:
	struct VulkanDevice {
		VkDevice logicalDevice;
		VkPhysicalDevice physicalDevice;
	} vkdevice;
	VkCommandPool commandPool;
	VkQueue graphicsQueue;
	VkImage               textureImage;
	VkImageLayout         textureImageLayout;
	VkDeviceMemory        textureImageMemory;
	VkImageView           textureImageView;
	uint32_t              width, height;
	uint32_t              mipLevels;
	uint32_t              layerCount;
	VkDescriptorImageInfo descriptor;
	VkSampler             textureSampler;

	Texture();
	~Texture();
	void loadFromFile(std::string filename, VkDevice logicalDevice,	VkPhysicalDevice physicalDevice,VkCommandPool commandPool, VkQueue graphicsQueue);
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	void createTextureSampler();
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
};

#endif // TEXTURE_H