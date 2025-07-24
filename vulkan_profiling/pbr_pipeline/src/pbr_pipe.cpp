#include <vector>
#include <iostream>
#include <string>
#include <argparse/argparse.hpp>
#include <pbr.h>


int main(int argc, char** argv) {
  std::vector<float> view_def = {
    0.707107, -0.5, 0.5, 0, 
    0, 0.707107, 0.707107, 0, 
    -0.707107, -0.5, 0.5, 0, 
    -0, -0, -2, 1 
  };
  // std::vector<float> view(view_def, view_def + 16);
  std::vector<float> proj_def = {
      0.974705f, 0.0f, 0.0f, 0.0f,
        0.0f, -1.73205f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0001f, -1.0f,
        0.0f, 0.0f, -0.010001f, 0.0f
  };
  // std::vector<float> proj(proj_def, proj_def + 16);
  std::vector<float> model_def = {
    1, 0, 0, 0, 
    0, 1, 0, 0, 
    0, 0, 1, 0, 
    0, 0, 0, 1 
  };

  std::vector<float> cam_def = {
    0.7f, 0.1f, 1.7f
  };
  // std::vector<float> model(model_def, model_def + 16);
  argparse::ArgumentParser parser("PBR");
  parser.add_argument("-i", "--input").help("input model 1 path.");
  parser.add_argument("-o", "--output").help("output image path.");
  // parser.add_argument("-i2", "--input2").help("input model 2 path.");
  parser.add_argument("-v", "--view").nargs(16).help("View Matrix").scan<'g', float>().default_value(view_def);
  parser.add_argument("-p", "--proj").nargs(16).help("Projection Matrix").scan<'g', float>().default_value(proj_def);
  parser.add_argument("-m", "--model").nargs(16).help("Model Matrix").scan<'g', float>().default_value(model_def);
  parser.add_argument("-c", "--camera").nargs(3).help("Camera Position").scan<'g', float>().default_value(cam_def);
  parser.add_argument("-S", "--shadow").default_value(false).implicit_value(true).help("Enable shadow mapping.");
  parser.add_argument("-P", "--pcf").default_value(false).implicit_value(true).help("Enable PCF shadow mapping.");
  parser.add_argument("-L", "--light").default_value(float(3.0)).help("Light Strength").scan<'g', float>();
  parser.add_argument("-A", "--ambient").default_value(float(0.01)).help("Ambient Light Strength").scan<'g', float>();
  try {
    std::cout << "Parsing arguments..." << std::endl;
    parser.parse_args(argc, argv);
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << parser;
    return 1;
  }
  
  PBR pbr_pipe;
  if(parser.is_used("input")) {
    auto model_p_1 = parser.get<std::string>("input");
    // auto model_p_2 = parser.get<std::string>("input2");
    std::cout << "model path: " << model_p_1 << std::endl;
    pbr_pipe.SetModelPath(model_p_1);
  }
  if(parser.is_used("output")) {
    auto output_p = parser.get<std::string>("output");
    std::cout << "output path: " << output_p << std::endl;
    pbr_pipe.SetOutputPath(output_p);
  }
  if(parser.is_used("view")) {
    view_def = parser.get<std::vector<float>>("view");
  }
  if(parser.is_used("proj")) {
    proj_def = parser.get<std::vector<float>>("proj");
  }
  if(parser.is_used("model")) {
    model_def = parser.get<std::vector<float>>("model");
  }
  if(parser.is_used("camera")) {
    cam_def = parser.get<std::vector<float>>("camera");
  }
  bool use_shadow = parser.get<bool>("shadow");
  bool use_pcf = parser.get<bool>("pcf");
  float light_strength = parser.get<float>("light");
  float ambient_strength = parser.get<float>("ambient");
  pbr_pipe.SetMatrices(view_def.data(), proj_def.data(), model_def.data(), cam_def.data());
  pbr_pipe.SetUseShadow(use_shadow, use_pcf);
  std::cout <<"Setting light strength to " << light_strength << " and ambient strength to " << ambient_strength << std::endl;
  pbr_pipe.SetLightStrength(light_strength, ambient_strength);
  pbr_pipe.run();

	return 0;
}