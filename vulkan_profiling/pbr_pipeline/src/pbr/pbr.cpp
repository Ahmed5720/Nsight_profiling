/*
* Vulkan Example - Physical based rendering a textured object (metal/roughness workflow) with image based lighting
*
* Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

// For reference see http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include <pbr.h>
#include <filesystem>
#include <iostream>
#include "generated/pbr_frag.h"
#include "generated/pbr_vert.h"
#include "generated/pbr_shadow_frag.h"
#include "generated/pbr_shadow_vert.h"
#include "generated/offscreen_frag.h"
#include "generated/offscreen_vert.h"

#include <stb_image_write.h>

void PBR::getEnabledFeatures() {
	enabledFeatures.samplerAnisotropy = deviceFeatures.samplerAnisotropy;
}

void PBR::saveScreenshot(std::string filename, uint32_t currentImage) {
	// std::cout<<"saving screenshot"<<std::endl;
	screenshotSaved = false;
	bool supportsBlit = true;

	// Check blit support for source and destination
	VkFormatProperties formatProps;

	// Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
	vkGetPhysicalDeviceFormatProperties(physicalDevice, swapChain.colorFormat, &formatProps);
	if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
		std::cerr << "Device does not support blitting from optimal tiled images, using copy instead of blit!" << std::endl;
		supportsBlit = false;
	}

	// Check if the device supports blitting to linear images
	vkGetPhysicalDeviceFormatProperties(physicalDevice, VK_FORMAT_R8G8B8A8_SRGB, &formatProps);
	if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
		std::cerr << "Device does not support blitting to linear tiled images, using copy instead of blit!" << std::endl;
		supportsBlit = false;
	}

	// Source for the copy is the last rendered swapchain image
	VkImage srcImage = swapChain.images[currentImage];

	// Create the linear tiled destination image to copy to and to read the memory from
	VkImageCreateInfo imageCreateCI{};
			imageCreateCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
	// Note that vkCmdBlitImage (if supported) will also do format conversions if the swapchain color format would differ
	imageCreateCI.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageCreateCI.extent.width = width;
	imageCreateCI.extent.height = height;
	imageCreateCI.extent.depth = 1;
	imageCreateCI.arrayLayers = 1;
	imageCreateCI.mipLevels = 1;
	imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateCI.tiling = VK_IMAGE_TILING_LINEAR;
	imageCreateCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	// Create the image
	VkImage dstImage;
	if(vkCreateImage(device, &imageCreateCI, nullptr, &dstImage) != VK_SUCCESS){
		throw std::runtime_error("failed to create image!");
	}
	// Create memory to back up the image
	VkMemoryRequirements memRequirements;
	VkMemoryAllocateInfo memAllocInfo{};
			memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkDeviceMemory dstImageMemory;
	vkGetImageMemoryRequirements(device, dstImage, &memRequirements);
	memAllocInfo.allocationSize = memRequirements.size;
	// Memory must be host visible to copy from
	memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if(vkAllocateMemory(device, &memAllocInfo, nullptr, &dstImageMemory) != VK_SUCCESS){
		throw std::runtime_error("failed to allocate image memory!");
	}
	if(vkBindImageMemory(device, dstImage, dstImageMemory, 0) != VK_SUCCESS){
		throw std::runtime_error("failed to bind image memory!");
	}

	// Do the actual blit from the swapchain image to our host visible destination image
	// VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Transition destination image to transfer destination layout
	// vks::tools::insertImageMemoryBarrier(
	// 	copyCmd,
	// 	dstImage,
	// 	0,
	// 	VK_ACCESS_TRANSFER_WRITE_BIT,
	// 	VK_IMAGE_LAYOUT_UNDEFINED,
	// 	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	// 	VK_PIPELINE_STAGE_TRANSFER_BIT,
	// 	VK_PIPELINE_STAGE_TRANSFER_BIT,
	// 	VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
			// std::cout<<"transitioning image"<<std::endl;
			VkImageMemoryBarrier imageMemoryBarrier_1{};
			imageMemoryBarrier_1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier_1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier_1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier_1.srcAccessMask = 0;
			imageMemoryBarrier_1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier_1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageMemoryBarrier_1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageMemoryBarrier_1.image = dstImage;
			imageMemoryBarrier_1.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier_1);

	// Transition swapchain image from present to transfer source layout
	// vks::tools::insertImageMemoryBarrier(
	// 	copyCmd,
	// 	srcImage,
	// 	VK_ACCESS_MEMORY_READ_BIT,
	// 	VK_ACCESS_TRANSFER_READ_BIT,
	// 	VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	// 	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	// 	VK_PIPELINE_STAGE_TRANSFER_BIT,
	// 	VK_PIPELINE_STAGE_TRANSFER_BIT,
	// 	VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
			// std::cout<<"transition swapchain image"<<std::endl;
			VkImageMemoryBarrier imageMemoryBarrier_2{};
			imageMemoryBarrier_2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier_2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier_2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier_2.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			imageMemoryBarrier_2.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			imageMemoryBarrier_2.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			imageMemoryBarrier_2.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageMemoryBarrier_2.image = srcImage;
			imageMemoryBarrier_2.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier_2);

	// If source and destination support blit we'll blit as this also does automatic format conversion (e.g. from BGR to RGB)
	if (supportsBlit)
	{
		// Define the region to blit (we will blit the whole swapchain image)
		VkOffset3D blitSize;
		blitSize.x = width;
		blitSize.y = height;
		blitSize.z = 1;
		VkImageBlit imageBlitRegion{};
		imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlitRegion.srcSubresource.layerCount = 1;
		imageBlitRegion.srcOffsets[1] = blitSize;
		imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlitRegion.dstSubresource.layerCount = 1;
		imageBlitRegion.dstOffsets[1] = blitSize;

		// Issue the blit command
		vkCmdBlitImage(
			copyCmd,
			srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&imageBlitRegion,
			VK_FILTER_NEAREST);
	}
	else
	{
		// Otherwise use image copy (requires us to manually flip components)
		VkImageCopy imageCopyRegion{};
		imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopyRegion.srcSubresource.layerCount = 1;
		imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopyRegion.dstSubresource.layerCount = 1;
		imageCopyRegion.extent.width = width;
		imageCopyRegion.extent.height = height;
		imageCopyRegion.extent.depth = 1;

		// Issue the copy command
		vkCmdCopyImage(
			copyCmd,
			srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&imageCopyRegion);
	}

	// Transition destination image to general layout, which is the required layout for mapping the image memory later on
	// vks::tools::insertImageMemoryBarrier(
	// 	copyCmd,
	// 	dstImage,
	// 	VK_ACCESS_TRANSFER_WRITE_BIT,
	// 	VK_ACCESS_MEMORY_READ_BIT,
	// 	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	// 	VK_IMAGE_LAYOUT_GENERAL,
	// 	VK_PIPELINE_STAGE_TRANSFER_BIT,
	// 	VK_PIPELINE_STAGE_TRANSFER_BIT,
	// 	VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
			// std::cout<<"transition destination image"<<std::endl;
			VkImageMemoryBarrier imageMemoryBarrier_3{};
			imageMemoryBarrier_3.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier_3.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier_3.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier_3.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier_3.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			imageMemoryBarrier_3.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageMemoryBarrier_3.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrier_3.image = dstImage;
			imageMemoryBarrier_3.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier_3);

	// Transition back the swap chain image after the blit is done
	// vks::tools::insertImageMemoryBarrier(
	// 	copyCmd,
	// 	srcImage,
	// 	VK_ACCESS_TRANSFER_READ_BIT,
	// 	VK_ACCESS_MEMORY_READ_BIT,
	// 	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	// 	VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	// 	VK_PIPELINE_STAGE_TRANSFER_BIT,
	// 	VK_PIPELINE_STAGE_TRANSFER_BIT,
	// 	VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
			// std::cout<<"transition back swapchain image"<<std::endl;
			VkImageMemoryBarrier imageMemoryBarrier_4{};
			imageMemoryBarrier_4.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier_4.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier_4.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier_4.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			imageMemoryBarrier_4.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			imageMemoryBarrier_4.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageMemoryBarrier_4.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			imageMemoryBarrier_4.image = srcImage;
			imageMemoryBarrier_4.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier_4);

	vulkanDevice->flushCommandBuffer(copyCmd, queue);

	// Get layout of the image (including row pitch)
	VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
	VkSubresourceLayout subResourceLayout;
	vkGetImageSubresourceLayout(device, dstImage, &subResource, &subResourceLayout);

	// Map image memory so we can start copying from it
	const char* data;
	vkMapMemory(device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
	data += subResourceLayout.offset;

			std::vector<unsigned char> imageData(width * height * 4);

			for (uint32_t y = 0; y < height; y++)
			{
					memcpy(&imageData[y * width * 4],
							data + y * subResourceLayout.rowPitch,
							width * 4);
			}

	// std::ofstream file(filename, std::ios::out | std::ios::binary);

	// ppm header
	// file << "P6\n" << width << "\n" << height << "\n" << 255 << "\n";

	// If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
	bool colorSwizzle = false;
	// Check if source is BGR
	// Note: Not complete, only contains most common and basic BGR surface formats for demonstration purposes
	if (!supportsBlit)
	{
		std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
		colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), swapChain.colorFormat) != formatsBGR.end());
	}

			if (colorSwizzle)
			{
					for (size_t i = 0; i < imageData.size(); i += 4)
							std::swap(imageData[i], imageData[i + 2]); // swap R and B
			}
	
			stbi_write_png(filename.c_str(), width, height, 4, imageData.data(), width * 4);

	// ppm binary pixel data
	// for (uint32_t y = 0; y < HEIGHT; y++)
	// {
	// 	unsigned int *row = (unsigned int*)data;
	// 	for (uint32_t x = 0; x < WIDTH; x++)
	// 	{
	// 		if (colorSwizzle)
	// 		{
	// 			file.write((char*)row+2, 1);
	// 			file.write((char*)row+1, 1);
	// 			file.write((char*)row, 1);
			//             file.write((char*)row+3, 1);
	// 		}
	// 		else
	// 		{
	// 			file.write((char*)row, 4);
	// 		}
	// 		row++;
	// 	}
	// 	data += subResourceLayout.rowPitch;
	// }
	// file.close();

	std::cout << "Screenshot saved to disk" << std::endl;

	// Clean up resources
	vkUnmapMemory(device, dstImageMemory);
	vkFreeMemory(device, dstImageMemory, nullptr);
	vkDestroyImage(device, dstImage, nullptr);

	screenshotSaved = true;
}



void PBR::buildCommandBuffer(uint32_t currentBuffer)
{
	// static auto startTime = std::chrono::high_resolution_clock::now();

  //       auto currentTime = std::chrono::high_resolution_clock::now();
  //       float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	// clearValues[0].color = defaultClearColor;
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	const VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
	const VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);

	// for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
	// {
		renderPassBeginInfo.framebuffer = frameBuffers[currentBuffer];
		VK_CHECK_RESULT(vkResetCommandBuffer(drawCmdBuffers[currentBuffer], 0));
		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[currentBuffer], &cmdBufInfo));
		vkCmdBeginRenderPass(drawCmdBuffers[currentBuffer], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(drawCmdBuffers[currentBuffer], 0, 1, &viewport);
		vkCmdSetScissor(drawCmdBuffers[currentBuffer], 0, 1, &scissor);
		// Bind scene matrices descriptor to set 0
		vkCmdBindDescriptorSets(drawCmdBuffers[currentBuffer], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

		// POI: Draw the glTF scene
		// model_cust = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		glTFScene.draw(drawCmdBuffers[currentBuffer], pipelineLayout, model_cust);

		// drawUI(drawCmdBuffers[currentBuffer]);
		vkCmdEndRenderPass(drawCmdBuffers[currentBuffer]);
		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[currentBuffer]));
	// }
	// std::cout << "Command buffers built" << std::endl;
}

void PBR::buildCommandBuffers()
{
	static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	// clearValues[0].color = defaultClearColor;
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	const VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
	const VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);

	for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
	{
		renderPassBeginInfo.framebuffer = frameBuffers[i];
		VK_CHECK_RESULT(vkResetCommandBuffer(drawCmdBuffers[i], 0));
		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
		vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
		vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
		// Bind scene matrices descriptor to set 0
		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

		// POI: Draw the glTF scene
		// model_cust = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		glTFScene.draw(drawCmdBuffers[i], pipelineLayout, model_cust);

		// drawUI(drawCmdBuffers[i]);
		vkCmdEndRenderPass(drawCmdBuffers[i]);
		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
	std::cout << "Command buffers built" << std::endl;
}

void PBR::loadAssets()
{
	loadglTFFile(model_path);
	// loadglTFFile("/home/nisarg/data/ship_bottle/scene.gltf");
}

void PBR::loadglTFFile(std::string filename) {
	tinygltf::Model glTFInput;
	tinygltf::TinyGLTF gltfContext;
	std::string error, warning;

	this->device = device;
	bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

	// Pass some Vulkan resources required for setup and rendering to the glTF model loading class
	glTFScene.vulkanDevice = vulkanDevice;
	glTFScene.copyQueue    = queue;
	glTFScene.model_cust = model_cust;

	size_t pos = filename.find_last_of('/');
	glTFScene.path = filename.substr(0, pos);

	std::vector<uint32_t> indexBuffer;
	std::vector<VulkanglTFScene::Vertex> vertexBuffer;

	if (fileLoaded) {
		glTFScene.loadImages(glTFInput);
		glTFScene.loadMaterials(glTFInput);
		glTFScene.loadTextures(glTFInput);
		const tinygltf::Scene& scene = glTFInput.scenes[0];
		for (size_t i = 0; i < scene.nodes.size(); i++) {
			const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
			glTFScene.loadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
		}
	}
	else {
		vks::tools::exitFatal("Could not open the glTF file.\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
		return;
	}

	// Create and upload vertex and index buffer
	// We will be using one single vertex buffer and one single index buffer for the whole glTF scene
	// Primitives (of the glTF model) will then index into these using index offsets

	size_t vertexBufferSize = vertexBuffer.size() * sizeof(VulkanglTFScene::Vertex);
	size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
	std::cout << "Vertex buffer size: " << vertexBufferSize / 1024 << " KB" << std::endl;
	std::cout << "Index buffer size: " << indexBufferSize / 1024 << " KB" << std::endl;
	glTFScene.indices.count = static_cast<uint32_t>(indexBuffer.size());

	struct StagingBuffer {
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertexStaging, indexStaging;

	// Create host visible staging buffers (source)
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		vertexBufferSize,
		&vertexStaging.buffer,
		&vertexStaging.memory,
		vertexBuffer.data()));
	// Index data
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		indexBufferSize,
		&indexStaging.buffer,
		&indexStaging.memory,
		indexBuffer.data()));

	// Create device local buffers (target)
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBufferSize,
		&glTFScene.vertices.buffer,
		&glTFScene.vertices.memory));
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBufferSize,
		&glTFScene.indices.buffer,
		&glTFScene.indices.memory));

	// Copy data from staging buffers (host) do device local buffer (gpu)
	VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkBufferCopy copyRegion = {};

	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(
		copyCmd,
		vertexStaging.buffer,
		glTFScene.vertices.buffer,
		1,
		&copyRegion);

	copyRegion.size = indexBufferSize;
	vkCmdCopyBuffer(
		copyCmd,
		indexStaging.buffer,
		glTFScene.indices.buffer,
		1,
		&copyRegion);

	vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

	// Free staging resources
	vkDestroyBuffer(device, vertexStaging.buffer, nullptr);
	vkFreeMemory(device, vertexStaging.memory, nullptr);
	vkDestroyBuffer(device, indexStaging.buffer, nullptr);
	vkFreeMemory(device, indexStaging.memory, nullptr);
}

	// void PBR::loadAssets() {
	// 	glTFScene.loadFromFile(model_path, vulkanDevice, queue);
	// }

void PBR::setupDescriptors() {
	/*
		This sample uses separate descriptor sets (and layouts) for the matrices and materials (textures)
	*/

	// One ubo to pass dynamic data to the shader
	// Two combined image samplers per material as each material uses color and normal maps
	std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),  // 1 for the matrices and one for the lights
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(glTFScene.materials.size()) * 4),
	};
	if(use_shadow) {
		// If shadows are enabled, we need to allocate space for the shadow maps
		// Each shadow map is a combined image sampler, so we need to allocate 6 of them (one for each light direction)
		poolSizes.push_back(vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6)); // 6 shadow maps
	}
	// One set for matrices and one per model image/texture
	uint32_t maxSetCount = static_cast<uint32_t>(glTFScene.images.size()) + 2;
	if (use_shadow) {
		maxSetCount += 6; // 6 shadow maps
	}
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, maxSetCount);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

	// Descriptor set layout for passing matrices
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
	};
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.matrices));

	// Descriptor set layout for passing lights
	// setLayoutBindings = {
	// 	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
	// };
	// descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
	// descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	// VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.lights));

	// Descriptor set layout for passing material textures
	setLayoutBindings = {
		// Color map
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		// Normal map
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		//metallic roughness map
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
		// Emissive Map
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
	};
	if (use_shadow) {
		// If shadows are enabled, we need to allocate space for the shadow maps
		// Each shadow map is a combined image sampler, so we need to allocate 6 of them (one for each light direction)
		setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4, 6)); // 6 shadow maps
	}
	descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
	descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.textures));

	// Descriptor set for scene matrices
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.matrices, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &shaderData.buffer.descriptor),
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &lightDir.buffer.descriptor),
	};
	// VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &shaderData.buffer.descriptor);
	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

	
	VkDescriptorImageInfo shadowMapDescriptor[6];
	// Image descriptor for the shadow map attachment
	for (int i=0;i<6;i++)
		shadowMapDescriptor[i] = vks::initializers::descriptorImageInfo(
				offscreenPass[i].depthSampler,
				offscreenPass[i].depth.view,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

	// Descriptor sets for materials
	for (auto& material : glTFScene.materials) {
		const VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.textures, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &material.descriptorSet));
		VkDescriptorImageInfo colorMap = glTFScene.getTextureDescriptor(material.baseColorTextureIndex);
		VkDescriptorImageInfo normalMap = glTFScene.getTextureDescriptor(material.normalTextureIndex);
		VkDescriptorImageInfo metallicRoughnessMap = glTFScene.getTextureDescriptor(material.metallicRoughnessTextureIndex);
		VkDescriptorImageInfo emissiveMap = glTFScene.getTextureDescriptor(material.emissiveTextureIndex);
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &colorMap),
			vks::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &normalMap),
			vks::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &metallicRoughnessMap),
			vks::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &emissiveMap),
			// vks::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, shadowMapDescriptor,6),	
		};
		if (use_shadow) {
			// If shadows are enabled, we need to allocate space for the shadow maps
			// Each shadow map is a combined image sampler, so we need to allocate 6 of them (one for each light direction)
			writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, shadowMapDescriptor, 6));
		}
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void PBR::setupDescriptorsOffscreen() {
	// std::cout << "Setting up offscreen descriptors" << std::endl;

	std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
	};

	const uint32_t maxSetCount = 1;
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, maxSetCount);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPoolOffscreen));
	
	// Descriptor set layout for UBO of ofscreen pass
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
	};
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayoutOffscreen));

	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPoolOffscreen, &descriptorSetLayoutOffscreen, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSetOffscreen));
	
	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		// Binding 0 : Vertex shader uniform buffer
		vks::initializers::writeDescriptorSet(descriptorSetOffscreen, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &offscreenData.buffer.descriptor),
	};
	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	std::cout << "Offscreen descriptors set up" << std::endl;
}

