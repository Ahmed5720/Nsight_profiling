#include "VulkanglTFModel.h"

VulkanglTFScene::~VulkanglTFScene()
{
	for (auto node : nodes) {
		delete node;
	}
	// Release all Vulkan resources allocated for the model
	vkDestroyBuffer(vulkanDevice->logicalDevice, vertices.buffer, nullptr);
	vkFreeMemory(vulkanDevice->logicalDevice, vertices.memory, nullptr);
	vkDestroyBuffer(vulkanDevice->logicalDevice, indices.buffer, nullptr);
	vkFreeMemory(vulkanDevice->logicalDevice, indices.memory, nullptr);
	for (Image image : images) {
		vkDestroyImageView(vulkanDevice->logicalDevice, image.texture.view, nullptr);
		vkDestroyImage(vulkanDevice->logicalDevice, image.texture.image, nullptr);
		vkDestroySampler(vulkanDevice->logicalDevice, image.texture.sampler, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, image.texture.deviceMemory, nullptr);
	}
	for (Material material : materials) {
		vkDestroyPipeline(vulkanDevice->logicalDevice, material.pipeline, nullptr);
	}
}

void VulkanglTFScene::Primitive::setDimensions(glm::vec3 min, glm::vec3 max)
{
	dimensions.min = min;
	dimensions.max = max;
	dimensions.size = max - min;
	dimensions.center = (min + max) / 2.0f;
	dimensions.radius = glm::distance(min, max) / 2.0f;
}

void VulkanglTFScene::getNodeDimensions(Node* node, glm::vec3& min, glm::vec3& max, glm::mat4 model_mat)
{
	if (node->hasMesh) {
		for (Primitive primitive : node->mesh.primitives) {
			glm::vec4 locMin = model_mat * glm::vec4(primitive.dimensions.min, 1.0f) ;
			glm::vec4 locMax = model_mat * glm::vec4(primitive.dimensions.max, 1.0f) ;
			if (locMin.x < min.x) { min.x = locMin.x; }
			if (locMin.y < min.y) { min.y = locMin.y; }
			if (locMin.z < min.z) { min.z = locMin.z; }
			if (locMax.x > max.x) { max.x = locMax.x; }
			if (locMax.y > max.y) { max.y = locMax.y; }
			if (locMax.z > max.z) { max.z = locMax.z; }
		}
	}
	for (auto child : node->children) {
		getNodeDimensions(child, min, max, model_mat);
	}
}
float VulkanglTFScene::getSceneDimensions(glm::mat4 model_mat)
{
	dimensions.min = glm::vec3(FLT_MAX);
	dimensions.max = glm::vec3(-FLT_MAX);
	for (auto node : nodes) {
		getNodeDimensions(node, dimensions.min, dimensions.max, model_mat);
	}
	// std::cout << "min: " << glm::to_string(dimensions.min) << std::endl;
	// std::cout << "max: " << glm::to_string(dimensions.max) << std::endl;
	dimensions.size = dimensions.max - dimensions.min;
	dimensions.center = (dimensions.min + dimensions.max) / 2.0f;
	// std::cout << "centre: " << glm::to_string(dimensions.center) << std::endl;
	dimensions.radius = glm::distance(dimensions.min, dimensions.max) / 2.0f;
	return dimensions.radius;
}

/*
	glTF loading functions

	The following functions take a glTF input model loaded via tinyglTF and convert all required data into our own structure
*/

void VulkanglTFScene::loadImages(tinygltf::Model& input)
{
	// POI: The textures for the glTF file used in this sample are stored as external ktx files, so we can directly load them from disk without the need for conversion
	images.resize(input.images.size());
	for (size_t i = 0; i < input.images.size(); i++) {
		tinygltf::Image& glTFImage = input.images[i];
		VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
		images[i].texture.loadFromFile(path + "/" + glTFImage.uri, format, vulkanDevice, copyQueue);
	}
	createEmptyTexture();
}

void VulkanglTFScene::createEmptyTexture()
{
	size_t bufferSize = 1 * 1 * 4;
	unsigned char* buffer = new unsigned char[bufferSize];
	memset(buffer, 0, bufferSize);
	Image emptyImage;
	emptyImage.texture.fromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_SRGB, 1, 1, vulkanDevice, copyQueue);
	images.push_back(emptyImage);
	delete[] buffer;
}

