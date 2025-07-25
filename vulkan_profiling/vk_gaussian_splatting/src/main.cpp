/*
 * Copyright (c) 2023-2025, NVIDIA CORPORATION.  All rights reserved.
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
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025, NVIDIA CORPORATION.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gaussian_splatting.h>

// create, setup and run an nvvkhl::Application
// with a GaussianSplatting element.
int main(int argc, char** argv)
{
  // Vulkan creation context information (see nvvk::Context)
  static VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR baryFeaturesKHR = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR};
  static VkPhysicalDeviceMeshShaderFeaturesEXT meshFeaturesEXT = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
  nvvk::ContextCreateInfo vkSetup;
  vkSetup.setVersion(1, 3);
  vkSetup.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  vkSetup.addDeviceExtension(VK_EXT_MESH_SHADER_EXTENSION_NAME, false, &meshFeaturesEXT);
  vkSetup.addDeviceExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME, false, &baryFeaturesKHR);
  vkSetup.addDeviceExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);  // for ImGui
  vkSetup.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  nvvkhl::addSurfaceExtensions(vkSetup.instanceExtensions);

  // from meshlettest.cpp sample
  vkSetup.fnDisableFeatures = [](VkStructureType sType, void* pFeatureStruct) {
    if(sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT)
    {
      auto* feature = reinterpret_cast<VkPhysicalDeviceMeshShaderFeaturesEXT*>(pFeatureStruct);
      // enabling and not using it may cost a tiny bit of performance on NV hardware
      feature->meshShaderQueries = VK_FALSE;
      // disable for the time beeing
      feature->primitiveFragmentShadingRateMeshShader = VK_FALSE;
    }
  };

  // Create Vulkan context
  nvvk::Context vkContext;
  vkContext.init(vkSetup);

  auto parser = std::make_shared<argparse::ArgumentParser>();
  parser->add_description("Gaussian Splatting");
  parser->add_argument("-i1", "--input1").help("Input ply file to load").default_value("/home/nisarg/data/amber/point_cloud/iteration_30000/point_cloud.ply");
  parser->add_argument("-i2", "--input2").help("Input gltf file to load").default_value("/home/nisarg/data/amber/scene.gltf");
  parser->add_argument("-o", "--output").help("output image path.");
  std::vector<float> view_def = {
    0.707107, -0.5, 0.5, 0, 
    0, 0.707107, 0.707107, 0, 
    -0.707107, -0.5, 0.5, 0, 
    -0, -0, -2, 1 
  };
  std::vector<float> proj_def = {
    0.974705f, 0.0f, 0.0f, 0.0f,
      0.0f, -1.73205f, 0.0f, 0.0f,
      0.0f, 0.0f, -1.0001f, -1.0f,
      0.0f, 0.0f, -0.010001f, 0.0f
  };

  parser->add_argument("-v", "--view").nargs(16).help("View matrix").scan<'g', float>().default_value(view_def);
  parser->add_argument("-p", "--proj").nargs(16).help("Projection matrix").scan<'g', float>().default_value(proj_def);
  try {
    parser->parse_args(argc, argv);
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << parser;
    return 1;
  }
  bool headless = false;
  // std::string output;

  // if(parser->is_used("output")) {
  //   // output = parser->get<std::string>("--output");
  //   headless = true;
  // }

  // Application setup
  nvvkhl::ApplicationCreateInfo appSetup;
  appSetup.name                  = fmt::format("{}", PROJECT_NAME);
  appSetup.vSync                 = false;
  appSetup.useMenu               = false;
  appSetup.windowSize            = {1600, 900};
  appSetup.hasUndockableViewport = false;
  appSetup.instance              = vkContext.m_instance;
  appSetup.device                = vkContext.m_device;
  appSetup.physicalDevice        = vkContext.m_physicalDevice;
  appSetup.queues.push_back({vkContext.m_queueGCT.familyIndex, vkContext.m_queueGCT.queueIndex, vkContext.m_queueGCT.queue});
  appSetup.headless = headless;
  // appSetup.headlessFrameCount = 10;

  // Setting up the layout of the application
  // appSetup.dockSetup = [](ImGuiID viewportID) {
    // right side panel container
    // ImGuiID settingID = ImGui::DockBuilderSplitNode(viewportID, ImGuiDir_Right, 0.25F, nullptr, &viewportID);
    // ImGui::DockBuilderDockWindow("Settings", settingID);
    // ImGui::DockBuilderDockWindow("Misc", settingID);

    // bottom panel container
    // ImGuiID memoryID = ImGui::DockBuilderSplitNode(viewportID, ImGuiDir_Down, 0.35F, nullptr, &viewportID);
    // ImGui::DockBuilderDockWindow("Memory Statistics", memoryID);
    // ImGuiID profilerID = ImGui::DockBuilderSplitNode(memoryID, ImGuiDir_Right, 0.33F, nullptr, &memoryID);
    // ImGui::DockBuilderDockWindow("Profiler", profilerID);
  // };

  // Create the application
  auto app = std::make_unique<nvvkhl::Application>(appSetup);
  if(!headless) {
    GLFWwindow* window = app->getWindowHandle();
    if(window) {
      glfwSetWindowSize(window, appSetup.windowSize.x, appSetup.windowSize.y);
    }
  }


  // create the profiler element
  // auto profiler = std::make_shared<nvvkhl::ElementProfiler>(true);
  // Create the benchmarking framework
  // auto benchmark = std::make_shared<nvvkhl::ElementBenchmarkParameters>(argc, argv);
  // benchmark->setProfiler(profiler);
  // create the core of the sample

  auto gaussianSplatting = std::make_shared<GaussianSplatting>(nullptr, parser);

  // Add all application elements including our sample specific gaussianSplatting
  app->addElement(gaussianSplatting);
  // app->addElement(benchmark);
  // app->addElement(std::make_shared<nvvkhl::ElementCamera>());
  app->addElement(std::make_shared<nvvkhl::ElementDefaultWindowTitle>("", fmt::format("({})", "GLSL")));  // Window title info//
  // app->addElement(profiler);
  // app->addElement(std::make_shared<nvvkhl::ElementNvml>());
  //
  gaussianSplatting->registerRecentFilesHandler();
  app->run();

  app.reset();

  // return benchmark->errorCode();
  return 0;
}