template <size_t N>
    VkShaderModule PBR::createShaderModule(const uint32_t (&source)[N]) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = sizeof(uint32_t) * N;
        createInfo.pCode = source;

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

void PBR::preparePipelines()
{
	// Layout
	// Pipeline layout uses both descriptor sets (set 0 = matrices, set 1 = material)
	std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptorSetLayouts.matrices, descriptorSetLayouts.textures };
	VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
	// We will use push constants to push the local matrices of a primitive to the vertex shader
	VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
	// Push constant ranges are part of the pipeline layout
	pipelineLayoutCI.pushConstantRangeCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

	// Pipelines
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

	const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		vks::initializers::vertexInputBindingDescription(0, sizeof(VulkanglTFScene::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	};
	const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFScene::Vertex, pos)),
		vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFScene::Vertex, normal)),
		vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(VulkanglTFScene::Vertex, uv)),
		vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFScene::Vertex, color)),
		vks::initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VulkanglTFScene::Vertex, tangent)),
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo(vertexInputBindings, vertexInputAttributes);

	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
	pipelineCI.pVertexInputState = &vertexInputStateCI;
	pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
	pipelineCI.pRasterizationState = &rasterizationStateCI;
	// pipelineCI.pColorBlendState = &colorBlendStateCI;
	pipelineCI.pMultisampleState = &multisampleStateCI;
	pipelineCI.pViewportState = &viewportStateCI;
	
	pipelineCI.pDynamicState = &dynamicStateCI;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();

	// shaderStages[0] = loadShader(getShadersPath() + "scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	// shaderStages[1] = loadShader(getShadersPath() + "scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	VkShaderModule vertShaderModule; //= createShaderModule(pbr_vert);
	VkShaderModule fragShaderModule; //= createShaderModule(pbr_frag);
	if(use_shadow) {
		vertShaderModule = createShaderModule(pbr_shadow_vert);
		fragShaderModule = createShaderModule(pbr_shadow_frag);
	} else {
		vertShaderModule = createShaderModule(pbr_vert);
		fragShaderModule = createShaderModule(pbr_frag);
	}
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].pName = "main";
	shaderStages[0].module = vertShaderModule;

	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].pName = "main";
	shaderStages[1].module = fragShaderModule;

	// POI: Instead if using a few fixed pipelines, we create one pipeline for each material using the properties of that material
	for (auto &material : glTFScene.materials) {

		// alpha blending
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);
		VkPipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		if (material.alphaMode == "BLEND") {
			// VkPipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_TRUE);
			blendAttachmentStateCI.blendEnable = VK_TRUE;
			blendAttachmentStateCI.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blendAttachmentStateCI.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			blendAttachmentStateCI.colorBlendOp = VK_BLEND_OP_ADD;
			blendAttachmentStateCI.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blendAttachmentStateCI.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			blendAttachmentStateCI.alphaBlendOp = VK_BLEND_OP_ADD;
			blendAttachmentStateCI.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			depthStencilStateCI.depthWriteEnable = VK_FALSE;
			// VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
			// pipelineCI.pColorBlendState = &colorBlendStateCI;
		}
			// VkPipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
		colorBlendStateCI.logicOpEnable = VK_FALSE;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		// pipelineCI.pColorBlendState = &colorBlendStateCI;
		struct MaterialSpecializationData {
			VkBool32 alphaMask;
			float alphaMaskCutoff;
			VkBool32 useMetalicTexture;
			float metallicFactor;
			float roughnessFactor;
			float aoFactor;
			float colorFactorR;
			float colorFactorG;
			float colorFactorB;
			float colorFactorA;
			VkBool32 useNormalTexture;
			VkBool32 useOcclusionTexture;
			VkBool32 useEmissiveTexture;
			float emissiveFactorR;
			float emissiveFactorG;
			float emissiveFactorB;
			VkBool32 use_pcf;
			float light_strength;
			float ambient_strength;
		} materialSpecializationData;

		materialSpecializationData.alphaMask = material.alphaMode == "MASK";
		materialSpecializationData.alphaMaskCutoff = material.alphaCutOff;
		materialSpecializationData.useMetalicTexture = material.hasMetalicRoughnessTexture;
		materialSpecializationData.metallicFactor = material.metallicFactor;
		materialSpecializationData.roughnessFactor = material.roughnessFactor;
		materialSpecializationData.aoFactor = material.aoFactor;
		materialSpecializationData.colorFactorR = material.baseColorFactor.r;
		materialSpecializationData.colorFactorG = material.baseColorFactor.g;
		materialSpecializationData.colorFactorB = material.baseColorFactor.b;
		materialSpecializationData.colorFactorA = material.baseColorFactor.a;
		materialSpecializationData.useNormalTexture = material.hasNormalTexture;
		materialSpecializationData.useOcclusionTexture = material.hasOcclusionTexture;
		materialSpecializationData.useEmissiveTexture = material.hasEmissiveTexture;
		materialSpecializationData.emissiveFactorR = material.emissiveFactor.r;
		materialSpecializationData.emissiveFactorG = material.emissiveFactor.g;
		materialSpecializationData.emissiveFactorB = material.emissiveFactor.b;
		materialSpecializationData.use_pcf = use_pcf;
		materialSpecializationData.light_strength = light_strength;
		materialSpecializationData.ambient_strength = ambient_strength;
		// std::cout << "Material: " << std::endl;
		// std::cout << "  alphaMask: " << materialSpecializationData.alphaMask << std::endl;
		// std::cout << "  alphaMaskCutoff: " << materialSpecializationData.alphaMaskCutoff << std::endl;
		// std::cout << "  useMetalicTexture: " << materialSpecializationData.useMetalicTexture << std::endl;
		// std::cout << "  metallicFactor: " << materialSpecializationData.metallicFactor << std::endl;
		// std::cout << "  roughnessFactor: " << materialSpecializationData.roughnessFactor << std::endl;
		// std::cout << "  aoFactor: " << materialSpecializationData.aoFactor << std::endl;
		// std::cout << "  colorFactorR: " << materialSpecializationData.colorFactorR << std::endl;
		// std::cout << "  colorFactorG: " << materialSpecializationData.colorFactorG << std::endl;
		// std::cout << "  colorFactorB: " << materialSpecializationData.colorFactorB << std::endl;
		// std::cout << "  colorFactorA: " << materialSpecializationData.colorFactorA << std::endl;
		// std::cout << "  useNormalTexture: " << materialSpecializationData.useNormalTexture << std::endl;
		// std::cout << "  useOcclusionTexture: " << materialSpecializationData.useOcclusionTexture << std::endl;
		// std::cout << "  useEmissiveTexture: " << materialSpecializationData.useEmissiveTexture << std::endl;
		// std::cout << "  emissiveFactorR: " << materialSpecializationData.emissiveFactorR << std::endl;
		// std::cout << "  emissiveFactorG: " << materialSpecializationData.emissiveFactorG << std::endl;	
		// std::cout << "  emissiveFactorB: " << materialSpecializationData.emissiveFactorB << std::endl;


		// POI: Constant fragment shader material parameters will be set using specialization constants
		std::vector<VkSpecializationMapEntry> specializationMapEntries = {
			vks::initializers::specializationMapEntry(0, offsetof(MaterialSpecializationData, alphaMask), sizeof(MaterialSpecializationData::alphaMask)),
			vks::initializers::specializationMapEntry(1, offsetof(MaterialSpecializationData, alphaMaskCutoff), sizeof(MaterialSpecializationData::alphaMaskCutoff)),
			vks::initializers::specializationMapEntry(2, offsetof(MaterialSpecializationData, useMetalicTexture), sizeof(MaterialSpecializationData::useMetalicTexture)),
			vks::initializers::specializationMapEntry(3, offsetof(MaterialSpecializationData, metallicFactor), sizeof(MaterialSpecializationData::metallicFactor)),
			vks::initializers::specializationMapEntry(4, offsetof(MaterialSpecializationData, roughnessFactor), sizeof(MaterialSpecializationData::roughnessFactor)),
			vks::initializers::specializationMapEntry(5, offsetof(MaterialSpecializationData, aoFactor), sizeof(MaterialSpecializationData::aoFactor)),
			vks::initializers::specializationMapEntry(6, offsetof(MaterialSpecializationData, colorFactorR), sizeof(MaterialSpecializationData::colorFactorR)),
			vks::initializers::specializationMapEntry(7, offsetof(MaterialSpecializationData, colorFactorG), sizeof(MaterialSpecializationData::colorFactorG)),
			vks::initializers::specializationMapEntry(8, offsetof(MaterialSpecializationData, colorFactorB), sizeof(MaterialSpecializationData::colorFactorB)),
			vks::initializers::specializationMapEntry(9, offsetof(MaterialSpecializationData, colorFactorA), sizeof(MaterialSpecializationData::colorFactorA)),
			vks::initializers::specializationMapEntry(10, offsetof(MaterialSpecializationData, useNormalTexture), sizeof(MaterialSpecializationData::useNormalTexture)),
			vks::initializers::specializationMapEntry(11, offsetof(MaterialSpecializationData, useOcclusionTexture), sizeof(MaterialSpecializationData::useOcclusionTexture)),
			vks::initializers::specializationMapEntry(12, offsetof(MaterialSpecializationData, useEmissiveTexture), sizeof(MaterialSpecializationData::useEmissiveTexture)),
			vks::initializers::specializationMapEntry(13, offsetof(MaterialSpecializationData, emissiveFactorR), sizeof(MaterialSpecializationData::emissiveFactorR)),
			vks::initializers::specializationMapEntry(14, offsetof(MaterialSpecializationData, emissiveFactorG), sizeof(MaterialSpecializationData::emissiveFactorG)),
			vks::initializers::specializationMapEntry(15, offsetof(MaterialSpecializationData, emissiveFactorB), sizeof(MaterialSpecializationData::emissiveFactorB)),
			vks::initializers::specializationMapEntry(16, offsetof(MaterialSpecializationData, MaterialSpecializationData::use_pcf), sizeof(MaterialSpecializationData::use_pcf)),
			vks::initializers::specializationMapEntry(17, offsetof(MaterialSpecializationData, MaterialSpecializationData::light_strength), sizeof(MaterialSpecializationData::light_strength)),
			vks::initializers::specializationMapEntry(18, offsetof(MaterialSpecializationData, MaterialSpecializationData::ambient_strength), sizeof(MaterialSpecializationData::ambient_strength)),
		};
		VkSpecializationInfo specializationInfo = vks::initializers::specializationInfo(specializationMapEntries, sizeof(materialSpecializationData), &materialSpecializationData);
		shaderStages[1].pSpecializationInfo = &specializationInfo;

		// For double sided materials, culling will be disabled
		// std::cout << "Creating pipeline for material " << material.doubleSided << std::endl;
		rasterizationStateCI.cullMode = material.doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &material.pipeline));
	}
	vkDestroyShaderModule(device, vertShaderModule, nullptr);
	vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

	// Prepare and initialize uniform buffer containing shader uniforms
