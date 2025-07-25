/*
 * Copyright (c) 2024, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2014-2024 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/*

This class is used to hold the scene data and the scene graph.

The scene is:
- glTF scene, and the corresponding Vulkan scene (gltfSceneVk), and the acceleration structure (gltfSceneRtx)
- HDR environment
- Skydome

The scene graph is used to show the hierarchy of the scene in the UI.





*/


#include <string>
#include <bitset>
#include <glm/glm.hpp>

// Device/host structures for the scene
#include "nvvkhl/shaders/dh_lighting.h"
// namespace DH {
// #include "shaders/device_host.h"  // Include the device/host structures
// }

// nvpro-core
#include "nvh/gltfscene.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvkhl/gltf_scene_rtx.hpp"
#include "nvvkhl/gltf_scene_vk.hpp"
#include "nvvkhl/hdr_env.hpp"
#include "nvvkhl/hdr_env_dome.hpp"
#include "nvvkhl/sky.hpp"

// Local to application
// #include "animation_control.hpp"
// #include "resources.hpp"
// #include "scene_graph_ui.hpp"
// #include "settings.hpp"


namespace gltfr {

// Scene with glTF model
class Scene
{
public:
  // Initialization and deinitialization
  void init(nvh::gltf::Scene* gltfScene, nvvkhl::SceneVk* gltfSceneVk, nvvk::ResourceAllocator* resAlloc);
  void deinit();

  // Loading and saving
  bool load(Resources& resources, const std::string& filename);
  bool save(const std::string& filename) const;

  // Validation and state checks
  bool isValid() const { return (m_gltfScene != nullptr) && m_gltfScene->valid(); }
  bool hasDirtyFlag(int flag) const { return m_dirtyFlags.test(flag); }
  void setDirtyFlag(int flag, bool changed) { changed ? m_dirtyFlags.set(flag) : m_dirtyFlags.reset(flag); }

  // Frame processing
  void resetFrameCount();

  nvh::Bbox getRenderNodeBbox(int node) const;

  bool processFrame(VkCommandBuffer cmdBuf, Settings& settings);
  bool onUI(Resources& resources, Settings& settings, GLFWwindow* winHandle);

  // Scene manipulation
  void fitSceneToView() const;
  void fitObjectToView() const;
  void selectRenderNode(int renderNodeIndex);
  int  getSelectedRenderNode() const { return m_selectedRenderNode; }

  // File information
  std::string getFilename() const;

  // Utility
  void recreateTangents(bool mikktspace);
  void generateHdrMipmap(VkCommandBuffer cmd, Resources& res);


  nvh::gltf::Scene*        m_gltfScene{};     // The glTF scene
  // std::unique_ptr<nvvkhl::SceneRtx>        m_gltfSceneRtx{};  // The Vulkan scene with RTX acceleration structures
  nvvkhl::SceneVk*         m_gltfSceneVk{};   // The Vulkan scene
  // std::unique_ptr<nvvkhl::HdrEnv>          m_hdrEnv{};        // The HDR environment
  // std::unique_ptr<nvvkhl::HdrEnvDome>      m_hdrDome{};       // The HDR environment dome (raster)
  // std::unique_ptr<nvvkhl::PhysicalSkyDome> m_sky{};           // The sky dome

  VkDescriptorPool      m_descriptorPool{};  // Scene descriptor pool, frame info + scene buffer address + textures
  VkDescriptorSetLayout m_sceneDescriptorSetLayout{};  // Descriptor set layout for the scene
  VkDescriptorSet m_sceneDescriptorSet{};  // Application descriptor set (storing all textures + scene and frame info)

  // DH::SceneFrameInfo m_sceneFrameInfo{};      // Used to pass the scene information to the shaders
  nvvk::Buffer       m_sceneFrameInfoBuffer;  // Buffer for the scene information

private:
  // Scene creation
  void createVulkanScene(Resources& resources);
  // void createHdr(Resources& resources, const std::string& filename);
  void postSceneCreateProcess(Resources& resources, const std::string& filename);

  // Frame update
  // bool updateFrameCount(Settings& settings);

  // Descriptor set management
  void createDescriptorPool(VkDevice device);
  void createDescriptorSet(VkDevice device);
  void destroyDescriptorSet(VkDevice device);
  void writeDescriptorSet(Resources& resources) const;


  // std::bitset<32>              m_dirtyFlags;               // Flags to indicate what has changed
  // AnimationControl             m_animControl;              // Animation control (UI)
  // std::unique_ptr<GltfModelUI> m_sceneGraph;               // Scene graph (UI)
  // std::string                  m_hdrFilename;              // Keep track of HDR filename
  int                          m_selectedRenderNode = -1;  // Selected render node
  nvvk::ResourceAllocator* m_alloc = nullptr;  // Resource allocator for Vulkan resources


public:
  enum DirtyFlags
  {
    eNewScene,          // When a new scene is loaded, same for multiple scenes
    eVulkanScene,       // When the Vulkan geometry buffers need to be updated
    eVulkanMaterial,    // When the Vulkan material buffers need to be updated
    eVulkanAttributes,  // When the Vulkan attributes need to be updated
    eRtxScene,          // When the RTX acceleration structures need to be updated
    eHdrEnv,            // When the HDR environment needs to be updated
    eNodeVisibility,    // When the node visibility has changed

    eNumDirtyFlags  // Keep last - Number of dirty flags
  };
};

}  // namespace gltfr