void VulkanglTFScene::loadTextures(tinygltf::Model& input)
{
	textures.resize(input.textures.size());
	for (size_t i = 0; i < input.textures.size(); i++) {
		textures[i].imageIndex = input.textures[i].source;
	}
}

void VulkanglTFScene::loadMaterials(tinygltf::Model& input)
{
	materials.resize(input.materials.size());
	for (size_t i = 0; i < input.materials.size(); i++) {
		// We only read the most basic properties required for our sample
		tinygltf::Material glTFMaterial = input.materials[i];
		tinygltf::PbrMetallicRoughness pbr = glTFMaterial.pbrMetallicRoughness;
		tinygltf::NormalTextureInfo normalTextureInfo = glTFMaterial.normalTexture;
		tinygltf::OcclusionTextureInfo occlusionTextureInfo = glTFMaterial.occlusionTexture;
		tinygltf::TextureInfo emissiveTextureInfo = glTFMaterial.emissiveTexture;
		tinygltf::TextureInfo baseColorTextureInfo = pbr.baseColorTexture;
		tinygltf::TextureInfo metallicRoughnessTextureInfo = pbr.metallicRoughnessTexture;
		materials[i].name = glTFMaterial.name;
		// Get the base color factor
		// if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
		// 	materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
		// }
		materials[i].baseColorFactor = glm::make_vec4(pbr.baseColorFactor.data());
		// Get base color texture index
		// if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
		// 	std::cout << "Base color texture found" << std::endl;
		// 	materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
		// } else {
		// 	materials[i].baseColorTextureIndex = images.size()-1;
		// }
		int factorIndex = baseColorTextureInfo.index;
		if (factorIndex >= 0) {
			std::cout << "Base color texture found" << std::endl;
			materials[i].baseColorTextureIndex = factorIndex;
		} else {
			materials[i].baseColorTextureIndex = images.size()-1;
		}
		// Get the normal map texture index
		// if (glTFMaterial.additionalValues.find("normalTexture") != glTFMaterial.additionalValues.end()) {
		// 	std::cout << "Normal texture found" << std::endl;
		// 	materials[i].normalTextureIndex = glTFMaterial.additionalValues["normalTexture"].TextureIndex();
		// 	materials[i].hasNormalTexture = true;
		// } else {
		// 	materials[i].normalTextureIndex = images.size()-1;
		// 	materials[i].hasNormalTexture = false;
		// }
		factorIndex = normalTextureInfo.index;
		if (factorIndex >= 0) {
			std::cout << "Normal texture found" << std::endl;
			materials[i].normalTextureIndex = factorIndex;
			materials[i].hasNormalTexture = true;
		} else {
			materials[i].normalTextureIndex = images.size()-1;
			materials[i].hasNormalTexture = false;
		}
		// Get the metallic roughness texture index
		// if (glTFMaterial.values.find("metallicRoughnessTexture") != glTFMaterial.values.end()) {
		// 	std::cout << "Metallic roughness texture found" << std::endl;
		// 	materials[i].metallicRoughnessTextureIndex = glTFMaterial.values["metallicRoughnessTexture"].TextureIndex();
		// 	materials[i].hasMetalicRoughnessTexture = true;
		// } else {
		// 	materials[i].metallicRoughnessTextureIndex = images.size()-1;
		// 	materials[i].hasMetalicRoughnessTexture = false;
		// }
		factorIndex = metallicRoughnessTextureInfo.index;
		if (factorIndex >= 0) {
			std::cout << "Metallic roughness texture found" << std::endl;
			materials[i].metallicRoughnessTextureIndex = factorIndex;
			materials[i].hasMetalicRoughnessTexture = true;
		} else {
			materials[i].metallicRoughnessTextureIndex = images.size()-1;
			materials[i].hasMetalicRoughnessTexture = false;
		}
		// Get the metallic factor
		// if (glTFMaterial.values.find("metallicFactor") != glTFMaterial.values.end()) {
		// 	materials[i].metallicFactor = (float)glTFMaterial.values["metallicFactor"].Factor();
		// }
		materials[i].metallicFactor = (float)pbr.metallicFactor;
		// Get the roughness factor
		// if (glTFMaterial.values.find("roughnessFactor") != glTFMaterial.values.end()) {
		// 	materials[i].roughnessFactor = (float)glTFMaterial.values["roughnessFactor"].Factor();
		// }
		materials[i].roughnessFactor = (float)pbr.roughnessFactor;
		// Get the occlusion factor
		// if (glTFMaterial.additionalValues.find("occlusionTexture") != glTFMaterial.additionalValues.end()) {
		// 	// materials[i].aoFactor = (float)glTFMaterial.additionalValues["occlusionTexture"].Factor();
		// 	materials[i].hasOcclusionTexture = true;
		// 	materials[i].aoFactor = (float)glTFMaterial.additionalValues["occlusionTexture"].TextureStrength();
		// } else {
		// 	materials[i].hasOcclusionTexture = false;
		// 	materials[i].aoFactor = 1.0f;
		// }
		factorIndex = occlusionTextureInfo.index;
		if (factorIndex >= 0) {
			std::cout << "Occlusion texture found" << std::endl;
			// materials[i].occlusionTextureIndex = factorIndex;
			materials[i].hasOcclusionTexture = true;
			materials[i].aoFactor = (float)occlusionTextureInfo.strength;
		} else {
			// materials[i].occlusionTextureIndex = images.size()-1;
			materials[i].hasOcclusionTexture = false;
			materials[i].aoFactor = (float)occlusionTextureInfo.strength;
		}
		// Get the emissive factor
		// if (glTFMaterial.additionalValues.find("emissiveFactor") != glTFMaterial.additionalValues.end()) {
		// 	materials[i].emissiveFactor = glm::make_vec4(glTFMaterial.additionalValues["emissiveFactor"].ColorFactor().data());
		// }
		materials[i].emissiveFactor = glm::make_vec3(glTFMaterial.emissiveFactor.data());
		// Get the emissive texture index
		// if (glTFMaterial.additionalValues.find("emissiveTexture") != glTFMaterial.additionalValues.end()) {
		// 	materials[i].emissiveTextureIndex = glTFMaterial.additionalValues["emissiveTexture"].TextureIndex();
		// 	materials[i].hasEmissiveTexture = true;
		// } else {
		// 	materials[i].emissiveTextureIndex = images.size()-1;
		// 	materials[i].hasEmissiveTexture = false;
		// }
		factorIndex = emissiveTextureInfo.index;
		if (factorIndex >= 0) {
			std::cout << "Emissive texture found" << std::endl;
			materials[i].emissiveTextureIndex = factorIndex;
			materials[i].hasEmissiveTexture = true;
		} else {
			materials[i].emissiveTextureIndex = images.size()-1;
			materials[i].hasEmissiveTexture = false;
		}
		// Get some additional material parameters that are used in this sample
		materials[i].alphaMode = glTFMaterial.alphaMode;
		materials[i].alphaCutOff = (float)glTFMaterial.alphaCutoff;
		materials[i].doubleSided = glTFMaterial.doubleSided;
	}
}