void PBR::prepareUniformBuffers() {
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &shaderData.buffer, sizeof(shaderData.values)));
	VK_CHECK_RESULT(shaderData.buffer.map());

	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &lightDir.buffer, sizeof(lightDir.values)));
	VK_CHECK_RESULT(lightDir.buffer.map());

	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &offscreenData.buffer, sizeof(offscreenData.values)));
	VK_CHECK_RESULT(offscreenData.buffer.map());
}

void PBR::prepareOffscreenPipeline() {
	// std::cout << "Preparing offscreen pipeline" << std::endl;
	// Offscreen pipeline layout
	std::array<VkDescriptorSetLayout, 1> setLayouts = { descriptorSetLayoutOffscreen };
	VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));

	VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(OffscreenPC), 0);
	pipelineLayoutCI.pushConstantRangeCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayoutOffscreen));
	assert(pipelineLayoutOffscreen != VK_NULL_HANDLE);
	// std::cout << "Offscreen pipeline layout prepared" << std::endl;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	rasterizationStateCI.depthBiasEnable = VK_TRUE;
	// VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(0, VK_NULL_HANDLE);
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
	VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
	std::array<VkPipelineShaderStageCreateInfo, 1> shaderStages{};
	VkShaderModule vertShaderModule = createShaderModule(offscreen_vert);
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].pName = "main";
	shaderStages[0].module = vertShaderModule;
	// std::cout << "Offscreen pipeline shader stages" << std::endl;

	const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		vks::initializers::vertexInputBindingDescription(0, sizeof(VulkanglTFScene::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	};
	const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFScene::Vertex, pos)),
		// vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFScene::Vertex, normal)),
		// vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(VulkanglTFScene::Vertex, uv)),
		// vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFScene::Vertex, color)),
		// vks::initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VulkanglTFScene::Vertex, tangent)),
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo(vertexInputBindings, vertexInputAttributes);
	
	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayoutOffscreen, renderPassOffscreen, 0);
	pipelineCI.pVertexInputState = &vertexInputStateCI;
	pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
	pipelineCI.pRasterizationState = &rasterizationStateCI;
	pipelineCI.pColorBlendState = &colorBlendStateCI;
	pipelineCI.pMultisampleState = &multisampleStateCI;
	pipelineCI.pViewportState = &viewportStateCI;
	pipelineCI.pDepthStencilState = &depthStencilStateCI;
	pipelineCI.pDynamicState = &dynamicStateCI;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();

	// shaderStages[0].pNext = nullptr;

	pipelineCI.renderPass = renderPassOffscreen;
	// std::cout << "calling vkCreateGraphicsPipelines" << std::endl;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelineOffscreen));
	std::cout << "Offscreen pipeline prepared" << std::endl;
	vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void PBR::prepareOffscreenRenderpass() {
	// std::cout << "Preparing offscreen renderpass" << std::endl;
	VkAttachmentDescription attachmentDescription{};
	attachmentDescription.format = offscreenDepthFormat;
	attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;							// Clear depth at beginning of the render pass
	attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;						// We will read from depth, so it's important to store the depth attachment results
	attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;					// We don't care about initial layout of the attachment
	attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;// Attachment will be transitioned to shader read at render pass end

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 0;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;			// Attachment will be used as depth/stencil during render pass

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 0;													// No color attachments
	subpass.pDepthStencilAttachment = &depthReference;									// Reference to our depth attachment

	// Use subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo = vks::initializers::renderPassCreateInfo();
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &attachmentDescription;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassCreateInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPassOffscreen));
	std::cout << "Offscreen renderpass prepared" << std::endl;
}

