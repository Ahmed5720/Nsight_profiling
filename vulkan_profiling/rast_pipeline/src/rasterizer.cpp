#include <vector>
#include <iostream>
#include <string>
#include <argparse/argparse.hpp>
#include <rast.h>

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
    0.1, 0, 0, 0, 
    0, 0.1, 0, 0, 
    0, 0, 0.1, 0, 
    0, 0, 0, 1 
  };
  // std::vector<float> model(model_def, model_def + 16);
  argparse::ArgumentParser parser("rast");
  parser.add_argument("-i", "--input1").help("input model 1 path.");
  parser.add_argument("-o", "--output").help("output image path.");
  // parser.add_argument("-i2", "--input2").help("input model 2 path.");
  parser.add_argument("-v", "--view").nargs(16).help("View Matrix").scan<'g', float>().default_value(view_def);
  parser.add_argument("-p", "--proj").nargs(16).help("Projection Matrix").scan<'g', float>().default_value(proj_def);
  parser.add_argument("-m", "--model").nargs(16).help("Model Matrix").scan<'g', float>().default_value(model_def);
  try {
    parser.parse_args(argc, argv);
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << parser;
    return 1;
  }

	try {
		Rasterizer app;
		if(parser.is_used("input1")) {
			auto model_p_1 = parser.get<std::string>("input1");
      // auto model_p_2 = parser.get<std::string>("input2");
      std::cout << "model path: " << model_p_1 << std::endl;
			app.SetModelPath(model_p_1);
		}
    if(parser.is_used("output")) {
      auto output_p = parser.get<std::string>("output");
      std::cout << "output path: " << output_p << std::endl;
      app.SetOutputPath(output_p);
    }
		std::cout<<"reading matrices into vectors"<<std::endl;
		std::vector<float> view = parser.get<std::vector<float>>("view");
		std::vector<float> proj = parser.get<std::vector<float>>("proj");
		std::vector<float> model = parser.get<std::vector<float>>("model");
    app.SetMatrices(view.data(), proj.data(), model.data());
		app.run();
	} catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
		