void VulkanglTFScene::loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, VulkanglTFScene::Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<VulkanglTFScene::Vertex>& vertexBuffer)
{
	VulkanglTFScene::Node* node = new VulkanglTFScene::Node{};
	node->name = inputNode.name;
	node->parent = parent;
	
	// Get the local node matrix
	// It's either made up from translation, rotation, scale or a 4x4 matrix
	node->matrix = glm::mat4(1.0f);
	if (inputNode.translation.size() == 3) {
		node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
	}
	if (inputNode.rotation.size() == 4) {
		glm::quat q = glm::make_quat(inputNode.rotation.data());
		node->matrix *= glm::mat4(q);
	}
	if (inputNode.scale.size() == 3) {
		node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
	}
	if (inputNode.matrix.size() == 16) {
		node->matrix = glm::make_mat4x4(inputNode.matrix.data());
	};

	// Load node's children
	if (inputNode.children.size() > 0) {
		for (size_t i = 0; i < inputNode.children.size(); i++) {
			loadNode(input.nodes[inputNode.children[i]], input, node, indexBuffer, vertexBuffer);
		}
	}

	// If the node contains mesh data, we load vertices and indices from the buffers
	// In glTF this is done via accessors and buffer views
	if (inputNode.mesh > -1) {
		const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
		node->hasMesh = true;
		// Iterate through all primitives of this node's mesh
		for (size_t i = 0; i < mesh.primitives.size(); i++) {
			const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
			uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
			uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
			uint32_t indexCount = 0;
			glm::vec3 posMin{};
			glm::vec3 posMax{};
			// Vertices
			{
				const float* positionBuffer = nullptr;
				const float* normalsBuffer = nullptr;
				const float* texCoordsBuffer = nullptr;
				const float* tangentsBuffer = nullptr;
				size_t vertexCount = 0;

				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					posMin = glm::vec3(accessor.minValues[0], accessor.minValues[1], accessor.minValues[2]);
					// glm::vec4 posMin4 = model_cust * glm::vec4(posMin, 1.0f);
					// posMin = glm::vec3(posMin4);
					posMax = glm::vec3(accessor.maxValues[0], accessor.maxValues[1], accessor.maxValues[2]);
					// glm::vec4 posMax4 = model_cust * glm::vec4(posMax, 1.0f);
					// posMax = glm::vec3(posMax4);
					vertexCount = accessor.count;
				}
				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// Get buffer data for vertex texture coordinates
				// glTF supports multiple sets, we only load the first one
				if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// POI: This sample uses normal mapping, so we also need to load the tangents from the glTF file
				if (glTFPrimitive.attributes.find("TANGENT") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TANGENT")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					tangentsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// Append data to model's vertex buffer
				for (size_t v = 0; v < vertexCount; v++) {
					Vertex vert{};
					vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
					vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
					vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
					vert.color = glm::vec3(1.0f);
					vert.tangent = tangentsBuffer ? glm::make_vec4(&tangentsBuffer[v * 4]) : glm::vec4(0.0f);
 					vertexBuffer.push_back(vert);
				}
			}
			// Indices
			{
				const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
				const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

				indexCount += static_cast<uint32_t>(accessor.count);

				// glTF supports different component types of indices
				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				default:
					std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
					return;
				}
			}
			Primitive primitive{};
			primitive.firstIndex = firstIndex;
			primitive.indexCount = indexCount;
			primitive.materialIndex = glTFPrimitive.material;
			// std::cout << "posMin " << glm::to_string(posMin) << std::endl;
			// std::cout << "posMax " << glm::to_string(posMax) << std::endl;
			primitive.setDimensions(posMin, posMax);
			node->mesh.primitives.push_back(primitive);
		}
	}

	if (parent) {
		parent->children.push_back(node);
	}
	else {
		nodes.push_back(node);
	}
}