void PBR::prepareOffscreenFramebuffer(int index)
{
	offscreenPass[index].width = shadowMapize;
	offscreenPass[index].height = shadowMapize;

	// For shadow mapping we only need a depth attachment
	VkImageCreateInfo image = vks::initializers::imageCreateInfo();
	image.imageType = VK_IMAGE_TYPE_2D;
	image.extent.width = offscreenPass[index].width;
	image.extent.height = offscreenPass[index].height;
	image.extent.depth = 1;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.format = offscreenDepthFormat;																// Depth stencil attachment
	image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;		// We will sample directly from the depth attachment for the shadow mapping
	VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &offscreenPass[index].depth.image));

	VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device, offscreenPass[index].depth.image, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offscreenPass[index].depth.mem));
	VK_CHECK_RESULT(vkBindImageMemory(device, offscreenPass[index].depth.image, offscreenPass[index].depth.mem, 0));

	VkImageViewCreateInfo depthStencilView = vks::initializers::imageViewCreateInfo();
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = offscreenDepthFormat;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;
	depthStencilView.image = offscreenPass[index].depth.image;
	VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &offscreenPass[index].depth.view));

	// Create sampler to sample from to depth attachment
	// Used to sample in the fragment shader for shadowed rendering
	VkFilter shadowmap_filter = vks::tools::formatIsFilterable(physicalDevice, offscreenDepthFormat, VK_IMAGE_TILING_OPTIMAL) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
	sampler.magFilter = shadowmap_filter;
	sampler.minFilter = shadowmap_filter;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &offscreenPass[index].depthSampler));

	// Create frame buffer
	VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
	fbufCreateInfo.renderPass = renderPassOffscreen;
	fbufCreateInfo.attachmentCount = 1;
	fbufCreateInfo.pAttachments = &offscreenPass[index].depth.view;
	fbufCreateInfo.width = offscreenPass[index].width;
	fbufCreateInfo.height = offscreenPass[index].height;
	fbufCreateInfo.layers = 1;

	VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offscreenPass[index].frameBuffer));
}

void PBR::buildOffscreenCommandBuffer(int index) {

	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	VkViewport viewport;
	VkRect2D scissor;

	clearValues[0].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = renderPassOffscreen;
	renderPassBeginInfo.framebuffer = offscreenPass[index].frameBuffer;
	renderPassBeginInfo.renderArea.extent.width = offscreenPass[index].width;
	renderPassBeginInfo.renderArea.extent.height = offscreenPass[index].height;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = clearValues;

	VK_CHECK_RESULT(vkBeginCommandBuffer(shadowCmdBuffer[index], &cmdBufInfo));

	vkCmdBeginRenderPass(shadowCmdBuffer[index], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	viewport = vks::initializers::viewport((float)offscreenPass[index].width, (float)offscreenPass[index].height, 0.0f, 1.0f);
	vkCmdSetViewport(shadowCmdBuffer[index], 0, 1, &viewport);

	scissor = vks::initializers::rect2D(offscreenPass[index].width, offscreenPass[index].height, 0, 0);
	vkCmdSetScissor(shadowCmdBuffer[index], 0, 1, &scissor);

	vkCmdSetDepthBias(
		shadowCmdBuffer[index],
		depthBiasConstant,
		0.0f,
		depthBiasSlope);
	
	assert(pipelineOffscreen != VK_NULL_HANDLE);
	vkCmdBindPipeline(shadowCmdBuffer[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineOffscreen);
	vkCmdBindDescriptorSets(shadowCmdBuffer[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayoutOffscreen, 0, 1, &descriptorSetOffscreen, 0, nullptr);
	// std::cout<<"puush constants size "<<sizeof(OffscreenPC)<<std::endl;
	OffscreenPC pushConstants;
	pushConstants.index = index;
	pushConstants.model = model_cust;
	vkCmdPushConstants(shadowCmdBuffer[index], pipelineLayoutOffscreen, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(OffscreenPC), &pushConstants);
	glTFScene.drawOffscreen(shadowCmdBuffer[index], pipelineLayoutOffscreen, model_cust);

	vkCmdEndRenderPass(shadowCmdBuffer[index]);
	// VK_CHECK_RESULT(vkEndCommandBuffer(shadowCmdBuffer[index]));
}

void PBR::updateUniformBuffers() {
	glm::mat4 clip_correct(1.0f);
	clip_correct[1][1] = -1.0f;
	shaderData.values.projection = clip_correct * proj_cust;
	shaderData.values.view = glm::transpose(view_cust);
	shaderData.values.viewPos = glm::vec4(cam_pos, 1.0f);
	for (int i = 0; i < 6; i++) 
		shaderData.values.lightSpaceMatrix[i] = offscreenData.values.depthMVP[i];
	memcpy(shaderData.buffer.mapped, &shaderData.values, sizeof(shaderData.values));
	

	memcpy(lightDir.buffer.mapped, &lightDir.values, sizeof(lightDir.values));
}

void PBR::prepare() {
	VulkanExampleBase::prepare();
	prepareUniformBuffers();
	loadAssets();
	if(use_shadow) {
		generateShadowMap();
	} else {
		std::cout << "Skipping shadow map generation" << std::endl;
	}
	// generateShadowMap();
	setupDescriptors();
	preparePipelines();
	buildCommandBuffers();
	prepared = true;
}

void PBR::generateShadowMap() {
	// std::cout << "Generating shadow map..." << std::endl;
	shadowCmdBuffer.resize(6);
	for (int i = 0; i < 6; i++) {
		shadowCmdBuffer[i] = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
	}
	setupDescriptorsOffscreen();
	prepareOffscreenRenderpass();
	prepareOffscreenPipeline();
	assert(pipelineLayoutOffscreen != VK_NULL_HANDLE);
	assert(pipelineOffscreen != VK_NULL_HANDLE);
	
	// Generate shadow map PV
	for (int i = 0; i < 6; i++) {
		// Set the view matrix for the light
		// glm::vec3 lightPos = glm::vec3(0.0) - glm::vec3(lightDir.values.lightdir[i]);
		// glm::mat4 view = glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		float aabb = 4 * glTFScene.getSceneDimensions(model_cust);
		// glm::vec3 aabbMin = glTFScene.dimensions.min;
		// glm::vec3 aabbMax = glTFScene.dimensions.max;
		// glm::vec3 corners[8] = {
		// 	{aabbMin.x, aabbMin.y, aabbMin.z},
		// 	{aabbMax.x, aabbMin.y, aabbMin.z},
		// 	{aabbMin.x, aabbMax.y, aabbMin.z},
		// 	{aabbMax.x, aabbMax.y, aabbMin.z},
		// 	{aabbMin.x, aabbMin.y, aabbMax.z},
		// 	{aabbMax.x, aabbMin.y, aabbMax.z},
		// 	{aabbMin.x, aabbMax.y, aabbMax.z},
		// 	{aabbMax.x, aabbMax.y, aabbMax.z}
    // };
		
		// std::cout << "AABB: " << aabb << std::endl;
		float orthoSize = aabb;
		float nearPlane = 0.1f;
		float farPlane = 2*orthoSize;
		glm::vec3 centre = glTFScene.dimensions.center;
		glm::vec3 lightPos = centre - glm::vec3(lightDir.values.lightdir[i])*orthoSize;
		glm::vec3 up = (abs(glm::dot(glm::vec3(lightDir.values.lightdir[i]), glm::vec3(0.0f, 1.0f, 0.0f))) > 0.99f)
    ? glm::vec3(0.0f, 0.0f, 1.0f)  // use Z up if Y is nearly parallel
    : glm::vec3(0.0f, 1.0f, 0.0f); // otherwise use Y up
		glm::mat4 view = glm::lookAt(lightPos, centre, up);
		// glm::vec3 minLS(FLT_MAX), maxLS(-FLT_MAX);
    // for (int i = 0; i < 8; ++i) {
		// 	glm::vec3 lightSpaceCorner = glm::vec3(view * glm::vec4(corners[i], 1.0f));
		// 	minLS = glm::min(minLS, lightSpaceCorner);
		// 	maxLS = glm::max(maxLS, lightSpaceCorner);
    // }
		// glm::mat4 proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 1000.0f);
		glm::mat4 proj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);
		// glm::mat4 proj = glm::ortho(minLS.x, maxLS.x, minLS.y, maxLS.y, minLS.z, maxLS.z);
		proj[1][1] *= -1;
		offscreenData.values.depthMVP[i] = proj * view;
		// memcpy(offscreenData.buffer.mapped, &mvp, sizeof(mvp));
	}
	memcpy(offscreenData.buffer.mapped, &offscreenData.values, sizeof(offscreenData.values));

	// std::cout << "building offscreen command buffers" << std::endl;
	for (int i = 0; i < 6; i++) {
		// Create framebuffer for shadow map
		prepareOffscreenFramebuffer(i);
		// Build command buffer for offscreen rendering
		buildOffscreenCommandBuffer(i);
	}
	std::cout << "building offscreen command buffers done" << std::endl;
	for (int i = 0; i < 6; i++) {
		vulkanDevice->flushCommandBuffer(shadowCmdBuffer[i], queue, true);
		vkQueueWaitIdle(queue);
		// std::stringstream ss;
		// ss << "shadowmap_" << i << ".png";
		// saveScreenshotOffscreen(ss.str(), i);
	}
	std::cout << "Shadow map generated!" << std::endl;
}


void PBR::render() {
	// while (!glfwWindowShouldClose(window)) {
		// glfwPollEvents();
		updateUniformBuffers();
		renderFrame();
		// buildCommandBuffers();

	// }
}

PBR::PBR() : VulkanExampleBase() {
	title = "PBR";

	camera.type = Camera::CameraType::firstperson;
	camera.movementSpeed = 4.0f;
	camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
	camera.rotationSpeed = 0.25f;

	camera.setRotation({ -7.75f, 150.25f, 0.0f });
	camera.setPosition({ 0.7f, 0.1f, 1.7f });
}

PBR::~PBR() {
	if (device) {
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorPool(device, descriptorPoolOffscreen, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayoutOffscreen, nullptr);
		vkDestroyPipeline(device, pipelineOffscreen, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayoutOffscreen, nullptr);
		vkDestroyRenderPass(device, renderPassOffscreen, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.matrices, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.textures, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.lights, nullptr); // Uncommented this line
		for (int i = 0; i < 6; i++) {
			vkDestroyFramebuffer(device, offscreenPass[i].frameBuffer, nullptr);
			vkDestroyImageView(device, offscreenPass[i].depth.view, nullptr);
			vkDestroyImage(device, offscreenPass[i].depth.image, nullptr);
			vkFreeMemory(device, offscreenPass[i].depth.mem, nullptr);
			vkDestroySampler(device, offscreenPass[i].depthSampler, nullptr);
		}
		shaderData.buffer.destroy();
		lightDir.buffer.destroy();
		offscreenData.buffer.destroy();
	}
}

void PBR::SetMatrices(const float* view, const float* proj, const float* model, const float* camPos) {
	view_cust = glm::make_mat4(view);
	proj_cust = glm::make_mat4(proj);
	model_cust = glm::make_mat4(model);
	glm::mat4 temp = glm::inverse(view_cust);
	glm::vec3 campos = glm::vec3(temp[0][3], temp[1][3], temp[2][3]);
	cam_pos = campos;
}
void PBR::SetModelPath(const std::string& model_p) {
	model_path = model_p;
}
void PBR::SetOutputPath(const std::string& output_p) {
	output_path = output_p;
	offScreen = true;
}

void PBR::run() {
	setupWindow();
	initVulkan();
	prepare();
	renderLoop();
}


// VULKAN_EXAMPLE_MAIN()