VkDescriptorImageInfo VulkanglTFScene::getTextureDescriptor(const size_t index)
{
	return images[index].texture.descriptor;
}

/*
	glTF rendering functions
*/

// Draw a single node including child nodes (if present)
void VulkanglTFScene::drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VulkanglTFScene::Node* node,glm::mat4 model_cust)
{
	if (!node->visible) {
		return;
	}
	if (node->mesh.primitives.size() > 0) {
		glm::mat4 nodeMatrix = model_cust;
		VulkanglTFScene::Node* currentParent = node->parent;
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
		for (VulkanglTFScene::Primitive& primitive : node->mesh.primitives) {
			if (primitive.indexCount > 0) {
				// std::cout << "Index count: " << primitive.indexCount << std::endl;
				VulkanglTFScene::Material& material = materials[primitive.materialIndex];
				// POI: Bind the pipeline for the node's material
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material.descriptorSet, 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
			}
		}
	}
	for (auto& child : node->children) {
		drawNode(commandBuffer, pipelineLayout, child, model_cust);
	}
}

// Draw the glTF scene starting at the top-level-nodes
void VulkanglTFScene::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, glm::mat4 model_cust)
{
	// All vertices and indices are stored in single buffers, so we only need to bind once
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	// Render all nodes at top-level
	for (auto& node : nodes) {
		drawNode(commandBuffer, pipelineLayout, node, model_cust);
	}
}

void VulkanglTFScene::drawOffscreen(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, glm::mat4 model_cust)
{
	// All vertices and indices are stored in single buffers, so we only need to bind once
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	// Render all nodes at top-level
	for (auto& node : nodes) {
		drawNodeOffscreen(commandBuffer, pipelineLayout, node, model_cust);
	}
}

void VulkanglTFScene::drawNodeOffscreen(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VulkanglTFScene::Node* node, glm::mat4 model_cust)
{
	if (!node->visible) {
		return;
	}
	if (node->mesh.primitives.size() > 0) {
		glm::mat4 nodeMatrix = model_cust;
		VulkanglTFScene::Node* currentParent = node->parent;
		// vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
		for (VulkanglTFScene::Primitive& primitive : node->mesh.primitives) {
			if (primitive.indexCount > 0) {
				VulkanglTFScene::Material& material = materials[primitive.materialIndex];
				// vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
				// vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material.descriptorSet, 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
			}
		}
	}
	for (auto& child : node->children) {
		drawNodeOffscreen(commandBuffer, pipelineLayout, child, model_cust);
	}
}